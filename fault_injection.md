# Fault Injection 機能設計書

## 概要

Fault Injection は、libseiのリカバリ機能（SDCリカバリ、SIGSEGVリカバリ）をテストするための機能です。意図的に障害を発生させ、システムが正しくリカバリできることを検証します。

## ビルド方法

```bash
# ROLLBACK（リカバリ機能）とFAULT_INJECTを有効化
make ROLLBACK=1 FAULT_INJECT=1
```

これにより以下のフラグが定義されます：
- `-DSEI_CPU_ISOLATION` - コアブラックリスト機能
- `-DSEI_SIGSEGV_RECOVERY` - SIGSEGVリカバリ機能
- `-DSEI_FAULT_INJECTION` - フォールトインジェクション機能

## 環境変数

### タイミング制御

| 環境変数 | 説明 | 例 |
|----------|------|-----|
| `SEI_FAULT_INJECT_AFTER_TXN` | N回目のトランザクション後に障害を発生 | `3` |
| `SEI_FAULT_INJECT_DELAY_MS` | 起動からNミリ秒後に障害を発生 | `5000` |

**注意**: 両方設定した場合、どちらかの条件が満たされた時点で障害が発生します。

### 障害タイプ

| `SEI_FAULT_TYPE` | 名前 | 説明 | 対象リカバリ |
|------------------|------|------|--------------|
| `0` (デフォルト) | first | COWバッファの最初のエントリを破損 | SDC |
| `1` | random | COWバッファのランダムなエントリを破損 | SDC |
| `2` | last | COWバッファの最後のエントリを破損 | SDC |
| `3` | multiple | COWバッファの複数エントリを破損 | SDC |
| `4` | talloc | tallocカウント不一致（未実装） | - |
| `5` | sigsegv | NULLポインタアクセスでSIGSEGVを発生 | SIGSEGV |

## 使用例

### SDCリカバリのテスト

```bash
# 3回目のトランザクション後に最初のエントリを破損
SEI_FAULT_TYPE=0 SEI_FAULT_INJECT_AFTER_TXN=3 ./build_sei/ukv-server.sei 10000

# ランダムなエントリを破損
SEI_FAULT_TYPE=1 SEI_FAULT_INJECT_AFTER_TXN=3 ./build_sei/ukv-server.sei 10000

# 複数エントリを破損
SEI_FAULT_TYPE=3 SEI_FAULT_INJECT_AFTER_TXN=3 ./build_sei/ukv-server.sei 10000
```

### SIGSEGVリカバリのテスト

```bash
# 3回目のトランザクション後にSIGSEGVを発生
SEI_FAULT_TYPE=5 SEI_FAULT_INJECT_AFTER_TXN=3 ./build_sei/ukv-server.sei 10000

# 5秒後にSIGSEGVを発生
SEI_FAULT_TYPE=5 SEI_FAULT_INJECT_DELAY_MS=5000 ./build_sei/ukv-server.sei 10000
```

## 実装詳細

### アーキテクチャ

```
┌─────────────────────────────────────────────────────────────────┐
│                    Fault Injection System                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  環境変数読み込み                                                 │
│  ┌────────────────────────────────────────────────────────┐    │
│  │ SEI_FAULT_TYPE         → fault_get_type()              │    │
│  │ SEI_FAULT_INJECT_AFTER_TXN → g_fault_after_txn         │    │
│  │ SEI_FAULT_INJECT_DELAY_MS  → g_fault_delay_us          │    │
│  └────────────────────────────────────────────────────────┘    │
│                           │                                     │
│                           ▼                                     │
│  タイミング判定                                                  │
│  ┌────────────────────────────────────────────────────────┐    │
│  │ fault_should_inject()                                  │    │
│  │   ├─ トランザクションカウント >= 閾値?                    │    │
│  │   └─ 経過時間 >= 遅延時間?                               │    │
│  └────────────────────────────────────────────────────────┘    │
│                           │                                     │
│                           ▼                                     │
│  障害注入                                                        │
│  ┌─────────────────────┐  ┌─────────────────────────────┐      │
│  │ Type 0-3            │  │ Type 5                      │      │
│  │ fault_inject_abuf() │  │ fault_inject_sigsegv()     │      │
│  │ COWバッファを破損    │  │ NULLポインタアクセス         │      │
│  └─────────────────────┘  └─────────────────────────────┘      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 呼び出しタイミング

```
トランザクション開始 (__begin)
        │
        ▼
