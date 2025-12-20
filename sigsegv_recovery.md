# SIGSEGV Recovery 機能設計書

## 概要

SIGSEGV Recovery は、トランザクション実行中に発生するセグメンテーションフォールトから自動的に回復する機能です。ROLLBACKフラグの一部として統合されており、SDC（Silent Data Corruption）リカバリと同じインフラストラクチャを使用します。

## 設計目標

1. トランザクション内でSIGSEGVが発生しても、プロセスを継続する
2. アプリケーションコードの変更を不要にする（透過的なリカバリ）
3. 障害コアをブラックリストに追加し、別コアで再実行する
4. 全コアが使用不可になった場合のみプロセスを終了する

## 基本原理

### なぜプロセスが停止しないのか

通常、SIGSEGVが発生するとOSはプロセスを強制終了します。しかし、`sigaction()`でシグナルハンドラを登録することで、OSはプロセスを終了する代わりにハンドラ関数を呼び出します。

```c
// デフォルト動作
SIGSEGV発生 → OSがプロセスを強制終了（core dump）

// ハンドラ登録後
SIGSEGV発生 → OSがハンドラを呼び出す → ハンドラ内でリカバリ処理
```

### 代替スタック（Alternate Signal Stack）

#### なぜ代替スタックが必要か

シグナルハンドラは通常、現在のスタック上で実行されます。しかし、以下の場合にメインスタックは使用できません：

1. **スタックオーバーフロー**: スタック領域を使い果たしてSIGSEGVが発生した場合、ハンドラを実行する空き領域がない
2. **スタックポインタの破損**: ハードウェア障害でRSPが不正な値になった場合

```
メインスタック（満杯/破損）        代替スタック（別領域）
┌─────────────────┐              ┌─────────────────┐
│ xxxxxxxxxx      │              │ [空き64KB]      │
│ xxxxxxxxxx      │              │                 │
│ xxxxxxxxxx      │              │ protect_handler │ ← ここで実行
└─────────────────┘              └─────────────────┘
     使えない                          安全に使える
```

#### 代替スタックの設定

```c
// 64KBの専用メモリ領域を確保（スレッドローカル）
static __thread char altstack[64 * 1024];

// OSにこの領域を代替スタックとして登録
stack_t ss;
ss.ss_sp = altstack;
ss.ss_size = 64 * 1024;
ss.ss_flags = 0;
sigaltstack(&ss, NULL);

// シグナルハンドラにSA_ONSTACKを指定
sa.sa_flags = SA_SIGINFO | SA_ONSTACK;  // 代替スタックを使用
```

代替スタックは事前に確保された**空のメモリ領域**です。SIGSEGV発生時、OSがこの領域上にハンドラ用のスタックフレームを作成します。

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
代替スタック                          メインスタック
┌────────────────┐                   ┌────────────────────────┐
│                │                   │ main()                 │
│                │                   │   └─ while(1)          │
│                │                   │       └─ __begin() ←───┼── ctx保存
│                │                   │           │            │
│                │                   │           ▼            │
│                │                   │       handle_request() │
│                │   SIGSEGV発生 ←──┼───────────┘            │
│                │                   │                        │
│ protect_handler│ ←─ OSが代替      │                        │
│   │            │    スタックで     │                        │
│   ├─blacklist  │    ハンドラ実行  │                        │
│   ├─migrate    │                   │                        │
│   ├─rollback ★│ ← 代替スタック上  │                        │
│   │            │   でメモリ復元    │                        │
│   └─__sei_switch ──────────────────┼──→ (ここに戻る)       │
│                │                   │       └─ __begin()直後 │
└────────────────┘                   │           リトライ開始  │
                                     └────────────────────────┘
