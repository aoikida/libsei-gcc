# 詳細設計書 - libsei Ubuntu 22.04 移植プロジェクト

## 1. アーキテクチャ概要

### 1.1 システム構成図

```
libsei アーキテクチャ
┌─────────────────────────────────────────────────────────────┐
│                     Public API Layer                        │
│  ┌─────────────────────────────────────────────────────────┐ │
│  │ sei.h: __begin(), __end(), __output_*() macros        │ │
│  └─────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                 TMI (Transactional Memory Interface)        │
│  ┌────────────────┬────────────────┬────────────────────────┐ │
│  │    tmi.c       │   tmi_asm.S    │     tmi_read.S        │ │
│  │ Core TM logic  │ Assembly TM    │  Assembly read ops    │ │
│  │ ITM interface  │ operations     │   (optional)          │ │
│  └────────────────┴────────────────┴────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                    Execution Modes                          │
│  ┌────────────────┬────────────────┬────────────────────────┐ │
│  │  mode_cow.c    │  mode_heap.c   │    mode_instr.c       │ │
│  │Copy-on-Write   │  Dual-Heap     │  Instrumentation      │ │
│  │ (default)      │   mode         │     only             │ │
│  └────────────────┴────────────────┴────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                  Memory Management                          │
│  ┌────────────────┬────────────────┬────────────────────────┐ │
│  │    cow.c       │    heap.c      │      talloc.c         │ │
│  │ COW operations │ Heap allocator │  Transaction alloc    │ │
│  └────────────────┴────────────────┴────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                   Buffer Management                         │
│  ┌────────────────┬────────────────┬────────────────────────┐ │
│  │    abuf.c      │    obuf.c      │      ibuf.c           │ │
│  │ Append buffers │ Output buffers │  Input buffers        │ │
│  └────────────────┴────────────────┴────────────────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                 Utility & Support                           │
│  ┌────────────────┬────────────────┬────────────────────────┐ │
│  │   support.c    │     crc.c      │      libc/            │ │
│  │ System calls   │ CRC calculation│  Custom libc impl     │ │
│  │ compatibility  │                │  (compatibility)      │ │
│  └────────────────┴────────────────┴────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
           ▲                          ▲
           │                          │
    ┌─────────────┐          ┌─────────────────┐
    │   GCC TM    │          │    glibc/       │
    │  (libitm)   │          │   eglibc        │
    │   Runtime   │          │  System calls   │
    └─────────────┘          └─────────────────┘
```

### 1.2 技術スタック

- **言語**: C (GNU C89 standard)
- **コンパイラ**: GCC 11.4.0 (移植後), GCC 4.7+ (元要件)
- **トランザクショナルメモリ**: 
  - GCC Transactional Memory (`-fgnu-tm`)
  - Intel TM ABI specification (Revision 1.1)
  - libitm (GCC TM runtime library)
- **プラットフォーム**: x86_64 Linux, SSE 4.2
- **ビルドシステム**: Makefile-based
- **依存関係**: 
  - glibc 2.35 (移植後), eglibc 2.19 (元環境)
  - pthread (スレッド管理)
  - libdl (動的ロード)

## 2. コンポーネント設計

### 2.1 コンポーネント一覧

| コンポーネント名 | 責務 | 依存関係 |
|---|---|---|
| **Public API** | ユーザー向けマクロAPI提供 | TMI Layer |
| **TMI Core** | トランザクショナルメモリ中核機能 | GCC TM, Execution Modes |
| **COW Mode** | Copy-on-Write実行モード | Memory Management, Buffer Management |
| **Heap Mode** | デュアルヒープ実行モード | Memory Management |
| **Memory Management** | メモリ操作とトランザクション制御 | System Calls |
| **Buffer Management** | 入出力バッファ管理 | CRC, Memory Management |
| **Support Layer** | システム互換性とユーティリティ | glibc/eglibc |

### 2.2 各コンポーネントの詳細

#### 2.2.1 Public API Layer (`include/sei.h`)

- **目的**: ユーザーアプリケーションへのシンプルなマクロAPI提供
- **公開インターフェース**:
  ```c
  // Event handler boundaries
  #define __begin(ptr, size, crc)    // Start transaction with input validation
  #define __end()                    // End transaction
  #define __begin_nm()               // Start without message
  #define __begin_rw(ptr, size, crc) // Start with read-write input
  
  // Output management
  #define __output_append(ptr, size) // Add data to output CRC
  #define __output_done()            // Finalize output message
  #define __crc_pop()                // Retrieve output CRC
  ```
- **内部実装方針**: TMIレイヤーへの薄いマクロラッパー

#### 2.2.2 TMI (Transactional Memory Interface) (`src/tmi.c`, `src/tmi_asm.S`)