┌───────────────────────────────────┐
│ アプリケーション処理               │
│                                   │
│   sei_write() が呼ばれるたび:     │
│     └─ fault_inject_sigsegv() ←──┼── Type 5: ここでSIGSEGV発生
│                                   │
└───────────────────────────────────┘
        │
        ▼
トランザクション終了 (__end)
        │
        ▼
┌───────────────────────────────────┐
│ sei_try_commit()                  │
│   │                               │
│   ├─ fault_count_transaction()    │  ← カウンタをインクリメント
│   │                               │
│   ├─ fault_inject_abuf() ←───────┼── Type 0-3: ここでCOW破損
│   │                               │
│   └─ DMR比較 (不一致検出)          │
└───────────────────────────────────┘
```

### 主要関数

#### fault_init()

**ファイル**: `src/mode_cow.c:103`

```c
static void fault_init(void) {
    if (g_fault_start_us) return;
    g_fault_start_us = now();  // 開始時刻を記録
    const char* s = getenv("SEI_FAULT_INJECT_DELAY_MS");
    if (s) g_fault_delay_us = (uint64_t)atol(s) * 1000;
}
```

`sei_init()`から呼び出され、時間ベースのトリガーを初期化します。

#### fault_should_inject()

**ファイル**: `src/mode_cow.c:115`

```c
static inline int fault_should_inject(void) {
    if (g_fault_injected) return 0;  // 既に注入済み

    // トランザクションカウントベースのチェック
    if (g_fault_after_txn > 0 && g_transaction_count >= g_fault_after_txn) {
        return 1;
    }

    // 時間ベースのチェック
    if (g_fault_delay_us > 0) {
        return (now() - g_fault_start_us) >= g_fault_delay_us;
    }

    return 0;
}
```

障害を注入するタイミングかどうかを判定します。一度障害が注入されると、以降は常に`0`を返します。

#### fault_inject_abuf()

**ファイル**: `src/mode_cow.c:178`

```c
static void fault_inject_abuf(abuf_t* abuf) {
    if (!fault_should_inject()) return;

    int fault_type = fault_get_type();

    switch (fault_type) {
    case 0: abuf_corrupt_first(abuf);    break;  // 最初のエントリ
    case 1: abuf_corrupt_random(abuf);   break;  // ランダム
    case 2: abuf_corrupt_last(abuf);     break;  // 最後のエントリ
    case 3: abuf_corrupt_multiple(abuf); break;  // 複数
    case 5: return;  // SIGSEGVは別処理
    }
    g_fault_injected = 1;
}
```

COWバッファのエントリを破損させます。`sei_try_commit()`内のDMR比較の直前で呼び出されます。

#### fault_inject_sigsegv()

**ファイル**: `src/mode_cow.c:157`

```c
static inline void fault_inject_sigsegv(void) {
    if (fault_get_type() != 5) return;
    if (!fault_should_inject()) return;

    fprintf(stderr, "[FAULT] type=sigsegv injecting...\n");
    g_fault_injected = 1;

    // NULLポインタアクセスでSIGSEGV発生
    volatile int* null_ptr = NULL;
    *null_ptr = 0xDEAD;
}
```

NULLポインタへの書き込みでSIGSEGVを発生させます。`sei_write()`マクロ内で呼び出されます。

### COWバッファの破損方法

#### abuf_corrupt_first()

**ファイル**: `src/abuf.c:677`

```c
void abuf_corrupt_first(abuf_t* abuf) {
    if (abuf->pushed == 0) return;

    // 最初のエントリの値の最下位ビットを反転
    abuf_entry_t* e = &abuf->buf[0];
    ABUF_WVAL(e) ^= 1;
}
```

#### abuf_corrupt_random()

**ファイル**: `src/abuf.c:690`

```c
void abuf_corrupt_random(abuf_t* abuf) {
    if (abuf->pushed == 0) return;

    // ランダムなエントリを選択して破損
    int idx = rand() % abuf->pushed;
    abuf_entry_t* e = &abuf->buf[idx];
    ABUF_WVAL(e) ^= 1;
}
```

#### abuf_corrupt_last()

**ファイル**: `src/abuf.c:703`

```c
void abuf_corrupt_last(abuf_t* abuf) {
    if (abuf->pushed == 0) return;

    // 最後のエントリを破損
    abuf_entry_t* e = &abuf->buf[abuf->pushed - 1];
    ABUF_WVAL(e) ^= 1;
}
```

#### abuf_corrupt_multiple()

**ファイル**: `src/abuf.c:715`

```c
void abuf_corrupt_multiple(abuf_t* abuf) {
    if (abuf->pushed < 2) return;

    // 複数のエントリを破損
    int count = 0;
    for (int i = 0; i < abuf->pushed && count < 3; i += 2) {
        abuf->buf[i].val.u64 ^= 1;
        count++;
    }
}
```

## リカバリの動作確認

### SDCリカバリ（Type 0-3）

```
┌────────────────────────────────────────────────────────────────┐
│ 正常なフロー                                                    │
├────────────────────────────────────────────────────────────────┤
│ Phase 0: 実行 → COW[0]に書き込みを記録                         │
│ Phase 1: 実行 → COW[1]に書き込みを記録                         │
│ DMR比較: COW[0] == COW[1] → コミット成功                       │
└────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────┐
│ 障害注入後のフロー                                              │
├────────────────────────────────────────────────────────────────┤
│ Phase 0: 実行 → COW[0]に書き込みを記録                         │
│ Phase 1: 実行 → COW[1]に書き込みを記録                         │
│ fault_inject_abuf(): COW[0]の値を破損 ← 障害注入               │
│ DMR比較: COW[0] != COW[1] → 不一致検出！                       │
│   ├─ cpu_isolation_blacklist_current()                         │
│   ├─ cpu_isolation_migrate_current_thread()                    │
│   ├─ sei_rollback()                                            │
│   └─ Phase 0から再実行                                         │
└────────────────────────────────────────────────────────────────┘
```

### SIGSEGVリカバリ（Type 5）

```
┌────────────────────────────────────────────────────────────────┐
│ 障害注入後のフロー                                              │
├────────────────────────────────────────────────────────────────┤
│ __begin(): コンテキスト保存                                     │
│     │                                                          │
│     ▼                                                          │
│ sei_write(): fault_inject_sigsegv() ← NULLアクセス             │
│     │                                                          │
│     ▼ SIGSEGV発生                                              │
│ protect_handler(): (代替スタック上で実行)                        │
│   ├─ cpu_isolation_blacklist_current()                         │
│   ├─ cpu_isolation_migrate_current_thread()                    │
│   ├─ sei_rollback()                                            │
│   └─ __sei_switch() → __begin()直後に戻る                      │
│     │                                                          │
│     ▼                                                          │
│ リトライ開始（別コアで）                                         │
└────────────────────────────────────────────────────────────────┘
```

## 期待される出力

### SDCリカバリ成功時

```
[FAULT] type=first injected
[DMR] mismatch detected, rolling back...
[CPU] blacklisted core 0
[CPU] migrating to core 1
(処理継続)
```

### SIGSEGVリカバリ成功時

```
[FAULT] type=sigsegv injecting...
[SIGSEGV] recovery triggered
[CPU] blacklisted core 0
[CPU] migrating to core 1
(処理継続)
```

## 制限事項

1. **一度のみ注入**: 障害は1回のみ注入されます。`g_fault_injected`フラグで管理されます。
2. **Type 4未実装**: tallocカウント不一致の障害注入は未実装です。
3. **ビルドフラグ必須**: `FAULT_INJECT=1`でビルドしないと障害注入コードは含まれません。
4. **ROLLBACKとの併用**: リカバリをテストする場合は`ROLLBACK=1`も必要です。

## 関連ファイル

| ファイル | 内容 |
|----------|------|
| `src/mode_cow.c` | 障害注入のメイン実装 |
| `src/abuf.c` | COWバッファの破損関数 |
| `src/abuf.h` | 破損関数の宣言 |
| `src/protect.c` | SIGSEGVハンドラ |
| `Makefile` | `FAULT_INJECT=1`フラグの処理 |
