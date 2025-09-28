# Task 2.1 完了報告 - support.h関数宣言の修正

## 実施内容

### 1. SEI_RESTRICTマクロの追加
```c
/* glibc restrict keyword compatibility */
#ifdef __GLIBC__
#define SEI_RESTRICT __restrict
#else
#define SEI_RESTRICT
#endif
```

### 2. strtol系関数のrestrict修飾子対応
修正前:
```c
SEI_DECL(long,  strtol,  (const char *nptr, char **endptr, int base))
SEI_DECL(long long, strtoll, (const char *nptr, char **endptr, int base))
SEI_DECL(unsigned long, strtoul, (const char *nptr, char **endptr, int base))
SEI_DECL(unsigned long long, strtoull, (const char *nptr, char **endptr, int base))
```

修正後:
```c
SEI_DECL(long,  strtol,  (const char *SEI_RESTRICT nptr, char **SEI_RESTRICT endptr, int base))
SEI_DECL(long long, strtoll, (const char *SEI_RESTRICT nptr, char **SEI_RESTRICT endptr, int base))
SEI_DECL(unsigned long, strtoul, (const char *SEI_RESTRICT nptr, char **SEI_RESTRICT endptr, int base))
SEI_DECL(unsigned long long, strtoull, (const char *SEI_RESTRICT nptr, char **SEI_RESTRICT endptr, int base))
```

### 3. string関数のrestrict修飾子対応
修正前:
```c
SEI_DECL(char*, strcpy, (char *dest, const char *src))
SEI_DECL(char*, strncpy, (char *dest, const char *src, size_t n))
```

修正後:
```c
SEI_DECL(char*, strcpy, (char *SEI_RESTRICT dest, const char *SEI_RESTRICT src))
SEI_DECL(char*, strncpy, (char *SEI_RESTRICT dest, const char *SEI_RESTRICT src, size_t n))
```

## 結果

### 成功要素
1. **条件付きrestrict対応**: `__GLIBC__`マクロによりglibc環境でのみrestrict修飾子が追加される
2. **後方互換性**: glibc以外の環境では従来通りの宣言を維持
3. **型整合性**: glibc 2.35の関数宣言と完全に一致

### 検証結果
- **glibc検出**: `__GLIBC__` = 2, `__GLIBC_MINOR__` = 35 を確認
- **マクロ展開**: SEI_RESTRICTが`__restrict`に正しく展開
- **コンパイル**: support.hの宣言段階でのエラーは解消

### 残存問題（Task 2.2で対応）
- support.cでの重複宣言エラーが依然として発生
- これは実装ファイル側の問題であり、Task 2.1の範囲外

## Task 2.1完了条件の達成状況

- [x] `include/sei/support.h`でのstrtol系関数宣言をrestrict対応に修正
- [x] SEI_DECLマクロの条件付きコンパイル対応検討・実装
- [x] glibc版判定マクロの追加
- [x] 修正後のコンパイル確認（宣言部分）

**Task 2.1 完了**: support.hの関数宣言がglibc 2.35と互換性を達成