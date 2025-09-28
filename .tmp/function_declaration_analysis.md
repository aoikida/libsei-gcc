# 関数宣言差分分析レポート

## 主要な宣言競合

### 1. strtol系関数の差分

#### glibc 2.35の実際の宣言 (/usr/include/stdlib.h)
```c
extern unsigned long long int strtoull (const char *__restrict __nptr,
                                       char **__restrict __endptr, int __base)
```

#### libseiの現在の宣言 (include/sei/support.h:44-45)
```c
SEI_DECL(unsigned long long, strtoull,
         (const char *nptr, char **endptr, int base))
```

**問題**: `restrict` 修飾子の欠如

### 2. string系関数の差分

#### strdup - glibc 2.35
```c
extern char *strdup (const char *__s)
```

#### strndup - glibc 2.35  
```c
extern char *strndup (const char *__string, size_t __n)
```

#### strcpy - glibc 2.35
```c
extern char *strcpy (char *__restrict __dest, const char *__restrict __src)
```

#### strncpy - glibc 2.35
```c
extern char *strncpy (char *__restrict __dest, const char *__restrict __src, size_t __n)
```

#### libseiの現在の宣言
```c
SEI_DECL(char*, strdup, (const char *s))          // line 46
SEI_DECL(char*, strndup, (const char *s, size_t n))  // line 47  
SEI_DECL(char*, strcpy, (char *dest, const char *src))  // line 48
SEI_DECL(char*, strncpy, (char *dest, const char *src, size_t n))  // line 49
```

**問題**: strcpy/strncpyで `restrict` 修飾子の欠如

### 3. realloc関数

#### glibc 2.35
```c
extern void *realloc (void *__ptr, size_t __size)
```

#### libsei
```c
SEI_DECL(void*, realloc, (void* ptr, size_t size))  // line 54
```

**問題**: 機能的には同一だが、複数の宣言が競合

## 詳細な差分表

| 関数 | glibc 2.35 | libsei | 差分 |
|------|------------|--------|------|
| strtol | `const char *__restrict __nptr, char **__restrict __endptr` | `const char *nptr, char **endptr` | restrict欠如 |
| strtoll | `const char *__restrict __nptr, char **__restrict __endptr` | `const char *nptr, char **endptr` | restrict欠如 |
| strtoull | `const char *__restrict __nptr, char **__restrict __endptr` | `const char *nptr, char **endptr` | restrict欠如 |
| strcpy | `char *__restrict __dest, const char *__restrict __src` | `char *dest, const char *src` | restrict欠如 |
| strncpy | `char *__restrict __dest, const char *__restrict __src` | `char *dest, const char *src` | restrict欠如 |
| strdup | `const char *__s` | `const char *s` | パラメータ名のみ |
| strndup | `const char *__string, size_t __n` | `const char *s, size_t n` | パラメータ名のみ |
| realloc | `void *__ptr, size_t __size` | `void* ptr, size_t size` | 宣言重複問題 |

## restrict修飾子について

### glibc 2.35での使用パターン
- **文字列操作関数**: src/destが重複しない場合にrestrict使用
- **変換関数**: 入力文字列と出力ポインタが重複しない場合にrestrict使用
- **目的**: コンパイラ最適化とポインタエイリアシング制御

### libseiでの影響
- **互換性**: restrict修飾子がないと型不一致エラー
- **機能性**: libseiは独自実装を提供するため、パラメータセマンティクスは保持可能
- **解決策**: 条件付きrestrict修飾子の追加

## 修正方針

### 推奨アプローチ
1. **restrict対応版への更新**: glibc 2.35との完全互換
2. **条件付きコンパイル**: 古いglibcとの後方互換性維持
3. **段階的修正**: 関数グループ別の個別修正

### 次のステップ
- Task 2.1: support.h関数宣言の修正
- Task 2.2: support.c実装との整合性確認