// Common type-check helpers parameterized by accessors, so interpreter/runtime can share logic.
// Callers must define:
//   - TC_TYPE(x): evaluate to integer type tag
//   - TC_IS_STRING(x): non-zero if x is string
//   - TC_IS_ARRAY(x): non-zero if x is array
//   - TC_IS_DICT(x):  non-zero if x is dict
//   - TC_IS_BOOL(x):  non-zero if x is bool
//   - TC_IS_NUMERIC(x): non-zero if int or float
//   - TC_IS_NULL(x): non-zero if null
//   - TC_ERR(ctx_line, ctx_file, fmt, ...) : report error and abort
//   - TC_CTX_LINE, TC_CTX_FILE : current context line/file (can be ints/pointers)
//
// These macros produce no side effects besides evaluating args once.

#ifndef TYPE_CHECK_COMMON_H
#define TYPE_CHECK_COMMON_H

#define TC_REQUIRE_NUMERIC(opname, L, R) \
    do { \
        if (!(TC_IS_NUMERIC(L) && TC_IS_NUMERIC(R))) { \
            TC_ERR(TC_CTX_LINE, TC_CTX_FILE, "Type error: %s requires numbers", (opname)); \
        } \
    } while (0)

#define TC_REQUIRE_STRING_CONCAT(L, R) \
    do { \
        if (!(TC_IS_STRING(L) && TC_IS_STRING(R))) { \
            TC_ERR(TC_CTX_LINE, TC_CTX_FILE, "Type error: string concatenation requires two strings"); \
        } \
    } while (0)

#define TC_REQUIRE_IN_RIGHT(R) \
    do { \
        if (!(TC_IS_ARRAY(R) || TC_IS_DICT(R) || TC_IS_STRING(R))) { \
            TC_ERR(TC_CTX_LINE, TC_CTX_FILE, "IN operator requires array, dict, or string on the right side"); \
        } \
    } while (0)

#define TC_REQUIRE_DICT_KEY_STRING(L) \
    do { \
        if (!TC_IS_STRING(L)) { \
            TC_ERR(TC_CTX_LINE, TC_CTX_FILE, "IN operator requires string key for dict"); \
        } \
    } while (0)

#define TC_REQUIRE_STRING_SUBSTRING(L) \
    do { \
        if (!TC_IS_STRING(L)) { \
            TC_ERR(TC_CTX_LINE, TC_CTX_FILE, "Can only check if string is in string"); \
        } \
    } while (0)

#define TC_COMPARE_GUARD(L, R) \
    do { \
        int l_type = TC_TYPE(L); \
        int r_type = TC_TYPE(R); \
        int ok = 0; \
        if (TC_IS_NUMERIC(L) && TC_IS_NUMERIC(R)) ok = 1; \
        else if (l_type == r_type && (TC_IS_STRING(L) || TC_IS_BOOL(L))) ok = 1; \
        if (!ok) { \
            TC_ERR(TC_CTX_LINE, TC_CTX_FILE, "Type error: comparison requires numbers, bools, or strings of same type"); \
        } \
    } while (0)

#endif // TYPE_CHECK_COMMON_H
