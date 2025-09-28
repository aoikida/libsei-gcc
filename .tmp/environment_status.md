# Ubuntu 22.04移植 - 環境状況レポート

## 実行環境
- **OS**: Linux (Ubuntu 22.04)
- **GCC**: 11.4.0-1ubuntu1~22.04.2 (Ubuntu 11.4.0)
- **glibc**: 2.35-0ubuntu3.10 (Ubuntu GLIBC 2.35)
- **libitm**: 12.3.0-1ubuntu1~22.04.2 (GNU Transactional Memory Library)
- **ブランチ**: ubuntu-22.04-port (master からの分岐)

## 主要ビルドエラー

### 1. TM警告（数百個）
```
warning: '__sei_thread' is static but used in inline function '_ITM_WaWU8' which is not static
warning: 'ignore_addr' is static but used in inline function '_ITM_WaWU8' which is not static
```
**場所**: `src/tmi.c:493`, `src/tmi.c:484`
**原因**: static変数/関数がinline関数から参照されているGCC 11.4の警告強化

### 2. 関数宣言競合エラー（多数）
**主要エラー**:
- `strtol`: `restrict`修飾子の不一致
- `strtoll`: `restrict`修飾子の不一致  
- `strtoull`: `restrict`修飾子の不一致
- `realloc`: 宣言の重複
- `memmove_bsd`: 型定義の競合
- `strdup`, `strncpy`, `strcpy`, `strndup`: 重複宣言

**場所**: `include/sei/support.h` vs システムヘッダー
**原因**: glibc 2.35でのrestrict修飾子導入による型不一致

## エラー分析

### TM警告の詳細
- **対象関数**: `_ITM_W*` series (約24関数 × 複数型 = 数百警告)
- **問題変数**: `__sei_thread` (static)
- **問題関数**: `ignore_addr` (static)
- **根本原因**: inline関数から静的スコープへのアクセス

### 関数宣言競合の詳細  
- **glibc 2.35変更**: `restrict`修飾子の標準追加
- **libsei宣言**: 旧形式（restrict無し）
- **競合箇所**: `SEI_DECL`マクロによる宣言

## ビルド設定
```makefile
MODE  : cow
ALGO  : sbuf  
DEBUG : (release)
CFLAGS: -msse4.2 -g -O3 -Wall -DNDEBUG -Iinclude -U_FORTIFY_SOURCE -fno-stack-protector
```

## 依存関係状況
- ✅ libitm1 正常インストール済み
- ✅ アセンブリファイル (.S) コンパイル成功
- ❌ C実装ファイル (.c) 宣言競合で失敗

## 修正優先度
1. **高**: 関数宣言競合（ビルド阻害）
2. **高**: TM警告（数が多く、実用性に影響）
3. **中**: ビルドシステム調整
4. **低**: パフォーマンス検証

## 次のアクション
Task 1.2: 関数宣言差分の詳細分析開始準備完了