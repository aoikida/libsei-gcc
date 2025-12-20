# SIGSEGV Recovery 機能設計書

## 概要

SIGSEGV Recovery は、トランザクション実行中に発生するセグメンテーションフォールトから自動的に回復する機能です。ROLLBACKフラグの一部として統合されており、SDC（Silent Data Corruption）リカバリと同じインフラストラクチャを使用します。

## 設計目標

1. トランザクション内でSIGSEGVが発生しても、プロセスを継続する
2. アプリケーションコードの変更を不要にする（透過的なリカバリ）
3. 障害コアをブラックリストに追加し、別コアで再実行する
4. 全コアが使用不可になった場合のみプロセスを終了する

## アーキテクチャ

### コンポーネント構成

```
┌─────────────────────────────────────────────────────────────┐
│                    アプリケーション                           │
│                  (__begin/__end で囲まれたトランザクション)      │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      libsei (tmi.c)                         │
│  ┌─────────────┐  ┌─────────────┐  ┌──────────────────┐    │
│  │ __sei_begin │  │ __sei_end   │  │ __sei_commit     │    │
│  │ ctx保存     │  │ コミット処理 │  │ SDCリカバリループ │    │
│  └─────────────┘  └─────────────┘  └──────────────────┘    │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    protect.c                                │
│  ┌──────────────────┐  ┌────────────────────────────────┐  │
│  │ protect_setsignal│  │ protect_handler (SIGSEGV)      │  │
│  │ シグナルハンドラ  │  │ - コアブラックリスト            │  │
│  │ 登録              │  │ - sei_rollback                 │  │
│  │                   │  │ - __sei_switch (再実行)        │  │
│  └──────────────────┘  └────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                  cpu_isolation.c                            │
│  ┌────────────────────────┐  ┌────────────────────────┐    │
│  │ cpu_isolation_blacklist│  │ cpu_isolation_migrate  │    │
│  │ _current               │  │ _current_thread        │    │
│  └────────────────────────┘  └────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

### ビルドフラグ

| フラグ | 説明 |
|--------|------|
| `ROLLBACK=1` | SDCリカバリ + SIGSEGVリカバリを有効化 |
| `FAULT_INJECT=1` | テスト用フォールトインジェクション |

Makefileでの設定:
```makefile
ifdef ROLLBACK
AFLAGS += -DSEI_CPU_ISOLATION -DSEI_SIGSEGV_RECOVERY
endif
```

## 実装詳細

### 1. シグナルハンドラの初期化

**ファイル**: `src/protect.c`

```c
void protect_setsignal() {
#ifdef SEI_SIGSEGV_RECOVERY
    // 代替スタックの設定
    // SIGSEGVでスタックが破損しても安全に動作するため
    #define SEI_ALTSTACK_SIZE (64 * 1024)
    static __thread char altstack[SEI_ALTSTACK_SIZE];
    stack_t ss;
    ss.ss_sp = altstack;
    ss.ss_size = SEI_ALTSTACK_SIZE;
    ss.ss_flags = 0;
    sigaltstack(&ss, NULL);
#endif

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
#ifdef SEI_SIGSEGV_RECOVERY
    sa.sa_flags |= SA_ONSTACK;  // 代替スタックを使用
#endif
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = protect_handler;
    sigaction(SIGSEGV, &sa, NULL);
}
```

**呼び出しタイミング**:
- シングルスレッド: `__sei_init()` (tmi.c:220)
- マルチスレッド: `__sei_thread_init()` (tmi.c:302)

### 2. SIGSEGVハンドラ

**ファイル**: `src/protect.c`

```c
#define PROTECT_HANDLER                                                 \
    void protect_handler(int sig, siginfo_t* si, void* args)            \
    {                                                                   \
        /* 1. トランザクション外: リカバリ不可 */                          \
        if (!__sei_thread || !__sei_thread->sei ||                      \
            sei_getp(__sei_thread->sei) < 0) {                          \
            signal(SIGSEGV, SIG_DFL);                                   \
            raise(SIGSEGV);                                             \
            return;                                                     \
        }                                                               \
                                                                        \
        /* 2. コアをブラックリストに追加して移行 */                        \
        sei_setp(__sei_thread->sei, -1);                                \
        cpu_isolation_blacklist_current();                              \
        cpu_isolation_migrate_current_thread();                         \
                                                                        \
        /* 3. トランザクション状態をロールバック */                        \
        sei_rollback(__sei_thread->sei);                                \
                                                                        \
        /* 4. Phase 0から再実行 */                                       \
        __sei_switch(&__sei_thread->ctx, 0x00);                         \
    }