- **目的**: GCC TMとの統合とトランザクション制御
- **公開インターフェース**:
  ```c
  // Core transaction management
  int __tmi_prepare(const void* ptr, size_t size, uint32_t crc, int readonly);
  void __tmi_prepare_nm(void);
  int __tmi_begin(int x);
  void __tmi_end(int x);
  
  // Output management
  void __tmi_output_append(const void* ptr, size_t size);
  void __tmi_output_done(void);
  uint32_t __tmi_output_next(void);
  
  // Memory operations (ITM integration)
  void* _ITM_malloc(size_t size);
  void _ITM_free(void* ptr);
  uint8_t _ITM_RU1(const uint8_t* addr);
  void _ITM_WU1(uint8_t* addr, uint8_t value);
  // ... other ITM_* functions
  ```
- **内部実装方針**: 
  - GCC TMのITMインターフェースとの統合
  - setjmp/longjmp機構による冗長実行
  - アセンブリ最適化 (tmi_asm.S, tmi_read.S)

#### 2.2.3 Execution Modes

##### COW Mode (`src/mode_cow.c`)
- **目的**: Copy-on-Write方式による状態保護
- **公開インターフェース**:
  ```c
  // Core mode operations
  int sei_init(void);
  void sei_fini(void);
  int sei_begin(void);
  void sei_commit(void);
  
  // Memory operations
  void* sei_malloc(sei_t *sei, size_t size);
  void sei_free(sei_t *sei, void* ptr);
  uint8_t sei_read_uint8_t(sei_t *sei, const uint8_t* addr);
  void sei_write_uint8_t(sei_t *sei, uint8_t* addr, uint8_t value);
  ```

##### Heap Mode (`src/mode_heap.c`)
- **目的**: デュアルヒープ方式による状態保護
- **類似インターフェース**: COW Modeと共通、実装が異なる

#### 2.2.4 Memory Management

##### COW Implementation (`src/cow.c`)
- **目的**: Copy-on-Write操作の実装
- **公開インターフェース**:
  ```c
  // COW operations
  cow_t* cow_init(size_t size);
  void cow_fini(cow_t* cow);
  void cow_apply(cow_t* cow);
  void cow_swap(cow_t* cow);
  
  // Type-specific read/write
  uint8_t cow_read_uint8_t(cow_t* cow, const uint8_t* addr);
  void cow_write_uint8_t(cow_t* cow, uint8_t* addr, uint8_t value);
  ```

##### Transaction Allocator (`src/talloc.c`)
- **目的**: トランザクション対応メモリアロケータ
- **公開インターフェース**:
  ```c
  void* talloc_malloc(size_t size);
  void talloc_free(void* ptr);
  void talloc_commit(void);
  void talloc_abort(void);
  ```

#### 2.2.5 Buffer Management

##### Append Buffer (`src/abuf.c`)
- **目的**: 追記専用バッファ管理
- **公開インターフェース**:
  ```c
  abuf_t* abuf_init(size_t size);
  void abuf_append(abuf_t* buf, const void* data, size_t len);
  void abuf_reset(abuf_t* buf);
  ```

##### Output Buffer (`src/obuf.c`)
- **目的**: 出力バッファとCRC管理
- **公開インターフェース**:
  ```c
  obuf_t* obuf_init(void);
  void obuf_append(obuf_t* buf, const void* data, size_t len);
  uint32_t obuf_finalize(obuf_t* buf);
  ```

#### 2.2.6 Support Layer (`src/support.c`, `include/sei/support.h`)

- **目的**: システム関数の互換性ラッパー
- **問題となる関数宣言**:
  ```c
  // 現在の宣言 (eglibc 2.19 互換)
  SEI_DECL(long, strtol, (const char *nptr, char **endptr, int base))
  SEI_DECL(void*, realloc, (void* ptr, size_t size))
  
  // glibc 2.35での期待される宣言
  long strtol(const char *restrict nptr, char **restrict endptr, int base);
  void *realloc(void *ptr, size_t size);
  ```

## 3. 互換性問題の詳細分析

### 3.1 主要な互換性問題

#### 3.1.1 関数宣言の競合 (`src/support.c`)

**問題**: glibc 2.35での`restrict`修飾子追加による型不整合

**影響範囲**:
- `strtol`, `strtoll`, `strtoul`, `strtoull`: `restrict`修飾子欠如
- `realloc`: 型シグネチャ完全一致必要
- `memmove_bsd`, `strdup`, `strncpy`, `strcpy`: 宣言重複

**解決アプローチ**:
1. **条件付きコンパイル**: glibc版に応じた宣言切り替え
2. **マクロラッパー**: SEI_DECL マクロの修正
3. **extern宣言の回避**: 必要最小限の宣言のみ保持

#### 3.1.2 Transactional Memory警告 (`src/tmi.c`)

**問題**: 静的変数の非静的インライン関数からのアクセス

**影響範囲**:
- `_ITM_WU1`, `_ITM_WU2`, `_ITM_WU4`, `_ITM_WU8`系関数
- `__sei_thread` 静的変数へのアクセス
- `ignore_addr` 静的関数の呼び出し

**解決アプローチ**:
1. **static inline → inline**: 関数宣言の修正
2. **外部リンケージ化**: 必要に応じてstatic修飾子削除
3. **マクロ展開**: インライン関数をマクロに変換

