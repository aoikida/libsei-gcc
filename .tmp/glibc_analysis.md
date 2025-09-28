# glibc関数宣言差分分析 - Task 1.2

## 宣言比較分析

### 1. strtol系関数の`restrict`修飾子問題

#### 1.1 glibc 2.35の実際の宣言 (/usr/include/stdlib.h)
```c
// Line 177-179
extern long int strtol (const char *__restrict __nptr,
                       char **__restrict __endptr, int __base)
     __THROW __nonnull ((1));

// Line 181-183  
extern unsigned long int strtoul (const char *__restrict __nptr,
                                 char **__restrict __endptr, int __base)
     __THROW __nonnull ((1));

// Line 201-203
extern long long int strtoll (const char *__restrict __nptr,
                             char **__restrict __endptr, int __base)
     __THROW __nonnull ((1));

// Line 206-208
extern unsigned long long int strtoull (const char *__restrict __nptr,
                                       char **__restrict __endptr, int __base)
     __THROW __nonnull ((1));
```

#### 1.2 libseiの現在の宣言 (include/sei/support.h)
```c
// Line 40-44
SEI_DECL(long,  strtol,  (const char *nptr, char **endptr, int base))
SEI_DECL(long long, strtoll, (const char *nptr, char **endptr, int base))
SEI_DECL(unsigned long, strtoul, (const char *nptr, char **endptr, int base))
SEI_DECL(unsigned long long, strtoull,
           (const char *nptr, char **endptr, int base))
```

#### 1.3 SEI_DECLマクロの展開
```c
// SEI_DECLマクロ定義 (Line 23)
#define SEI_DECL(RETURN, FUNC, ARGS) SEI_PREF RETURN FUNC ARGS SEI_ATTR;

// SEI_PREF定義 (Line 17-21)
#ifdef TMI_IMPL
#define SEI_PREF
#else  
#define SEI_PREF extern
#endif

// SEI_ATTR定義 (Line 10-11)
#define SEI_ATTR __attribute__((transaction_safe))

// 展開結果例 (strtol)
extern long strtol (const char *nptr, char **endptr, int base) __attribute__((transaction_safe));
```

#### 1.4 問題の本質
- **glibc 2.35**: `const char *__restrict` および `char **__restrict` 
- **libsei**: `const char *` および `char **` (restrict修飾子なし)
- **コンパイラエラー**: 型シグネチャの不一致により関数再宣言エラー

### 2. string関数の宣言問題

#### 2.1 strcpy/strncpyの宣言 (/usr/include/string.h)
```c
// Line 141-142
extern char *strcpy (char *__restrict __dest, const char *__restrict __src)
     __THROW __nonnull ((1, 2));

// Line 144-146  
extern char *strncpy (char *__restrict __dest,
                      const char *__restrict __src, size_t __n)
     __THROW __nonnull ((1, 2));
```

#### 2.2 strdupの宣言 (/usr/include/string.h)
```c
// Line 187-188 (条件付き)
extern char *strdup (const char *__s)
     __THROW __attribute_malloc__ __nonnull ((1));
```

#### 2.3 libseiの宣言 (include/sei/support.h)
```c
// Line 45-48
SEI_DECL(char*, strdup, (const char *s))
SEI_DECL(char*, strndup, (const char *s, size_t n))  
SEI_DECL(char*, strcpy, (char *dest, const char *src))
SEI_DECL(char*, strncpy, (char *dest, const char *src, size_t n))
```

### 3. reallocの宣言問題

#### 3.1 glibc 2.35の宣言 (/usr/include/stdlib.h)
```c
// Line 551-552
extern void *realloc (void *__ptr, size_t __size)
     __THROW __attribute_warn_unused_result__ __attribute_alloc_size__ ((2));
```

#### 3.2 libseiの宣言 (include/sei/support.h)
```c
// Line 53
SEI_DECL(void*, realloc, (void* ptr, size_t size))
```

## 修正方針決定

### 4. 最適な解決策: restrict修飾子対応

#### 4.1 推奨アプローチ
```c
// include/sei/support.h に追加
#ifdef __GLIBC__
#define SEI_RESTRICT __restrict
#else
#define SEI_RESTRICT
#endif

// 修正された宣言
SEI_DECL(long, strtol, (const char *SEI_RESTRICT nptr, char **SEI_RESTRICT endptr, int base))
SEI_DECL(long long, strtoll, (const char *SEI_RESTRICT nptr, char **SEI_RESTRICT endptr, int base))
SEI_DECL(unsigned long, strtoul, (const char *SEI_RESTRICT nptr, char **SEI_RESTRICT endptr, int base))
SEI_DECL(unsigned long long, strtoull, (const char *SEI_RESTRICT nptr, char **SEI_RESTRICT endptr, int base))
SEI_DECL(char*, strcpy, (char *SEI_RESTRICT dest, const char *SEI_RESTRICT src))
SEI_DECL(char*, strncpy, (char *SEI_RESTRICT dest, const char *SEI_RESTRICT src, size_t n))
```

## 完了状況

- [x] `/usr/include/stdlib.h`でのstrtol系関数の実際の宣言確認
- [x] `/usr/include/string.h`でのstring系関数の実際の宣言確認  
- [x] restrict修飾子の使用パターン分析
- [x] libseiでのSEI_DECLマクロ分析
- [x] 修正方針の決定

**Task 1.2 完了**: restrict修飾子問題の解決策が確定