```

### 3. リカバリ処理フロー

```
┌────────────────────────────────────────────────────────────────┐
│ トランザクション開始 (__begin)                                   │
│   - sei_ctx_t ctx にCPUレジスタ状態を保存                        │
│   - __sei_switch() の戻り点を設定                               │
└────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌────────────────────────────────────────────────────────────────┐
│ トランザクション実行中                                           │
│   - メモリ読み書き（COWバッファに記録）                           │
│   - 出力メッセージ作成                                           │
└────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌────────────────────────────────────────────────────────────────┐
│ ★ SIGSEGV発生！                                                │
│   - NULLポインタアクセス                                         │
│   - 不正メモリアクセス                                           │
│   - ハードウェア障害                                             │
└────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌────────────────────────────────────────────────────────────────┐
│ protect_handler() 呼び出し                                      │
│                                                                │
│ (1) トランザクション内かチェック                                  │
│     └─ sei_getp() >= 0 → トランザクション内                      │
│     └─ sei_getp() < 0  → トランザクション外 → 通常クラッシュ       │
│                                                                │
│ (2) sei_setp(-1): トランザクション外に設定                        │
│     └─ これ以降のlibsei関数呼び出しで状態が壊れないようにする       │
│                                                                │
│ (3) cpu_isolation_blacklist_current()                          │
│     └─ 現在のコアIDをブラックリストビットマスクに追加              │
│                                                                │
│ (4) cpu_isolation_migrate_current_thread()                     │
│     └─ 利用可能なコアを探して移行                                │
│     └─ 全コアがブラックリストされていれば exit(1)                 │
│                                                                │
│ (5) sei_rollback(): メモリ状態を復元                             │
│     └─ COWバッファの変更を破棄                                   │
│     └─ abuf/obuf/ibuf をリセット                                │
│     └─ sei->p = 0 に設定                                        │
│                                                                │
│ (6) __sei_switch(&ctx, 0x00): Phase 0から再開                   │
│     └─ 保存したCPUレジスタ状態に復帰                              │
│     └─ __begin() の直後から再実行                                │
└────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌────────────────────────────────────────────────────────────────┐
│ トランザクション再実行（新しいコアで）                             │
│   - 同じ入力メッセージで再処理                                    │
│   - 前回の変更は全て破棄されている                                │
└────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌────────────────────────────────────────────────────────────────┐
│ トランザクション終了 (__end)                                     │
│   - N-way検証（SDCチェック）                                     │
│   - 成功すればコミット                                           │
└────────────────────────────────────────────────────────────────┘
```

### 4. 主要関数

#### sei_rollback()

**ファイル**: `src/mode_cow.c`

トランザクションの状態を初期状態に戻す:

```c
void sei_rollback(sei_t* sei) {
    // COWバッファをクリア（全フェーズ）
    for (int i = 0; i < sei->redundancy_level; i++) {
        abuf_clear(sei->cow[i]);
    }

    // 出力バッファをクリア
    obuf_clear(sei->obuf);

    // 入力バッファをリワインド
    ibuf_rewind(sei->ibuf);

    // トランザクションフェーズをリセット
    sei->p = 0;
}
```

#### __sei_switch()

**ファイル**: `src/tmi_asm.S`

アセンブリで実装されたコンテキストスイッチ:

```asm
; CPUレジスタを保存した ctx に戻る
; setjmp/longjmp に似た動作
__sei_switch:
    ; ctx からレジスタを復元
    ; 第2引数をPhase番号として返す