```

**重要なポイント**:
- `sei_rollback()`は**代替スタック上で実行**される
- `__sei_switch()`が呼ばれた瞬間に**メインスタックに戻る**
- メインスタックの「安全な位置」（`__begin`直後）から再実行が始まる

### 4. メモリのロールバック

#### COW（Copy-on-Write）の動作

**書き込み時**:
```c
abuf_push(cow, addr, *addr);  // 1. 古い値をCOWバッファに保存
*addr = value;                 // 2. 新しい値を直接メモリに書き込み
```

**ロールバック時**:
```c
abuf_restore_filtered(sei->cow[0], ...);  // COWバッファから古い値をメモリに復元
abuf_clean(sei->cow[i]);                   // その後バッファをクリア
```

図解:
```
┌────────────────────────────────────────────────────────────────┐
│ 書き込み時                                                      │
├────────────────────────────────────────────────────────────────┤
│  メモリ[addr] = 100 (元の値)                                    │
│        ↓                                                       │
│  abuf_push(cow, addr, 100)  ← 元の値100をCOWバッファに保存     │
│        ↓                                                       │
│  *addr = 200               ← 新しい値200をメモリに直接書き込み  │
│        ↓                                                       │
│  メモリ[addr] = 200, COW = {addr, 100}                         │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│ ロールバック時                                                  │
├────────────────────────────────────────────────────────────────┤
│  メモリ[addr] = 200, COW = {addr, 100}                         │
│        ↓                                                       │
│  abuf_restore(cow)         ← COWバッファから復元               │
│  *addr = 100               ← 元の値100をメモリに書き戻す       │
│        ↓                                                       │
│  abuf_clean(cow)           ← バッファをクリア                  │
│        ↓                                                       │
│  メモリ[addr] = 100, COW = {} (元通り！)                       │
└────────────────────────────────────────────────────────────────┘
```

### 5. 主要関数

#### sei_rollback()

**ファイル**: `src/mode_cow.c`

```c
void sei_rollback(sei_t* sei) {
    // Step 1: COWバッファからメモリを復元
    abuf_restore_filtered(sei->cow[0], sei->talloc);

    // Step 2: COWバッファをクリア（全フェーズ）
    for (int i = 0; i < sei->redundancy_level; i++) {
        abuf_clean(sei->cow[i]);
    }

    // Step 3: 動的メモリ割り当てをロールバック
    tbin_reset(sei->tbin);
    talloc_rollback(sei->talloc);

    // Step 4: 出力バッファをクリア
    obuf_reset(sei->obuf);

    // Step 5: 入力バッファをリセット
    ibuf_reset(sei->ibuf);

    // Step 6: トランザクションフェーズをリセット
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
    ; ctx からレジスタを復元（RSP含む）
    ; 第2引数をPhase番号として返す
```

この関数が呼ばれると、保存されたCPUレジスタ（スタックポインタ含む）が復元され、メインスタックの`__begin()`直後の位置に「ジャンプ」します。

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

## リカバリの適用範囲

### 復元できるもの

| 対象 | 復元方法 |
|------|----------|
| トランザクション中の書き込み | COWバッファから古い値を復元 |
| sei構造体の状態 | `sei->p = 0`にリセット |
| 動的メモリ割り当て | `talloc_rollback()` |
| 出力バッファ | `obuf_reset()` |

### 復元できないもの

| 対象 | 理由 |
|------|------|
| トランザクション外で発生した破損 | COWで追跡していない |
| libsei管理外のメモリ | COWで追跡していない |
| ソフトウェアバグ | 再実行しても同じエラーが発生 |

### 想定するユースケース

```
┌─────────────────────────────────────────────────────────────┐
│ リカバリが有効なケース                                        │
├─────────────────────────────────────────────────────────────┤
│ ✓ 一時的なハードウェア障害（ビット反転、キャッシュエラー）      │
│ ✓ 特定コアのみの障害                                        │
│ ✓ トランザクション開始前の状態は正常                          │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│ リカバリが無効なケース                                        │
├─────────────────────────────────────────────────────────────┤
│ ✗ ソフトウェアバグ（再実行しても同じエラー）                   │
│ ✗ 永続的なメモリ破損（全コアで同じ障害）                      │
│ ✗ トランザクション外で発生した破損                            │
└─────────────────────────────────────────────────────────────┘
```

## SDCリカバリとの比較

| 項目 | SDCリカバリ | SIGSEGVリカバリ |
|------|-------------|-----------------|
| 検出タイミング | `sei_try_commit()` 時 | 実行中（即座） |
| 検出方法 | N-way比較で不一致検出 | SIGSEGVシグナル |
| 処理場所 | `__sei_commit()` 内のループ | シグナルハンドラ（代替スタック） |
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
4. **ソフトウェアバグ**: 同じバグが再発する場合はリカバリできない（無限ループの可能性）
5. **libsei管理外のメモリ**: COWで追跡していないメモリの破損は復元できない

## 変更ファイル一覧

| ファイル | 変更内容 |
|----------|----------|
| `Makefile` | `ROLLBACK=1`に`-DSEI_SIGSEGV_RECOVERY`を追加 |
| `src/protect.c` | SIGSEGVリカバリハンドラ、代替スタック設定 |
| `src/tmi.c` | マルチスレッド用の`HEAP_PROTECT_INIT`呼び出し追加 |
| `src/mode_cow.c` | `SEI_FAULT_TYPE=5`（SIGSEGV）のフォールトインジェクション |
| `README.rst` | ドキュメント更新 |
