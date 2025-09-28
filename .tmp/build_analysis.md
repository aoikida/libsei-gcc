# ビルド分析結果 - Task 1.1

## 環境情報

### システム環境
- **OS**: Ubuntu 22.04 LTS
- **GCC**: 11.4.0 (Ubuntu 11.4.0-1ubuntu1~22.04.2)  
- **glibc**: 2.35 (Ubuntu GLIBC 2.35-0ubuntu3.10)
- **アーキテクチャ**: x86_64

### 作業ブランチ
- **ブランチ名**: ubuntu-22.04-migration
- **ベースブランチ**: master
- **状態**: 作成完了、チェックアウト済み

## ビルドエラー分析

### 1. 主要エラー: 関数宣言の競合 (src/support.c)

#### 1.1 strtol系関数の`restrict`修飾子問題
```
error: conflicting types for 'strtol'; have 'long int(const char *, char **, int)'
note: previous declaration of 'strtol' with type 'long int(const char * restrict,  char ** restrict,  int)'
```

**影響関数**:
- `strtol`, `strtoll`, `strtoul`, `strtoull`

#### 1.2 関数実装の重複宣言
```
error: conflicting types for 'realloc'; have 'void *(void *, size_t)'
error: conflicting types for 'memmove_bsd'; have 'void *(void *, const void *, size_t)'
error: conflicting types for 'strdup'; have 'char *(const char *)'
error: conflicting types for 'strncpy'; have 'char *(char *, const char *, size_t)'
error: conflicting types for 'strcpy'; have 'char *(char *, const char *)'
error: conflicting types for 'strndup'; have 'char *(const char *, size_t)'
```

### 2. 警告: Transactional Memory関連 (src/tmi.c)

#### 2.1 静的変数アクセス警告
```
warning: '__sei_thread' is static but used in inline function '_ITM_WU8' which is not static
warning: 'ignore_addr' is static but used in inline function '_ITM_WU8' which is not static
```

**影響関数群**:
- `_ITM_WU1`, `_ITM_WU2`, `_ITM_WU4`, `_ITM_WU8`
- `_ITM_WaRU1`, `_ITM_WaRU2`, `_ITM_WaRU4`, `_ITM_WaRU8`
- `_ITM_WaWU1`, `_ITM_WaWU2`, `_ITM_WaWU4`, `_ITM_WaWU8`

**警告の数**: 約36個（1つ当たり3つのパターン × 12関数）

### 3. 依存関係確認

#### 3.1 libitm (GCC Transactional Memory Runtime)
- **ライブラリファイル**: 確認済み
  - `/usr/lib/gcc/x86_64-linux-gnu/11/libitm.a` (静的ライブラリ)
  - `/usr/lib/gcc/x86_64-linux-gnu/11/libitm.so` (共有ライブラリ)
  - `/usr/lib/x86_64-linux-gnu/libitm.so.1.0.0` (ランタイム)
- **pkg-config**: 未対応（GCCの内部ライブラリのため）
- **状態**: 利用可能、ビルドで自動リンク

#### 3.2 ビルドフラグ
- **TM フラグ**: `-fgnu-tm` 適用済み（support.c コンパイル時）
- **最適化**: `-O3` 適用済み
- **警告レベル**: `-Wall` 適用済み
- **セキュリティ**: `-U_FORTIFY_SOURCE -fno-stack-protector` で無効化済み

## エラー分類

### 高優先度 (Phase 2で解決必須)
1. **support.c の関数宣言競合**: コンパイルエラーを引き起こすため最優先
2. **strtol系のrestrict修飾子**: glibc 2.35との互換性問題

### 中優先度 (Phase 2で解決推奨)
1. **TM警告**: ビルドは成功するが大量の警告発生
2. **静的inline関数**: コードの保守性に影響

### 低優先度 (Phase 3以降)
1. **アセンブリ互換性**: 現時点で問題なし
2. **最適化**: 現時点で問題なし

## 次のアクション

### Task 1.2への移行準備完了
- 関数宣言の具体的差分確認が必要
- `/usr/include/stdlib.h`, `/usr/include/string.h`の詳細調査

### Task 1.3への移行準備完了
- `src/tmi.c`の静的変数アクセス問題の詳細調査
- インライン関数とstatic変数の組み合わせ問題の解決策検討

## 成功基準達成状況

- [x] 作業ブランチの作成とバックアップ
- [x] 現在のビルドエラーの詳細記録 
- [x] GCC/glibc バージョン情報の確認と記録
- [x] 依存関係（libitm等）の確認
- [x] 全エラーが分類・記録され、作業環境が準備完了

**完了条件**: ✅ 達成済み - Task 1.1 完了