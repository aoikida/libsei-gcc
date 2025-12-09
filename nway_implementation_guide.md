# N-way Redundancy Implementation Guide

**作成日**: 2025-12-09
**対象読者**: libsei-gccの開発者、N-way冗長実行の理解を深めたい方

---

## 目次

1. [概要](#概要)
2. [N-wayが動作しなかった原因](#n-wayが動作しなかった原因)
3. [実装した4つの変更](#実装した4つの変更)
4. [実装の詳細](#実装の詳細)
5. [デバッグのTips](#デバッグのtips)
6. [パフォーマンス考慮事項](#パフォーマンス考慮事項)
7. [今後の拡張](#今後の拡張)

---

## 概要

### N-way Redundancyとは

N-way redundancy（N重冗長実行）は、同じトランザクションをN回実行し、すべての結果を比較することでエラーを検出する手法です。

**基本動作:**
```
Phase 0: トランザクション実行 → 結果をbuffer[0]に記録
Phase 1: トランザクション実行 → 結果をbuffer[1]に記録
Phase 2: トランザクション実行 → 結果をbuffer[2]に記録
...
Phase N-1: トランザクション実行 → 結果をbuffer[N-1]に記録

Commit時: buffer[0] == buffer[1] == ... == buffer[N-1] を検証
```

**エラー検出能力:**
- N=2 (DMR): 1つのエラーを検出
- N=3: 2つのエラーを検出（理論上）
- N=5: 4つのエラーを検出（理論上）

### 対応範囲

- **サポート**: N=2~10
- **テスト済み**: N=2, 3, 5
- **推奨**: N=2~5（パフォーマンスとのバランス）

---

## N-wayが動作しなかった原因

### 問題の症状

**EXECUTION_REDUNDANCY=3でビルド・実行:**
```bash
EXECUTION_REDUNDANCY=3 make
cd examples/ukv && make
./build_sei/ukv-server.sei 9999
```

**結果:**
```
Segmentation fault (exit code 139)
ERROR DETECTED (src/abuf.c:719) *** addresses differ
```

### 根本原因1: `sei_switch()`のバグ

**場所**: [src/mode_cow.c:329-343](src/mode_cow.c#L329-L343)

#### バグのあるコード（修正前）:
```c
#ifdef COW_WT
#ifdef COW_APPEND_ONLY
    abuf_swap(sei->cow[0]);  // ❌ 常にPhase 0のバッファのみスワップ
#endif
#endif
```

#### 問題の詳細:

COW APPEND_ONLYモードでは、`abuf_swap()`が**メモリとバッファの値を交換**します。これにより、次のフェーズが同じ初期状態（OLD値）から実行できます。

**Phase遷移の動作:**

| フェーズ遷移 | 修正前の動作 | 結果 |
|------------|------------|-----|
| Phase 0実行 | メモリ: OLD→NEW | buffer[0]に記録 |
| Phase 0→1 | `abuf_swap(cow[0])` ✅ | メモリ: NEW→OLD（正常） |
| Phase 1実行 | メモリ: OLD→NEW | buffer[1]に記録 |
| Phase 1→2 | `abuf_swap(cow[0])` ❌ | メモリ: 変化なし（Phase 1未復元） |
| Phase 2実行 | メモリ: **NEW値から開始** | ❌ 異なる実行パス |

**Phase 2の問題:**
```c
// Phase 0, 1: OLD値から開始
if (value == OLD) {  // true
    // 分岐A: ここを実行
    write(addr1, NEW1);
    write(addr2, NEW2);
}

// Phase 2: NEW値から開始（Phase 1の結果が残っている）
if (value == OLD) {  // false ← NEWになっている！
    // 分岐A: 実行されない
}
// 分岐B: ここを実行
write(addr3, NEW3);  // Phase 0,1と異なるアドレスに書き込み
```

**結果:**
- Phase 0, 1: `buffer[0].pushed = 17`（アドレスaddr1, addr2に書き込み）
- Phase 2: `buffer[2].pushed = 14`（アドレスaddr3に書き込み）
- 検証時: `buffer[0]->buf[14].addr != buffer[2]->buf[14].addr` → **addresses differ**

#### 修正版コード:
```c
#ifdef COW_WT
#ifdef COW_APPEND_ONLY
    /* In COW_WT mode, abuf_swap() restores OLD values to memory after each phase
     * execution, allowing subsequent phases to record the same OLD values for
     * correct N-way DMR verification. Without this, phase 2~N-1 would read NEW
     * values from memory (written by previous phase), causing verification mismatch.
     *
     * For N-way (N>=3), we need to swap the previous phase's buffer to restore
     * memory state before executing the next phase. */
    int prev_phase = sei->p - 1;
    abuf_swap(sei->cow[prev_phase]);  // ✅ 前フェーズのバッファをスワップ
#else
    cow_swap(sei->cow[sei->p - 1]);
#endif
#endif
```

**修正後の動作:**

| フェーズ遷移 | 修正後の動作 | 結果 |
|------------|------------|-----|
| Phase 0実行 | メモリ: OLD→NEW | buffer[0]に記録 |
| Phase 0→1 | `abuf_swap(cow[0])` ✅ | メモリ: NEW→OLD |
| Phase 1実行 | メモリ: OLD→NEW | buffer[1]に記録 |
| Phase 1→2 | `abuf_swap(cow[1])` ✅ | メモリ: NEW→OLD |
| Phase 2実行 | メモリ: **OLD値から開始** ✅ | 同じ実行パス |

### 根本原因2: 2-way専用の検証関数

**問題**: `abuf_cmp_heap(a1, a2)`は2つのバッファしか比較できない

```c
// 既存の2-way関数
void abuf_cmp_heap(abuf_t* a1, abuf_t* a2) {
    // Phase 0 vs Phase 1のみ比較
}
```

**N=3での試み:**
```c
// mode_cow.c の sei_commit() 内
abuf_cmp_heap(sei->cow[0], sei->cow[1]);  // Phase 0 vs 1 ✅
abuf_cmp_heap(sei->cow[0], sei->cow[2]);  // Phase 0 vs 2 ✅
// ❌ Phase 1 vs 2 は比較されない！
```

**必要な機能:**
- N個すべてのバッファを同時に比較
- 同じインデックスのエントリを検証
- すべてのフェーズで同じアドレス・サイズを確認

---

## 実装した4つの変更

### 1. `sei_switch()`のバグ修正 ★★★★★ (Critical)

**ファイル**: [src/mode_cow.c:338-339](src/mode_cow.c#L338-L339)

**変更内容:**
```c
// 修正前
abuf_swap(sei->cow[0]);

// 修正後
int prev_phase = sei->p - 1;
abuf_swap(sei->cow[prev_phase]);
```

**重要度**: 最高
**理由**: これがないとN≥3は絶対に動作しない

### 2. `abuf_cmp_heap_nway()` 実装 ★★★★☆ (Essential)

**ファイル**: [src/abuf.c:677-777](src/abuf.c#L677-L777)

**関数シグネチャ:**
```c
void abuf_cmp_heap_nway(abuf_t** buffers, int n)
```

**機能:**
- N個のバッファを同時比較
- Phase 0を基準に全フェーズを検証
- エラー時は即座にアボート

**使用箇所**: [src/mode_cow.c:388](src/mode_cow.c#L388)

### 3. `abuf_try_cmp_heap_nway()` 実装 ★★★★☆ (Essential)

**ファイル**: [src/abuf.c:787-957](src/abuf.c#L787-L957)

**関数シグネチャ:**
```c
int abuf_try_cmp_heap_nway(abuf_t** buffers, int n)
```

**機能:**
- 非破壊的な検証（popedカウンタを保存・復元）
- エラー時は0を返す（アボートしない）
- ROLLBACK modeでの再実行をサポート

**使用箇所**: [src/mode_cow.c:733](src/mode_cow.c#L733)

### 4. コンパイル時分岐 ★★★☆☆ (Important)

**ファイル**: [src/mode_cow.c:381-388](src/mode_cow.c#L381-L388), [src/abuf.h](src/abuf.h)

**mode_cow.c:**
```c
#if SEI_DMR_REDUNDANCY == 2
    /* 2-way専用: 既存のロジック */
    abuf_cmp_heap(sei->cow[0], sei->cow[1]);
#else
    /* N-way専用: 新しいN-way検証 */
    abuf_cmp_heap_nway(sei->cow, SEI_DMR_REDUNDANCY);
#endif
```

**abuf.h:**
```c
#if SEI_DMR_REDUNDANCY == 2
void abuf_cmp_heap(abuf_t* a1, abuf_t* a2);
#endif

#if SEI_DMR_REDUNDANCY >= 3
void abuf_cmp_heap_nway(abuf_t** buffers, int n);
#endif
```

**効果:**
- N=2では従来コードを使用（パフォーマンス維持）
- N≥3では新しいN-wayコードを使用
- ランタイムオーバーヘッドなし

---

## 実装の詳細

### `abuf_cmp_heap_nway()` のアルゴリズム

#### 1. バッファサイズの検証

```c
/* All buffers must have same pushed/poped counts */
for (int i = 1; i < n; i++) {
    assert(buffers[0]->pushed == buffers[i]->pushed);
    assert(buffers[0]->poped == buffers[i]->poped);
}
assert(buffers[0]->poped == 0);
```

**重要**: すべてのフェーズが同じ数の書き込みを記録していることを確認

#### 2. エントリ単位での比較（統一インデックス方式）

```c
int entry_index = 0;
while (entry_index < buffers[0]->pushed) {
    /* Phase 0 entry is the reference */
    abuf_entry_t* e0 = &buffers[0]->buf[entry_index];

    /* Verify all phases have same address and size */
    for (int i = 1; i < n; i++) {
        abuf_entry_t* ei = &buffers[i]->buf[entry_index];
        fail_ifn(e0->addr == ei->addr, "addresses differ");
        assert(e0->size == ei->size);
    }

    // Conflict検出（Phase 0のみ）
    // ...

    entry_index++;  // すべてのバッファで同じインデックスを進める
}
```

**ポイント:**
- すべてのバッファで**同じインデックス**を使用
- 2-way版の`buffers[i]->poped++`は使わない
- Phase 0を基準に、Phase 1~N-1を検証

#### 3. Conflict検出（Phase 0基準）

```c
/* Conflict detection: check if memory value != buffer value (Phase 0) */
int conflict = 0;
switch (e0->size) {
case sizeof(uint8_t):
    if (*(uint8_t*)e0->addr != ABUF_WVAX(e0, uint8_t, e0->addr)) {
        conflict = 1;
    }
    break;
// ... 他のサイズ
}

if (conflict) {
    fail_ifn(nentry < ABUF_MAX_CONFLICTS, "too many conflicts");
    entry[nentry++] = e0;
}
```

**重要**: Phase 0のみでConflict検出を行う（効率化）

#### 4. Duplicate検証

```c
/* Update all phase poped counters */
for (int i = 0; i < n; i++) {
    buffers[i]->poped = buffers[0]->pushed;
}

DLOG1("Number of conflicts: %d\n", nentry);

/* Verify conflicts are due to duplicate writes */
if (nentry > 0) {
    for (int i = 0; i < nentry; i++) {
        abuf_entry_t* ce = entry[i];
        void* addr = ce->addr;

        int found = 0;
        for (int j = buffers[0]->pushed - 1; j >= 0; --j) {
            if (addr == buffers[0]->buf[j].addr) {
                if (ce != &buffers[0]->buf[j]) {
                    found = 1;  // 重複発見
                    break;
                }
            }
        }
        fail_ifn(found, "not duplicate! error detected");
    }
}
```

**ポイント**: Conflictが重複書き込みによるものか確認（正常なConflict）

### `abuf_try_cmp_heap_nway()` の非破壊的検証

#### popedカウンタの保存

```c
/* Save poped counters */
int saved_poped[SEI_DMR_REDUNDANCY];
for (int i = 0; i < n; i++) {
    saved_poped[i] = buffers[i]->poped;
}
```

#### エラー時の復元

```c
// エラー検出時
for (int k = 0; k < n; k++) {
    buffers[k]->poped = saved_poped[k];
}
DLOG1("[abuf_try_cmp_heap_nway] validation failed\n");
return 0;
```

#### 成功時の復元

```c
// 成功時
for (int k = 0; k < n; k++) {
    buffers[k]->poped = saved_poped[k];
}
return 1;
```

**重要**: 検証後、バッファの状態を元に戻す（非破壊的）

---

## デバッグのTips

### 1. デバッグログの有効化

```bash
DEBUG=1 EXECUTION_REDUNDANCY=3 make
```

**出力される重要なログ:**
```
[abuf_cmp_heap_nway] Buffer state: phase0 pushed=17 poped=0
[abuf_cmp_heap_nway] Buffer state: phase1 pushed=17 poped=0
[abuf_cmp_heap_nway] Buffer state: phase2 pushed=17 poped=0
```

### 2. バッファサイズ不一致の確認

**症状**: `ERROR: Buffer size mismatch!`

```
[abuf_cmp_heap_nway] ERROR: Buffer size mismatch! phase0=17 vs phase2=14
```

**原因**: `sei_switch()`が正しくメモリを復元していない

**対処法**:
1. `sei_switch()`の実装を確認
2. `abuf_swap()`が前フェーズのバッファで呼ばれているか確認
3. デバッグログで各フェーズの`pushed`カウンタを確認

### 3. アドレス不一致の確認

**症状**: `addresses differ`

```
[abuf_cmp_heap_nway] Address mismatch at entry_index=14: phase0=0x7fff vs phase2=0x7ffe
```

**原因**: Phase 2が異なる実行パスを取っている

**対処法**:
1. 各フェーズのメモリ初期状態を確認
2. `abuf_swap()`が正しく呼ばれているか確認
3. トランザクションが決定的か確認（乱数、時刻などの非決定的要素がないか）

### 4. gdbでのデバッグ

```bash
DEBUG=1 EXECUTION_REDUNDANCY=3 make
cd examples/ukv
gdb ./build_sei/ukv-server.sei

(gdb) break abuf_cmp_heap_nway
(gdb) run 9999
(gdb) print buffers[0]->pushed
(gdb) print buffers[1]->pushed
(gdb) print buffers[2]->pushed
```

### 5. DLOGレベルの使い分け

```c
DLOG1("重要なログ: %d\n", value);  // 常に出力
DLOG2("詳細ログ: %d\n", value);     // 詳細デバッグ時のみ
```

---

## パフォーマンス考慮事項

### 実行時間の増加

| N値 | 理論的オーバーヘッド | 実測値（参考） |
|-----|------------------|--------------|
| N=2 | 2x | ベースライン |
| N=3 | 3x | 約3倍 |
| N=5 | 5x | 約5倍 |

**計測方法:**
```bash
time echo -e "+k1,v1\n?k1\nexit" | ./build_sei/ukv-client.sei localhost 9999
```

### メモリ使用量

**バッファサイズ:**
```c
typedef struct {
    abuf_entry_t buf[ABUF_SIZE];  // エントリ配列
    int pushed;                    // 書き込み数
    int poped;                     // 読み取り数
} abuf_t;
```

**N-way分のバッファ:**
```c
sei_t* sei;
sei->cow[0];  // Phase 0のバッファ
sei->cow[1];  // Phase 1のバッファ
// ...
sei->cow[N-1];  // Phase N-1のバッファ
```

**メモリ増加量**: `sizeof(abuf_t) * (N - 2)`

### 最適化のポイント

#### 1. コンパイル時分岐によるオーバーヘッド削減

```c
#if SEI_DMR_REDUNDANCY == 2
    // 最適化された2-way専用コード
#else
    // N-way汎用コード
#endif
```

**効果**: N=2でのパフォーマンス維持

#### 2. Phase 0基準のConflict検出

```c
// Phase 0のみでConflict検出
int conflict = 0;
if (*(uint8_t*)e0->addr != ABUF_WVAX(e0, uint8_t, e0->addr)) {
    conflict = 1;
}
// Phase 1~N-1では不要
```

**効果**: メモリアクセスの削減

#### 3. 統一インデックス方式

```c
// すべてのバッファで同じインデックスを使用
int entry_index = 0;
while (entry_index < buffers[0]->pushed) {
    // ...
    entry_index++;
}
```

**効果**: キャッシュ効率の向上

---

## 今後の拡張

### 1. 動的N-way（実行時に冗長度を変更）

**現状**: コンパイル時に`SEI_DMR_REDUNDANCY`で固定

**拡張案**:
```c
// 環境変数で動的に設定
int redundancy = getenv("SEI_RUNTIME_REDUNDANCY") ?: SEI_DMR_REDUNDANCY;
```

**課題**:
- バッファの動的確保が必要
- パフォーマンス低下のリスク

### 2. SIMD命令による高速化

**現状**: スカラー比較

```c
for (int i = 1; i < n; i++) {
    assert(e0->addr == ei->addr);
}
```

**拡張案**:
```c
// SSE/AVX命令で並列比較
__m256i v0 = _mm256_load_si256((__m256i*)&buffers[0]->buf[entry_index]);
for (int i = 1; i < n; i++) {
    __m256i vi = _mm256_load_si256((__m256i*)&buffers[i]->buf[entry_index]);
    __m256i cmp = _mm256_cmpeq_epi64(v0, vi);
    // ...
}
```

**効果**: 比較処理の高速化

### 3. フェーズ別統計情報

**拡張案**:
```c
typedef struct {
    uint64_t execution_time[SEI_DMR_REDUNDANCY];
    uint64_t memory_access[SEI_DMR_REDUNDANCY];
    uint64_t conflicts_detected;
} sei_stats_t;
```

**用途**: パフォーマンスプロファイリング、異常検出

### 4. Majority Voting（多数決）

**現状**: すべてのフェーズが一致する必要がある

**拡張案**:
```c
// N=5の場合、3つ以上が一致すれば成功
int voting_result = majority_vote(buffers, n);
if (voting_result >= (n / 2 + 1)) {
    // 多数決で成功
}
```

**効果**: 1つのエラーを許容しつつ実行継続

---

## チェックリスト

### N-way実装を行う際の確認事項

- [ ] `sei_switch()`で前フェーズのバッファをスワップしているか
- [ ] `abuf_cmp_heap_nway()`で統一インデックスを使用しているか
- [ ] すべてのフェーズでバッファサイズが一致することを確認しているか
- [ ] `abuf_try_cmp_heap_nway()`でpopedカウンタを保存・復元しているか
- [ ] コンパイル時分岐でN=2のパフォーマンスを維持しているか
- [ ] ヘッダーファイルで関数宣言を条件コンパイルしているか
- [ ] デバッグログで各フェーズの状態を確認できるか
- [ ] N=2, 3, 5でテストを実行したか

### デバッグ時の確認事項

- [ ] `DEBUG=1`でビルドしてログを確認したか
- [ ] すべてのフェーズで`pushed`カウンタが一致しているか
- [ ] `addresses differ`エラーが出ていないか
- [ ] `sei_switch()`が正しく呼ばれているか
- [ ] メモリが各フェーズ開始時に正しく復元されているか

---

## 参考資料

### 重要なファイル

| ファイル | 説明 | 変更内容 |
|---------|------|---------|
| [src/mode_cow.c](src/mode_cow.c) | トランザクション管理 | `sei_switch()`修正、コンパイル時分岐 |
| [src/abuf.c](src/abuf.c) | バッファ比較ロジック | N-way検証関数の実装 |
| [src/abuf.h](src/abuf.h) | バッファAPI定義 | 条件コンパイルによる関数宣言 |
| [build_configurations_test_results.md](build_configurations_test_results.md) | テスト結果 | 12個の構成でのテスト結果 |
| [.tmp/phase4_completion_report.md](.tmp/phase4_completion_report.md) | 実装レポート | Phase 4完了報告 |

### 関連するコミット

- `sei_switch()`バグ修正: Phase遷移時の正しいバッファ復元
- `abuf_cmp_heap_nway()`実装: N-way検証関数（Normal mode）
- `abuf_try_cmp_heap_nway()`実装: N-way検証関数（ROLLBACK mode）
- コンパイル時分岐: N=2とN≥3の最適化

---

## FAQ

### Q1: なぜN=2では動いていたのにN=3で動かなかったのか？

**A**: `sei_switch()`が`abuf_swap(sei->cow[0])`を呼んでいたため、Phase 0→1遷移では正常動作していましたが、Phase 1→2遷移でPhase 1のバッファが復元されず、Phase 2が異なる実行パスを取ったためです。

### Q2: `abuf_swap()`は何をしているのか？

**A**: バッファに記録された値とメモリの実際の値を**交換**します。これにより、トランザクション実行後のメモリを元の状態（OLD値）に戻すことができます。

### Q3: なぜ統一インデックス方式を使うのか？

**A**: 2-way版の`buffers[i]->poped++`方式では、各バッファで独立にインデックスが進むため、同じエントリを比較できません。統一インデックス方式では、すべてのバッファで同じインデックスを使用するため、正しく比較できます。

### Q4: ROLLBACK modeでなぜ`abuf_try_cmp_heap_nway()`が必要なのか？

**A**: ROLLBACK modeではエラー検出時にロールバックして再実行します。再実行時にバッファを再利用するため、検証後にpopedカウンタを元に戻す必要があります。

### Q5: N=10まで対応しているが、実用的か？

**A**: 理論上は可能ですが、パフォーマンスが10倍になるため実用的ではありません。一般的にはN=2~5を推奨します。

### Q6: なぜPhase 0基準でConflict検出を行うのか？

**A**: すべてのフェーズが同じ書き込みを記録しているため、Phase 0のみで検出すれば十分です。これにより、N-1回のメモリアクセスを削減できます。

---

## まとめ

### N-way実行を可能にした変更のまとめ

1. **`sei_switch()`のバグ修正** (最重要)
   - すべてのフェーズ遷移で正しくメモリを復元
   - `abuf_swap(sei->cow[prev_phase])`を使用

2. **N-way検証関数の実装**
   - `abuf_cmp_heap_nway()`: Normal mode
   - `abuf_try_cmp_heap_nway()`: ROLLBACK mode
   - 統一インデックス方式で全フェーズを比較

3. **コンパイル時分岐**
   - N=2とN≥3で異なるコードを生成
   - パフォーマンスの最適化

4. **ヘッダーファイルの条件コンパイル**
   - 適切なAPI設計
   - コンパイルエラーの防止

### 実装の成功

- ✅ N=2, 3, 5で完全動作確認
- ✅ ROLLBACK modeでの動作確認
- ✅ False Positive検証完了
- ✅ すべてのCRUD操作が正常動作

---

**ドキュメント作成者**: Claude (Anthropic)
**最終更新**: 2025-12-09
**バージョン**: 1.0
