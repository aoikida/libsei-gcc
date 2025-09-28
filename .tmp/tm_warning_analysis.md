# TM警告の根本原因分析レポート

## 問題の概要
GCC 11.4でのビルド時に数百個のTM警告が発生。static要素がinline関数から参照されることが原因。

## 根本原因
1. **static変数**: `__sei_thread` (スレッド状態管理)
2. **static関数**: `ignore_addr` (アドレス無視判定)  
3. **inline関数**: `ITM_WRITE`マクロで生成される12個の関数

## 推奨修正方針
- `__sei_thread`: extern化
- `ignore_addr`: static削除
- 影響: 300-400個の警告解消