#### 3.1.3 アセンブリ互換性 (`src/tmi_asm.S`, `src/tmi_read.S`)

**問題**: GCC アセンブラ構文の進化

**影響範囲**:
- アセンブリディレクティブの変更
- レジスタ制約の厳密化
- SSE/AVX命令セットの拡張

**解決アプローチ**:
1. **構文アップデート**: 現代的なアセンブリ構文への変更
2. **条件付きアセンブリ**: GCCバージョンに応じたコード分岐
3. **C実装フォールバック**: アセンブリ問題時のC実装提供

## 4. 移植戦略

### 4.1 段階的移植プロセス

#### Phase 1: 基本コンパイル成功
1. **関数宣言修正**: support.h/support.cの互換性対応
2. **インライン関数警告解決**: tmi.cの静的関数問題修正
3. **基本ビルド成功**: `make`コマンド成功

#### Phase 2: 機能検証
1. **単体テスト実行**: `make test`でのテスト成功
2. **TM機能検証**: 基本的なトランザクション動作確認
3. **サンプル動作**: examples/simpleの実行成功

#### Phase 3: 完全互換性
1. **全警告解決**: コンパイル時警告の最小化
2. **パフォーマンス検証**: 元実装との性能比較
3. **ストレステスト**: 高負荷での安定性確認

### 4.2 リスク軽減策

#### 4.2.1 後方互換性保証
- **API不変**: 公開APIの変更禁止
- **設定可能**: 古い環境でもビルド可能な条件付きコンパイル
- **フォールバック**: 新機能使用不可時の代替実装

#### 4.2.2 検証戦略
- **回帰テスト**: 既存テストスイートの完全通過
- **クロス環境テスト**: Ubuntu 14.04/22.04での動作確認
- **性能ベンチマーク**: トランザクション性能の測定

## 5. APIインターフェース

### 5.1 外部API (libsei使用者向け)

```c
// Event handler integration
if (__begin(input_msg, msg_len, input_crc)) {
    // Process message
    process_application_logic(input_msg);
    
    // Generate output
    char* output = create_output(&output_len);
    __output_append(output, output_len);
    __output_done();
    
    __end();
    
    // Send output with CRC
    send_output(output, output_len, __crc_pop());
}
```

### 5.2 内部API (コンポーネント間)

```c
// TMI ↔ Mode interface
struct sei_interface {
    int (*init)(void);
    void (*fini)(void);
    int (*begin)(void);
    void (*commit)(void);
    void* (*malloc)(sei_t* sei, size_t size);
    void (*free)(sei_t* sei, void* ptr);
};
```

## 6. エラーハンドリング

### 6.1 エラー分類

- **入力検証エラー**: CRC不一致、不正な入力サイズ
- **トランザクションエラー**: TM abort、リソース不足
- **システムエラー**: メモリ不足、システムコール失敗
- **互換性エラー**: サポートされていない環境での実行

### 6.2 エラー通知

- **戻り値**: 関数戻り値による成功/失敗通知
- **ロギング**: DEBUGレベルに応じた詳細ログ出力
- **グレースフル動作**: エラー時の安全な状態復帰

## 7. パフォーマンス設計

### 7.1 最適化ポイント

- **アセンブリ最適化**: 頻繁な読み書きでのアセンブリ使用
- **インライン展開**: 小さな関数のインライン化
- **メモリ効率**: COWの効率的な実装
- **libitm活用**: GCC TMランタイムの最適アルゴリズム選択

### 7.2 性能要件

- **レイテンシ**: 元実装の±10%以内
- **スループット**: 単位時間当たりの処理性能維持
- **メモリ使用量**: メモリオーバーヘッドの最小化

## 8. 実装上の注意事項

### 8.1 GCC Transactional Memory制約

- **実験的機能**: `-fgnu-tm`の実験的性質に注意
- **アーキテクチャ依存**: x86_64以外での動作未保証
- **libitm依存**: GCC TMランタイムライブラリとの密結合

### 8.2 互換性維持

- **条件付きコンパイル**: マクロを使用した環境依存コード分離
- **最小限変更**: 既存ロジックへの影響最小化
- **テスト駆動**: 各変更のテストによる検証

### 8.3 保守性確保

- **コメント追加**: 変更理由と制約の明確な文書化
- **バージョン管理**: 段階的な変更履歴の記録
- **将来拡張**: 次期GCCバージョンへの対応準備

## 9. 次期フェーズ: テスト設計

詳細なテスト設計については、`/test-design`コマンドを実行してテスト設計書を作成してください。

テスト設計書では以下の内容を定義します：

- **単体テスト**: 各コンポーネントの機能検証
- **統合テスト**: TMI-Mode間の連携テスト
- **互換性テスト**: Ubuntu 14.04/22.04での動作検証
- **パフォーマンステスト**: トランザクション性能測定
- **回帰テスト**: 既存機能の非回帰確認
- **自動化戦略**: CI/CDでの継続的テスト