```

#### cpu_isolation_migrate_current_thread()

**ファイル**: `src/cpu_isolation.c`

```c
int cpu_isolation_migrate_current_thread(void) {
    // 利用可能なコアを計算
    uint64_t available = available_cores & ~blacklist;

    // 全コアがブラックリストされている場合
    if (available == 0) {
        fprintf(stderr, "cpu_isolation: no available cores\n");
        exit(1);
    }

    // 最初の利用可能なコアを選択
    int new_core = __builtin_ctzll(available);

    // CPU affinityを設定
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(new_core, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);

    return new_core;
}
```

## SDCリカバリとの比較

| 項目 | SDCリカバリ | SIGSEGVリカバリ |
|------|-------------|-----------------|
| 検出タイミング | `sei_try_commit()` 時 | 実行中（即座） |
| 検出方法 | N-way比較で不一致検出 | SIGSEGVシグナル |
| 処理場所 | `__sei_commit()` 内のループ | シグナルハンドラ |
| トリガー | ソフトエラー（ビット反転等） | メモリアクセス違反 |
| 共通処理 | コアブラックリスト → 移行 → ロールバック → 再実行 |

## HEAP_PROTECTとの関係

`HEAP_PROTECT`と`SEI_SIGSEGV_RECOVERY`は両方ともSIGSEGVハンドラを使用しますが、目的が異なります:

| 機能 | HEAP_PROTECT | SEI_SIGSEGV_RECOVERY |
|------|--------------|----------------------|
| 目的 | ヒープページのCOW保護 | 予期しないSIGSEGVからの回復 |
| SIGSEGV発生 | 意図的（保護ページへの書き込み） | 予期しない（バグ/障害） |
| ハンドラ動作 | ページを書き込み可能にする | ロールバックして再実行 |
| デフォルト | 無効 | `ROLLBACK=1`で有効 |

**優先順位**: `SEI_SIGSEGV_RECOVERY` > `HEAP_PROTECT`

両方が定義されている場合、SIGSEGV_RECOVERYのハンドラが使用されます。ただし、HEAP_PROTECTはデフォルトで無効であり、通常この競合は発生しません。

## フォールトインジェクション

テスト用にSIGSEGVを意図的に発生させる機能:

**ファイル**: `src/mode_cow.c`

```c
static inline void fault_inject_sigsegv(void) {
    if (fault_get_type() != 5) return;
    if (!fault_should_inject()) return;

    fprintf(stderr, "[FAULT] type=sigsegv injecting...\n");
    g_fault_injected = 1;

    // NULLポインタアクセスでSIGSEGVを発生
    volatile int* null_ptr = NULL;
    *null_ptr = 0xDEAD;
}
```

**使用方法**:

```bash
# ビルド
make ROLLBACK=1 FAULT_INJECT=1

# 3回目のトランザクション後にSIGSEGVを発生
SEI_FAULT_TYPE=5 SEI_FAULT_INJECT_AFTER_TXN=3 ./build_sei/ukv-server.sei 10000
```

**環境変数**:

| 変数 | 説明 |
|------|------|
| `SEI_FAULT_TYPE=5` | SIGSEGVを発生させる |
| `SEI_FAULT_INJECT_AFTER_TXN=N` | N回目のトランザクション後に発生 |
| `SEI_FAULT_INJECT_DELAY_MS=N` | N ミリ秒後に発生 |

## 制限事項

1. **トランザクション外のSIGSEGV**: リカバリ不可（通常のクラッシュ）
2. **スタック破損**: 代替スタックで軽減されるが、完全な保護ではない
3. **全コア使用不可**: 全コアがブラックリストされるとプロセス終了
4. **非決定的な障害**: 同じ障害が再発する場合は無限ループの可能性

## 変更ファイル一覧

| ファイル | 変更内容 |
|----------|----------|
| `Makefile` | `ROLLBACK=1`に`-DSEI_SIGSEGV_RECOVERY`を追加 |
| `src/protect.c` | SIGSEGVリカバリハンドラ、代替スタック設定 |
| `src/tmi.c` | マルチスレッド用の`HEAP_PROTECT_INIT`呼び出し追加 |
| `src/mode_cow.c` | `SEI_FAULT_TYPE=5`（SIGSEGV）のフォールトインジェクション |
| `README.rst` | ドキュメント更新 |
