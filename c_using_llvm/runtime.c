#include "runtime.h"
#include "gc.h"
#include <regex.h>
#include <math.h>
#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include "type_check_common.h"
#include "gc.h"

// Global storage for command line arguments
static int g_argc = 0;
static char **g_argv = NULL;
static jmp_buf try_stack[256];
static int try_top = 0;
static Value current_exception = {TYPE_NULL, 0};
static int current_err_line = 0;
static const char *current_err_file = NULL;
static double value_to_double(Value v);

void set_source_ctx(int line, const char *file) {
    current_err_line = line;
    current_err_file = file;
}

static __attribute__((noreturn)) void type_error_ctx(int line, const char *file, const char *fmt, ...) {
    fprintf(stderr, "Error");
    if (file) fprintf(stderr, " at %s", file);
    else if (current_err_file) fprintf(stderr, " at %s", current_err_file);
    if (line > 0) fprintf(stderr, ":%d", line);
    else if (current_err_line > 0) fprintf(stderr, ":%d", current_err_line);
    fprintf(stderr, ": ");
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

static __attribute__((noreturn)) void type_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "Error");
    if (current_err_file) fprintf(stderr, " at %s", current_err_file);
    if (current_err_line > 0) fprintf(stderr, ":%d", current_err_line);
    fprintf(stderr, ": ");
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

// Shared type-check accessor mappings for type_check_common.h
#define TC_TYPE(v) ((v).type)
#define TC_IS_STRING(v) ((v).type == TYPE_STRING)
#define TC_IS_ARRAY(v) ((v).type == TYPE_ARRAY)
#define TC_IS_DICT(v) ((v).type == TYPE_DICT)
#define TC_IS_BOOL(v) ((v).type == TYPE_BOOL)
#define TC_IS_NUMERIC(v) ((v).type == TYPE_INT || (v).type == TYPE_FLOAT)
#define TC_IS_NULL(v) ((v).type == TYPE_NULL)
#define TC_ERR(ctx_line, ctx_file, fmt, ...) type_error_ctx((ctx_line), (ctx_file), (fmt), ##__VA_ARGS__)
#define TC_CTX_LINE (line)
#define TC_CTX_FILE (file)

#define REQUIRE_NUMERIC(opname) TC_REQUIRE_NUMERIC((opname), left, right)
#define REQUIRE_BOTH_STRING() TC_REQUIRE_STRING_CONCAT(left, right)
#define REQUIRE_IN_RIGHT(R) TC_REQUIRE_IN_RIGHT((R))
#define REQUIRE_DICT_KEY_STRING(L) TC_REQUIRE_DICT_KEY_STRING((L))
#define REQUIRE_STRING_SUBSTRING(L) TC_REQUIRE_STRING_SUBSTRING((L))
#define IS_NUMERIC(t) ((t) == TYPE_INT || (t) == TYPE_FLOAT)

// Structures now defined in runtime.h

// Track current method call stack for privacy checks
static Instance *this_stack[256];
static int this_stack_top = 0;

// Helper to create array
static Array* new_array() {
    Array *a = gc_alloc(TYPE_ARRAY, sizeof(Array));
    a->size = 0;
    a->capacity = 8;
    a->data = gc_alloc(TYPE_ARRAY, 8 * sizeof(Value));
    return a;
}

static void push_this(Instance *inst) {
    if (this_stack_top >= 256) return;
    this_stack[this_stack_top++] = inst;
}

static void pop_this() {
    if (this_stack_top > 0) this_stack_top--;
}

static int is_internal_access(Instance *inst) {
    return this_stack_top > 0 && this_stack[this_stack_top - 1] == inst;
}

// Create empty array
Value make_array(void) {
    Array *a = new_array();
    Value result = {TYPE_ARRAY, (long)a};
    return result;
}

Value make_null(void) {
    Value result = {TYPE_NULL, 0};
    return result;
}

// Append value to array
Value append(Value arr, Value val) {
    if (arr.type != TYPE_ARRAY) {
        fprintf(stderr, "append requires array as first argument\n");
        exit(1);
    }
    Array *a = (Array*)(arr.data);
    if (a->size >= a->capacity) {
        // Allocate new buffer with GC
        int new_capacity = a->capacity * 2;
        void *new_data = gc_alloc(TYPE_ARRAY, new_capacity * sizeof(Value));
        // Copy old data
        memcpy(new_data, a->data, a->size * sizeof(Value));
        // Update array (old data will be collected by GC)
        a->data = new_data;
        a->capacity = new_capacity;
    }
    ((Value*)a->data)[a->size++] = val;
    return arr;  // Return the array, not a boolean
}

// Get array element
Value array_get(Value arr, Value index) {
    Array *a = (Array*)(arr.data);
    long idx = index.data;
    if (idx >= 0 && idx < a->size) {
        return ((Value*)a->data)[idx];
    }
    Value result = {TYPE_INT, 0};
    return result;
}

// Generic index access (handles both array and dict)
Value index_get(Value obj, Value index) {
    if (obj.type == TYPE_ARRAY) {
        return array_get(obj, index);
    } else if (obj.type == TYPE_DICT) {
        return dict_get(obj, index);
    } else if (obj.type == TYPE_STRING) {
        // String indexing
        char *s = (char*)(obj.data);
        long idx = index.data;
        long len = strlen(s);
        if (idx < 0) idx += len;
        if (idx >= 0 && idx < len) {
            char c[2] = {s[idx], '\0'};
            char *result_str = strdup(c);
            Value result = {TYPE_STRING, (long)result_str};
            return result;
        }
    }
    Value result = {TYPE_INT, 0};
    return result;
}

// Set array element
Value array_set(Value arr, Value index, Value val) {
    Array *a = (Array*)(arr.data);
    long idx = index.data;
    if (idx >= 0 && idx < a->size) {
        ((Value*)a->data)[idx] = val;
    }
    return val;
}

// Generic index set (handles both array and dict)
Value index_set(Value obj, Value index, Value val) {
    if (obj.type == TYPE_ARRAY) {
        return array_set(obj, index, val);
    } else if (obj.type == TYPE_DICT) {
        return dict_set(obj, index, val);
    } else {
        fprintf(stderr, "Error: Can only assign to array or dict indices\n");
        exit(1);
    }
}

// Get length
Value len(Value v) {
    if (v.type == TYPE_ARRAY) {
        Array *a = (Array*)(v.data);
        Value result = {TYPE_INT, a->size};
        return result;
    } else if (v.type == TYPE_STRING) {
        char *s = (char*)(v.data);
        Value result = {TYPE_INT, strlen(s)};
        return result;
    } else if (v.type == TYPE_DICT) {
        Dict *d = (Dict*)(v.data);
        Value result = {TYPE_INT, d->size};
        return result;
    }
    type_error("len() requires array, string, or dict");
}

// Type conversion functions
Value to_int(Value v) {
    if (v.type == TYPE_INT) return v;
    if (v.type == TYPE_FLOAT) {
        double f = *(double*)&v.data;
        Value result = {TYPE_INT, (long)f};
        return result;
    }
    if (v.type == TYPE_BOOL) {
        Value result = {TYPE_INT, v.data};
        return result;
    }
    if (v.type == TYPE_STRING) {
        char *str = (char*)v.data;
        long val = atol(str);  // Use atol for long conversion
        Value result = {TYPE_INT, val};
        return result;
    }
    type_error("int() requires int/float/bool/string");
}

Value to_float(Value v) {
    if (v.type == TYPE_FLOAT) return v;
    if (v.type == TYPE_INT) {
        double f = (double)v.data;
        Value result = {TYPE_FLOAT, *(long*)&f};
        return result;
    }
    if (v.type == TYPE_BOOL) {
        double f = (double)v.data;
        Value result = {TYPE_FLOAT, *(long*)&f};
        return result;
    }
    if (v.type == TYPE_STRING) {
        char *str = (char*)v.data;
        double f = atof(str);
        Value result = {TYPE_FLOAT, *(long*)&f};
        return result;
    }
    type_error("float() requires int/float/bool/string");
}

Value to_string(Value v) {
    char buf[64];
    if (v.type == TYPE_INT) {
        snprintf(buf, sizeof(buf), "%ld", v.data);
        char *s = strdup(buf);
        Value result = {TYPE_STRING, (long)s};
        return result;
    } else if (v.type == TYPE_FLOAT) {
        double f = *(double*)&v.data;
        snprintf(buf, sizeof(buf), "%g", f);
        char *s = strdup(buf);
        Value result = {TYPE_STRING, (long)s};
        return result;
    } else if (v.type == TYPE_STRING) {
        return v;
    } else if (v.type == TYPE_BOOL) {
        const char *s = v.data ? "true" : "false";
        char *dup = strdup(s);
        Value result = {TYPE_STRING, (long)dup};
        return result;
    } else if (v.type == TYPE_NULL) {
        char *dup = strdup("null");
        Value result = {TYPE_STRING, (long)dup};
        return result;
    }
    type_error("str() requires int/float/string/bool/null");
}

// Convert value to string representation (like str() in Python)
Value str(Value v) {
    return to_string(v);
}

// Get type name as string
Value type(Value v) {
    const char *type_name;
    if (v.type == TYPE_INT) {
        type_name = "int";
    } else if (v.type == TYPE_FLOAT) {
        type_name = "float";
    } else if (v.type == TYPE_STRING) {
        type_name = "string";
    } else if (v.type == TYPE_ARRAY) {
        type_name = "array";
    } else if (v.type == TYPE_DICT) {
        type_name = "dict";
    } else if (v.type == TYPE_BOOL) {
        type_name = "bool";
    } else if (v.type == TYPE_CLASS) {
        type_name = "class";
    } else if (v.type == TYPE_INSTANCE) {
        Instance *inst = (Instance*)v.data;
        type_name = inst && inst->cls && inst->cls->name ? inst->cls->name : "instance";
    } else if (v.type == TYPE_NULL) {
        type_name = "null";
    } else {
        type_name = "unknown";
    }
    char *s = strdup(type_name);
    Value result = {TYPE_STRING, (long)s};
    return result;
}

// Slice array or string
Value slice_access(Value obj, Value start_v, Value end_v) {
    long start = start_v.data;
    long end = end_v.data;

    if (obj.type == TYPE_ARRAY) {
        Array *a = (Array*)(obj.data);
        long size = a->size;

        // Handle negative indices
        if (start < 0) start += size;
        if (end < 0) end += size;

        // Clamp to bounds
        if (start < 0) start = 0;
        if (end > size) end = size;
        if (start > end) start = end;

        // Create new array with slice
        Array *new_a = new_array();
        for (long i = start; i < end; i++) {
            Value elem = ((Value*)a->data)[i];
            if (new_a->size >= new_a->capacity) {
                int old_capacity = new_a->capacity;
                new_a->capacity *= 2;
                new_a->data = gc_realloc(new_a->data, TYPE_ARRAY,
                                         old_capacity * sizeof(Value),
                                         new_a->capacity * sizeof(Value));
            }
            ((Value*)new_a->data)[new_a->size++] = elem;
        }

        Value result = {TYPE_ARRAY, (long)new_a};
        return result;
    } else if (obj.type == TYPE_STRING) {
        char *s = (char*)(obj.data);
        long size = strlen(s);

        // Handle negative indices
        if (start < 0) start += size;
        if (end < 0) end += size;

        // Clamp to bounds
        if (start < 0) start = 0;
        if (end > size) end = size;
        if (start > end) start = end;

        // Create new string with slice
        long len = end - start;
        char *new_s = malloc(len + 1);
        strncpy(new_s, s + start, len);
        new_s[len] = '\0';

        Value result = {TYPE_STRING, (long)new_s};
        return result;
    }

    Value result = {TYPE_STRING, (long)strdup("")};
    return result;
}

// Read input from user
Value input(Value prompt) {
    if (prompt.type == TYPE_STRING) {
        char *s = (char*)(prompt.data);
        printf("%s", s);
        fflush(stdout);
    }

    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len - 1] == '\n') {
            buffer[len - 1] = '\0';
        }
        char *s = strdup(buffer);
        Value result = {TYPE_STRING, (long)s};
        return result;
    }

    Value result = {TYPE_STRING, (long)strdup("")};
    return result;
}

// Read file contents
Value file_read(Value filename) {
    if (filename.type != TYPE_STRING) {
        fprintf(stderr, "file_read requires filename string\n");
        exit(1);
    }
    char *fname = (char*)(filename.data);
    FILE *f = fopen(fname, "rb");

    if (f == NULL) {
        Value result = {TYPE_STRING, (long)strdup("")};
        return result;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    Value result = {TYPE_STRING, (long)content};
    return result;
}

// Write content to file
Value file_write(Value content, Value filename) {
    if (filename.type != TYPE_STRING) {
        fprintf(stderr, "file_write requires filename string\n");
        exit(1);
    }
    char *fname = (char*)(filename.data);
    FILE *f = fopen(fname, "w");

    if (f == NULL) {
        Value result = {TYPE_BOOL, 0};
        return result;
    }

    if (content.type == TYPE_STRING) {
        char *s = (char*)(content.data);
        fprintf(f, "%s", s);
    } else if (content.type == TYPE_INT) {
        fprintf(f, "%ld", content.data);
    } else if (content.type == TYPE_FLOAT) {
        double d = *(double*)&content.data;
        fprintf(f, "%g", d);
    }

    fclose(f);
    Value result = {TYPE_INT, 1};
    return result;
}

Value file_append(Value content, Value filename) {
    if (filename.type != TYPE_STRING) {
        fprintf(stderr, "file_append requires filename string\n");
        exit(1);
    }
    char *fname = (char*)(filename.data);
    FILE *f = fopen(fname, "a");
    if (f == NULL) {
        Value result = {TYPE_BOOL, 0};
        return result;
    }
    if (content.type == TYPE_STRING) {
        fprintf(f, "%s", (char*)content.data);
    } else if (content.type == TYPE_INT) {
        fprintf(f, "%ld", content.data);
    } else if (content.type == TYPE_FLOAT) {
        double d = *(double*)&content.data;
        fprintf(f, "%g", d);
    }
    fclose(f);
    Value result = {TYPE_BOOL, 1};
    return result;
}

Value file_size(Value filename) {
    if (filename.type != TYPE_STRING) {
        fprintf(stderr, "file_size requires filename string\n");
        exit(1);
    }
    char *fname = (char*)(filename.data);
    FILE *f = fopen(fname, "rb");
    if (f == NULL) {
        Value result = {TYPE_INT, 0};
        return result;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    Value result = {TYPE_INT, size};
    return result;
}

Value file_exist(Value filename) {
    if (filename.type != TYPE_STRING) {
        fprintf(stderr, "file_exist requires filename string\n");
        exit(1);
    }
    char *fname = (char*)(filename.data);
    FILE *f = fopen(fname, "rb");
    if (f == NULL) {
        Value result = {TYPE_BOOL, 0};
        return result;
    }
    fclose(f);
    Value result = {TYPE_BOOL, 1};
    return result;
}

// backward-compatible aliases
Value read(Value filename) { return file_read(filename); }
Value write(Value content, Value filename) { return file_write(content, filename); }

// ===== Dict Functions =====

// Hash function for dict keys
static unsigned int hash(const char *key) {
    unsigned int h = 0;
    while (*key) {
        h = h * 31 + *key++;
    }
    return h % HASH_SIZE;
}

// Create empty dict
Value make_dict(void) {
    Dict *d = gc_alloc(TYPE_DICT, sizeof(Dict));
    d->buckets = gc_alloc(TYPE_DICT, HASH_SIZE * sizeof(DictEntry*));
    d->size = 0;

    Value result = {TYPE_DICT, (long)d};
    return result;
}

// Set key-value pair in dict
Value dict_set(Value dict, Value key, Value val) {
    Dict *d = (Dict*)(dict.data);

    // Convert key to string
    char *key_str;
    if (key.type == TYPE_STRING) {
        key_str = (char*)(key.data);
    } else if (key.type == TYPE_INT) {
        // Convert int key to string
        char buf[32];
        snprintf(buf, sizeof(buf), "%ld", key.data);
        key_str = strdup(buf);
    } else {
        // Unsupported key type, return val as-is
        return val;
    }

    unsigned int idx = hash(key_str);
    DictEntry *entry = d->buckets[idx];

    // Check if key already exists
    while (entry != NULL) {
        if (strcmp(entry->key, key_str) == 0) {
            entry->value = val;  // Update existing value
            if (key.type == TYPE_INT) free(key_str);  // Free temp string if int key
            return dict;
        }
        entry = entry->next;
    }

    // Insert new entry
    entry = malloc(sizeof(DictEntry));
    if (key.type == TYPE_STRING) {
        entry->key = strdup(key_str);
    } else {
        entry->key = key_str;  // Already allocated for int keys
    }
    entry->value = val;
    entry->next = d->buckets[idx];
    d->buckets[idx] = entry;
    d->size++;

    return dict;
}

// Get value from dict by key
Value dict_get(Value dict, Value key) {
    Dict *d = (Dict*)(dict.data);

    // Convert key to string
    char *key_str;
    char buf[32];
    if (key.type == TYPE_STRING) {
        key_str = (char*)(key.data);
    } else if (key.type == TYPE_INT) {
        snprintf(buf, sizeof(buf), "%ld", key.data);
        key_str = buf;
    } else {
        // Unsupported key type, return 0
        Value result = {TYPE_INT, 0};
        return result;
    }

    unsigned int idx = hash(key_str);
    DictEntry *entry = d->buckets[idx];

    while (entry != NULL) {
        if (strcmp(entry->key, key_str) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    // Key not found, return 0
    Value result = {TYPE_INT, 0};
    return result;
}

// Check if key exists in dict
Value dict_has(Value dict, Value key) {
    Dict *d = (Dict*)(dict.data);

    // Convert key to string
    char *key_str;
    char buf[32];
    if (key.type == TYPE_STRING) {
        key_str = (char*)(key.data);
    } else if (key.type == TYPE_INT) {
        snprintf(buf, sizeof(buf), "%ld", key.data);
        key_str = buf;
    } else {
        Value result = {TYPE_INT, 0};
        return result;
    }

    unsigned int idx = hash(key_str);
    DictEntry *entry = d->buckets[idx];

    while (entry != NULL) {
        if (strcmp(entry->key, key_str) == 0) {
            Value result = {TYPE_BOOL, 1};  // true
            return result;
        }
        entry = entry->next;
    }

    Value result = {TYPE_BOOL, 0};  // false
    return result;
}

// Get all keys from dict as an array
Value dict_keys(Value dict) {
    Dict *d = (Dict*)(dict.data);

    // Create array to store keys
    Value arr = make_array();

    for (int i = 0; i < HASH_SIZE; i++) {
        DictEntry *entry = d->buckets[i];
        while (entry != NULL) {
            // Create string Value for the key
            char *key_copy = strdup(entry->key);
            Value key_val = {TYPE_STRING, (long)key_copy};
            append(arr, key_val);
            entry = entry->next;
        }
    }

    return arr;
}

// Alias for dict_keys (matches builtin name in interpreter)
Value keys(Value dict) {
    return dict_keys(dict);
}

static int is_truthy_rt(Value v) {
    switch (v.type) {
        case TYPE_BOOL: return v.data != 0;
        case TYPE_INT: return v.data != 0;
        case TYPE_FLOAT: {
            double f = *(double*)&v.data;
            return f != 0.0;
        }
        case TYPE_STRING: return ((char*)v.data)[0] != '\0';
        case TYPE_ARRAY: return ((Array*)v.data)->size > 0;
        case TYPE_DICT: return ((Dict*)v.data)->size > 0;
        case TYPE_NULL: return 0;
        default: return 1;
    }
}

// IN operator: check if left is in right (element in array, key in dict, substring in string)
Value in_operator(Value left, Value right, int line, const char *file) {
    if (right.type == TYPE_ARRAY) {
        // Check if element is in array
        Array *a = (Array*)(right.data);
        Value *elements = (Value*)(a->data);

        for (int i = 0; i < a->size; i++) {
            Value elem = elements[i];

            // Compare by type and value
            if (left.type == elem.type) {
                if (left.type == TYPE_INT && left.data == elem.data) {
                    Value result = {TYPE_INT, 1};  // true
                    return result;
                } else if (left.type == TYPE_FLOAT && left.data == elem.data) {
                    Value result = {TYPE_INT, 1};  // true
                    return result;
                } else if (left.type == TYPE_STRING) {
                    char *left_str = (char*)(left.data);
                    char *elem_str = (char*)(elem.data);
                    if (strcmp(left_str, elem_str) == 0) {
                        Value result = {TYPE_INT, 1};  // true
                        return result;
                    }
                }
            }
        }

        Value result = {TYPE_BOOL, 0};  // false
        return result;

    } else if (right.type == TYPE_DICT) {
        // Check if key is in dict (use dict_has)
        REQUIRE_DICT_KEY_STRING(left);
        return dict_has(right, left);

    } else if (right.type == TYPE_STRING) {
        // Check if substring is in string
        REQUIRE_STRING_SUBSTRING(left);

        char *left_str = (char*)(left.data);
        char *right_str = (char*)(right.data);

        if (strstr(right_str, left_str) != NULL) {
            Value result = {TYPE_BOOL, 1};  // true
            return result;
        } else {
            Value result = {TYPE_BOOL, 0};  // false
            return result;
        }

    } else {
        REQUIRE_IN_RIGHT(right);
        // Never reached (REQUIRE_IN_RIGHT exits), but satisfies compiler
        Value result = {TYPE_BOOL, 0};
        return result;
    }
}

Value not_in_operator(Value left, Value right, int line, const char *file) {
    Value v = in_operator(left, right, line, file);
    int truth = is_truthy_rt(v);
    Value result = {TYPE_BOOL, truth ? 0 : 1};
    return result;
}

// Binary operations - handles all types including string concatenation
Value binary_op(Value left, int op, Value right, int line, const char *file) {
    // OP codes: ADD=0, SUB=1, MUL=2, DIV=3, MOD=4, EQ=5, NE=6, LT=7, LE=8, GT=9, GE=10
#define TC_TYPE(v) ((v).type)
#define TC_IS_STRING(v) ((v).type == TYPE_STRING)
#define TC_IS_ARRAY(v) ((v).type == TYPE_ARRAY)
#define TC_IS_DICT(v) ((v).type == TYPE_DICT)
#define TC_IS_BOOL(v) ((v).type == TYPE_BOOL)
#define TC_IS_NUMERIC(v) ((v).type == TYPE_INT || (v).type == TYPE_FLOAT)
#define TC_IS_NULL(v) ((v).type == TYPE_NULL)
#define TC_ERR(ctx_line, ctx_file, fmt, ...) type_error_ctx((ctx_line), (ctx_file), (fmt), ##__VA_ARGS__)
#define TC_CTX_LINE (line)
#define TC_CTX_FILE (file)

#define REQUIRE_NUMERIC(opname) TC_REQUIRE_NUMERIC((opname), left, right)
#define REQUIRE_BOTH_STRING() TC_REQUIRE_STRING_CONCAT(left, right)
    switch (op) {
        case 0: { // ADD
            // Array concatenation
            if (left.type == TYPE_ARRAY && right.type == TYPE_ARRAY) {
                Array *la = (Array*)left.data;
                Array *ra = (Array*)right.data;
                Array *na = new_array();
                Value arr_val = {TYPE_ARRAY, (long)na};
                Value *le = (Value*)la->data;
                Value *re = (Value*)ra->data;
                for (int i = 0; i < la->size; i++) {
                    append(arr_val, le[i]);
                }
                for (int i = 0; i < ra->size; i++) {
                    append(arr_val, re[i]);
                }
                return arr_val;
            }

            // String concatenation
            if (left.type == TYPE_STRING || right.type == TYPE_STRING) {
                REQUIRE_BOTH_STRING();
                char buf[1024];
                char left_str[512], right_str[512];
                
                // Convert left to string
                strncpy(left_str, (char*)left.data, sizeof(left_str) - 1);
                left_str[sizeof(left_str) - 1] = '\0';
                
                // Convert right to string
                strncpy(right_str, (char*)right.data, sizeof(right_str) - 1);
                right_str[sizeof(right_str) - 1] = '\0';
                
                // Concatenate
                snprintf(buf, sizeof(buf), "%s%s", left_str, right_str);
                char *result_str = strdup(buf);
                Value result = {TYPE_STRING, (long)result_str};
                return result;
            }
            
            // Numeric addition
            REQUIRE_NUMERIC("addition");
            if (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) {
                double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
                double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
                double sum = l + r;
                Value result = {TYPE_FLOAT, *(long*)&sum};
                return result;
            } else {
                long sum = left.data + right.data;
                Value result = {TYPE_INT, sum};
                return result;
            }
        }
        
        case 1: { // SUB
            REQUIRE_NUMERIC("subtraction");
            if (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) {
                double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
                double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
                double diff = l - r;
                Value result = {TYPE_FLOAT, *(long*)&diff};
                return result;
            } else {
                long diff = left.data - right.data;
                Value result = {TYPE_INT, diff};
                return result;
            }
        }
        
        case 2: { // MUL
            REQUIRE_NUMERIC("multiplication");
            if (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) {
                double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
                double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
                double prod = l * r;
                Value result = {TYPE_FLOAT, *(long*)&prod};
                return result;
            } else {
                long prod = left.data * right.data;
                Value result = {TYPE_INT, prod};
                return result;
            }
        }
        
        case 3: { // DIV
            REQUIRE_NUMERIC("division");
            if (left.type == TYPE_INT && right.type == TYPE_INT) {
                if (right.data == 0) {
                    Value msg = {TYPE_STRING, (long)"Division by zero"};
                    __raise(msg, TC_CTX_LINE, (char*)TC_CTX_FILE);
                }
                long quot = left.data / right.data;
                Value result = {TYPE_INT, quot};
                return result;
            } else {
                double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
                double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
                if (r == 0.0) {
                    Value msg = {TYPE_STRING, (long)"Division by zero"};
                    __raise(msg, TC_CTX_LINE, (char*)TC_CTX_FILE);
                }
                double quot = l / r;
                Value result = {TYPE_FLOAT, *(long*)&quot};
                return result;
            }
        }
        
        case 4: { // MOD
            REQUIRE_NUMERIC("modulo");
            if (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) {
                double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
                double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
                if (r == 0.0) {
                    Value msg = {TYPE_STRING, (long)"Modulo by zero"};
                    __raise(msg, TC_CTX_LINE, (char*)TC_CTX_FILE);
                }
                double rem = fmod(l, r);
                Value result = {TYPE_FLOAT, *(long*)&rem};
                return result;
            }
            if (right.data == 0) {
                Value msg = {TYPE_STRING, (long)"Modulo by zero"};
                __raise(msg, TC_CTX_LINE, (char*)TC_CTX_FILE);
            }
            long rem = left.data % right.data;
            Value result = {TYPE_INT, rem};
            return result;
        }
        
        case 5: { // EQ
            if (IS_NUMERIC(left.type) && IS_NUMERIC(right.type)) {
                double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
                double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
                Value result = {TYPE_INT, l == r};
                return result;
            }
            if (left.type == right.type && left.type == TYPE_STRING) {
                int eq = strcmp((char*)left.data, (char*)right.data) == 0;
                Value result = {TYPE_INT, eq};
                return result;
            }
            if (left.type == right.type) {
                int eq = (left.data == right.data);
                Value result = {TYPE_INT, eq};
                return result;
            }
            Value result = {TYPE_INT, 0};
            return result;
        }
        
        case 6: { // NE
            if (IS_NUMERIC(left.type) && IS_NUMERIC(right.type)) {
                double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
                double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
                Value result = {TYPE_INT, l != r};
                return result;
            }
            if (left.type == right.type && left.type == TYPE_STRING) {
                int ne = strcmp((char*)left.data, (char*)right.data) != 0;
                Value result = {TYPE_INT, ne};
                return result;
            }
            if (left.type == right.type) {
                int ne = (left.data != right.data);
                Value result = {TYPE_INT, ne};
                return result;
            }
            Value result = {TYPE_INT, 1};
            return result;
        }
        
        case 7: { // LT
            TC_COMPARE_GUARD(left, right);
            if (left.type == TYPE_STRING && right.type == TYPE_STRING) {
                int lt = strcmp((char*)left.data, (char*)right.data) < 0;
                Value result = {TYPE_INT, lt};
                return result;
            }
            if (left.type == TYPE_BOOL && right.type == TYPE_BOOL) {
                Value result = {TYPE_INT, (left.data < right.data)};
                return result;
            }
            double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
            double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
            Value result = {TYPE_INT, l < r};
            return result;
        }
        
        case 8: { // LE
            TC_COMPARE_GUARD(left, right);
            if (left.type == TYPE_STRING && right.type == TYPE_STRING) {
                int le = strcmp((char*)left.data, (char*)right.data) <= 0;
                Value result = {TYPE_INT, le};
                return result;
            }
            if (left.type == TYPE_BOOL && right.type == TYPE_BOOL) {
                Value result = {TYPE_INT, (left.data <= right.data)};
                return result;
            }
            double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
            double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
            Value result = {TYPE_INT, l <= r};
            return result;
        }
        
        case 9: { // GT
            TC_COMPARE_GUARD(left, right);
            if (left.type == TYPE_STRING && right.type == TYPE_STRING) {
                int gt = strcmp((char*)left.data, (char*)right.data) > 0;
                Value result = {TYPE_INT, gt};
                return result;
            }
            if (left.type == TYPE_BOOL && right.type == TYPE_BOOL) {
                Value result = {TYPE_INT, (left.data > right.data)};
                return result;
            }
            double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
            double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
            Value result = {TYPE_INT, l > r};
            return result;
        }
        
        case 10: { // GE
            TC_COMPARE_GUARD(left, right);
            if (left.type == TYPE_STRING && right.type == TYPE_STRING) {
                int ge = strcmp((char*)left.data, (char*)right.data) >= 0;
                Value result = {TYPE_INT, ge};
                return result;
            }
            if (left.type == TYPE_BOOL && right.type == TYPE_BOOL) {
                Value result = {TYPE_INT, (left.data >= right.data)};
                return result;
            }
            double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
            double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
            Value result = {TYPE_INT, l >= r};
            return result;
        }

        case 11: { // AND
            // Check if left is truthy
            int l_truthy = 0;
            if (left.type == TYPE_INT) {
                l_truthy = (left.data != 0);
            } else {
                l_truthy = (left.data != 0);  // Non-zero pointer means truthy
            }

            // Check if right is truthy
            int r_truthy = 0;
            if (right.type == TYPE_INT) {
                r_truthy = (right.data != 0);
            } else {
                r_truthy = (right.data != 0);  // Non-zero pointer means truthy
            }

            Value result = {TYPE_INT, l_truthy && r_truthy};
            return result;
        }

        case 12: { // OR
            // Check if left is truthy
            int l_truthy = 0;
            if (left.type == TYPE_INT) {
                l_truthy = (left.data != 0);
            } else {
                l_truthy = (left.data != 0);  // Non-zero pointer means truthy
            }

            // Check if right is truthy
            int r_truthy = 0;
            if (right.type == TYPE_INT) {
                r_truthy = (right.data != 0);
            } else {
                r_truthy = (right.data != 0);  // Non-zero pointer means truthy
            }

            Value result = {TYPE_INT, l_truthy || r_truthy};
            return result;
        }

        default: {
            Value result = {TYPE_INT, 0};
            return result;
        }
    }
}

// Regular expression functions

// regexp_match(pattern, str) -> returns 1 if match, 0 otherwise
Value regexp_match(Value pattern_val, Value str_val) {
    if (pattern_val.type != TYPE_STRING || str_val.type != TYPE_STRING) {
        fprintf(stderr, "regexp_match requires two string arguments\n");
        exit(1);
    }

    char *pattern = (char*)pattern_val.data;
    char *str = (char*)str_val.data;

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        fprintf(stderr, "Failed to compile regex: %s\n", pattern);
        regfree(&regex);
        Value result = {TYPE_INT, 0};
        return result;
    }

    ret = regexec(&regex, str, 0, NULL, 0);
    regfree(&regex);

    Value result = {TYPE_INT, (ret == 0) ? 1 : 0};
    return result;
}

// regexp_find(pattern, str) -> returns array of matched strings or capture groups
Value regexp_find(Value pattern_val, Value str_val) {
    if (pattern_val.type != TYPE_STRING || str_val.type != TYPE_STRING) {
        fprintf(stderr, "regexp_find requires two string arguments\n");
        exit(1);
    }

    char *pattern = (char*)pattern_val.data;
    char *str = (char*)str_val.data;

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        fprintf(stderr, "Failed to compile regex: %s\n", pattern);
        regfree(&regex);
        // Return empty array
        return make_array();
    }

    // Get number of capture groups
    size_t num_groups = regex.re_nsub + 1;  // +1 for the whole match
    regmatch_t *matches = (regmatch_t*)malloc(num_groups * sizeof(regmatch_t));

    // Find all matches
    Array *result_arr = (Array*)calloc(1, sizeof(Array));
    result_arr->size = 0;
    result_arr->capacity = 10;
    result_arr->data = calloc(result_arr->capacity, sizeof(Value));

    char *search_str = str;

    while (regexec(&regex, search_str, num_groups, matches, 0) == 0) {
        // If there are capture groups, return only the captured parts
        // Otherwise return the whole match
        int start_idx = (num_groups > 1) ? 1 : 0;  // Skip whole match if we have groups

        for (size_t i = start_idx; i < num_groups; i++) {
            if (matches[i].rm_so == -1) continue;  // This group didn't match

            int match_len = matches[i].rm_eo - matches[i].rm_so;
            char *matched = (char*)malloc(match_len + 1);
            strncpy(matched, search_str + matches[i].rm_so, match_len);
            matched[match_len] = '\0';

            // Add to result array
            if (result_arr->size >= result_arr->capacity) {
                int old_capacity = result_arr->capacity;
                result_arr->capacity *= 2;
                result_arr->data = gc_realloc(result_arr->data, TYPE_ARRAY,
                                              old_capacity * sizeof(Value),
                                              result_arr->capacity * sizeof(Value));
            }

            Value matched_val = {TYPE_STRING, (long)matched};
            ((Value*)result_arr->data)[result_arr->size++] = matched_val;
        }

        // Move to next position after the whole match
        int advance = matches[0].rm_eo;
        if (advance == 0) {
            if (*search_str == '\0') break;
            search_str++;
        } else {
            search_str += advance;
        }
    }

    free(matches);
    regfree(&regex);

    Value result = {TYPE_ARRAY, (long)result_arr};
    return result;
}

// regexp_replace(pattern, str, replacement) -> returns new string with replacements
Value regexp_replace(Value pattern_val, Value str_val, Value replacement_val) {
    if (pattern_val.type != TYPE_STRING || str_val.type != TYPE_STRING || replacement_val.type != TYPE_STRING) {
        type_error("regexp_replace requires three string arguments");
    }

    char *pattern = (char*)pattern_val.data;
    char *str = (char*)str_val.data;
    char *replacement = (char*)replacement_val.data;

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        fprintf(stderr, "Error");
        if (current_err_file) fprintf(stderr, " at %s", current_err_file);
        if (current_err_line > 0) fprintf(stderr, ":%d", current_err_line);
        fprintf(stderr, ": Failed to compile regex: %s\n", pattern);
        regfree(&regex);
        // Return original string
        Value result = {TYPE_STRING, (long)strdup(str)};
        return result;
    }

    size_t num_groups = regex.re_nsub + 1;
    regmatch_t *matches = malloc(sizeof(regmatch_t) * num_groups);

    // Build result string (dynamic buffer)
    size_t cap = 4096;
    size_t result_pos = 0;
    char *result_str = malloc(cap);
    result_str[0] = '\0';

    char *search_str = str;

    while (regexec(&regex, search_str, num_groups, matches, 0) == 0) {
        // Copy text before match
        int pre_len = matches[0].rm_so;
        if (result_pos + pre_len + 1 >= cap) {
            cap = cap + pre_len + 256;
            result_str = realloc(result_str, cap);
        }
        strncat(result_str, search_str, pre_len);
        result_pos += pre_len;

        // Expand replacement with backreferences \1..\9
        for (const char *rp = replacement; *rp; rp++) {
            if (*rp == '\\' && rp[1] >= '0' && rp[1] <= '9') {
                int idx = rp[1] - '0';
                if (idx < (int)num_groups && matches[idx].rm_so != -1) {
                    int mlen = matches[idx].rm_eo - matches[idx].rm_so;
                    if (result_pos + mlen + 1 >= cap) {
                        cap = cap + mlen + 256;
                        result_str = realloc(result_str, cap);
                    }
                    strncat(result_str, search_str + matches[idx].rm_so, mlen);
                    result_pos += mlen;
                }
                rp++; // skip digit
            } else {
                if (result_pos + 2 >= cap) {
                    cap *= 2;
                    result_str = realloc(result_str, cap);
                }
                result_str[result_pos++] = *rp;
                result_str[result_pos] = '\0';
            }
        }

        // Move to next position
        search_str += matches[0].rm_eo;

        // Prevent infinite loop on zero-length matches
        if (matches[0].rm_eo == 0) {
            if (*search_str == '\0') break;
            if (result_pos + 2 >= cap) {
                cap *= 2;
                result_str = realloc(result_str, cap);
            }
            result_str[result_pos++] = *search_str;
            result_str[result_pos] = '\0';
            search_str++;
        }
    }

    // Copy remaining text
    size_t remain = strlen(search_str);
    if (result_pos + remain + 1 >= cap) {
        cap = result_pos + remain + 1;
        result_str = realloc(result_str, cap);
    }
    strcat(result_str, search_str);

    free(matches);

    regfree(&regex);

    Value result = {TYPE_STRING, (long)result_str};
    return result;
}

// String utility functions

// str_split(str, separator) -> returns array of strings
Value str_split(Value str_val, Value sep_val) {
    if (str_val.type != TYPE_STRING || sep_val.type != TYPE_STRING) {
        fprintf(stderr, "str_split requires two string arguments\n");
        exit(1);
    }

    char *str = (char*)str_val.data;
    char *separator = (char*)sep_val.data;
    int sep_len = strlen(separator);

    if (sep_len == 0) {
        fprintf(stderr, "str_split separator cannot be empty\n");
        exit(1);
    }

    // Create result array
    Array *result_arr = (Array*)calloc(1, sizeof(Array));
    result_arr->size = 0;
    result_arr->capacity = 10;
    result_arr->data = calloc(result_arr->capacity, sizeof(Value));

    char *current = str;
    char *next;

    while ((next = strstr(current, separator)) != NULL) {
        // Extract substring before separator
        int len = next - current;
        char *part = (char*)malloc(len + 1);
        strncpy(part, current, len);
        part[len] = '\0';

        // Add to result array
        if (result_arr->size >= result_arr->capacity) {
            int old_capacity = result_arr->capacity;
            result_arr->capacity *= 2;
            result_arr->data = gc_realloc(result_arr->data, TYPE_ARRAY,
                                          old_capacity * sizeof(Value),
                                          result_arr->capacity * sizeof(Value));
        }

        Value part_val = {TYPE_STRING, (long)part};
        ((Value*)result_arr->data)[result_arr->size++] = part_val;

        // Move to position after separator
        current = next + sep_len;
    }

    // Add remaining part after last separator
    char *last_part = strdup(current);
    if (result_arr->size >= result_arr->capacity) {
        int old_capacity = result_arr->capacity;
        result_arr->capacity *= 2;
        result_arr->data = gc_realloc(result_arr->data, TYPE_ARRAY,
                                      old_capacity * sizeof(Value),
                                      result_arr->capacity * sizeof(Value));
    }
    Value last_val = {TYPE_STRING, (long)last_part};
    ((Value*)result_arr->data)[result_arr->size++] = last_val;

    Value result = {TYPE_ARRAY, (long)result_arr};
    return result;
}

// str_join(array, separator) -> returns joined string
Value str_join(Value arr_val, Value sep_val) {
    if (arr_val.type != TYPE_ARRAY || sep_val.type != TYPE_STRING) {
        fprintf(stderr, "str_join requires array and string separator\n");
        exit(1);
    }

    Array *arr = (Array*)(arr_val.data);
    char *separator = (char*)sep_val.data;

    // Calculate total length needed
    int total_len = 0;
    Value *elements = (Value*)(arr->data);
    
    for (int i = 0; i < arr->size; i++) {
        if (elements[i].type == TYPE_STRING) {
            total_len += strlen((char*)elements[i].data);
        } else if (elements[i].type == TYPE_INT) {
            total_len += 20;  // Enough for any int
        } else {
            total_len += 30;  // Enough for other types
        }
        
        if (i < arr->size - 1) {
            total_len += strlen(separator);
        }
    }

    // Build result string
    char *result_str = (char*)malloc(total_len + 1);
    result_str[0] = '\0';

    for (int i = 0; i < arr->size; i++) {
        char temp[128];
        
        if (elements[i].type == TYPE_STRING) {
            strcat(result_str, (char*)elements[i].data);
        } else if (elements[i].type == TYPE_INT) {
            sprintf(temp, "%ld", elements[i].data);
            strcat(result_str, temp);
        } else if (elements[i].type == TYPE_FLOAT) {
            double f = *(double*)&elements[i].data;
            sprintf(temp, "%g", f);
            strcat(result_str, temp);
        } else {
            strcat(result_str, "<object>");
        }

        if (i < arr->size - 1) {
            strcat(result_str, separator);
        }
    }

    Value result = {TYPE_STRING, (long)result_str};
    return result;
}

// Trim characters from both ends of a string
Value str_trim(Value str_val, Value chars_val) {
    if (str_val.type != TYPE_STRING) {
        type_error("str_trim requires string input");
    }
    if (!(chars_val.type == TYPE_STRING || chars_val.type == TYPE_NULL || chars_val.type == TYPE_INT)) {
        // allow default by passing null or fallback
        type_error("str_trim chars must be string or omitted");
    }
    const char *s = (char*)str_val.data;
    const char *trim_chars = chars_val.type == TYPE_STRING ? (char*)chars_val.data : " \t\n";
    int start = 0;
    int end = strlen(s) - 1;
    while (start <= end && strchr(trim_chars, s[start]) != NULL) start++;
    while (end >= start && strchr(trim_chars, s[end]) != NULL) end--;
    int len = end - start + 1;
    if (len < 0) len = 0;
    char *res = malloc(len + 1);
    strncpy(res, s + start, len);
    res[len] = '\0';
    Value result = {TYPE_STRING, (long)res};
    return result;
}

// Simple formatter supporting %d, %f, %s (with precision for %f and %s)
Value str_format(Value fmt_val, Value *args, int arg_count) {
    if (fmt_val.type != TYPE_STRING) {
        type_error("str_format requires format string");
    }
    const char *fmt = (char*)fmt_val.data;
    int ai = 0;
    int cap = 256, len = 0;
    char *buf = malloc(cap);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            if (len >= cap - 1) { cap *= 2; buf = realloc(buf, cap); }
            buf[len++] = *p;
            continue;
        }
        p++;
        if (*p == '%') { if (len >= cap - 1) { cap *= 2; buf = realloc(buf, cap); } buf[len++] = '%'; continue; }
        int precision = -1;
        if (*p == '.') {
            p++;
            precision = 0;
            while (*p >= '0' && *p <= '9') { precision = precision * 10 + (*p - '0'); p++; }
        }
        if (ai >= arg_count) { fprintf(stderr, "str_format: insufficient arguments\n"); exit(1); }
        Value v = args[ai++];
        char tmp[256];
        if (*p == 'd') {
            long iv = (v.type == TYPE_INT) ? v.data : (long)value_to_double(v);
            snprintf(tmp, sizeof(tmp), "%ld", iv);
        } else if (*p == 'f') {
            double dv = value_to_double(v);
            if (precision >= 0) {
                char fmtbuf[16];
                snprintf(fmtbuf, sizeof(fmtbuf), "%%.%df", precision);
                snprintf(tmp, sizeof(tmp), fmtbuf, dv);
            } else {
                snprintf(tmp, sizeof(tmp), "%f", dv);
            }
        } else if (*p == 's') {
            const char *sv = (v.type == TYPE_STRING) ? (char*)v.data : "";
            if (precision >= 0) {
                snprintf(tmp, sizeof(tmp), "%.*s", precision, sv);
            } else {
                snprintf(tmp, sizeof(tmp), "%s", sv);
            }
        } else {
            fprintf(stderr, "str_format: unsupported specifier %%%c\n", *p);
            exit(1);
        }
        int tlen = strlen(tmp);
        while (len + tlen >= cap) { cap *= 2; buf = realloc(buf, cap); }
        memcpy(buf + len, tmp, tlen);
        len += tlen;
    }
    buf[len] = '\0';
    Value result = {TYPE_STRING, (long)buf};
    return result;
}

// ===== Misc Helpers =====

static double value_to_double(Value v) {
    switch (v.type) {
        case TYPE_INT: return (double)v.data;
        case TYPE_FLOAT: return *(double*)&v.data;
        case TYPE_STRING: return atof((char*)v.data);
        default:
            fprintf(stderr, "Math functions require numeric/string convertible types\n");
            exit(1);
    }
}

// Math helpers
static Value make_float_value(double d) {
    Value r = {TYPE_FLOAT, *(long*)&d};
    return r;
}

Value math_sin(Value a) { return make_float_value(sin(value_to_double(a))); }
Value math_cos(Value a) { return make_float_value(cos(value_to_double(a))); }
Value math_asin(Value a) { return make_float_value(asin(value_to_double(a))); }
Value math_acos(Value a) { return make_float_value(acos(value_to_double(a))); }
Value math_log(Value a) { return make_float_value(log(value_to_double(a))); }
Value math_exp(Value a) { return make_float_value(exp(value_to_double(a))); }
Value math_ceil(Value a) { return make_float_value(ceil(value_to_double(a))); }
Value math_floor(Value a) { return make_float_value(floor(value_to_double(a))); }
Value math_round(Value a) { return make_float_value(round(value_to_double(a))); }
Value math_sqrt(Value a) { return make_float_value(sqrt(value_to_double(a))); }
Value math_pow_val(Value a, Value b) {
    return make_float_value(pow(value_to_double(a), value_to_double(b)));
}
Value math_random_val(Value a, Value b, int arg_count) {
    double r = (double)rand() / (double)RAND_MAX;
    if (arg_count == 2) {
        double min = value_to_double(a);
        double max = value_to_double(b);
        r = min + r * (max - min);
    }
    return make_float_value(r);
}

Value remove_entry(Value obj, Value key_or_index) {
    if (obj.type == TYPE_ARRAY) {
        if (key_or_index.type != TYPE_INT) {
            Value r = {TYPE_INT, 0};
            return r;
        }
        Array *arr = (Array*)obj.data;
        long idx = key_or_index.data;
        if (idx < 0 || idx >= arr->size) {
            Value r = {TYPE_INT, 0};
            return r;
        }
        Value *elements = (Value*)arr->data;
        for (long i = idx; i < arr->size - 1; i++) {
            elements[i] = elements[i + 1];
        }
        arr->size--;
        Value r = {TYPE_INT, 1};
        return r;
    }

    if (obj.type == TYPE_DICT) {
        if (key_or_index.type != TYPE_STRING) {
            Value r = {TYPE_INT, 0};
            return r;
        }
        const char *key = (char*)key_or_index.data;
        unsigned int idx = hash(key);
        Dict *dict = (Dict*)obj.data;
        DictEntry *entry = dict->buckets[idx];
        DictEntry *prev = NULL;
        while (entry != NULL) {
            if (strcmp(entry->key, key) == 0) {
                if (prev == NULL) {
                    dict->buckets[idx] = entry->next;
                } else {
                    prev->next = entry->next;
                }
                free(entry->key);
                free(entry);
                dict->size--;
                Value r = {TYPE_INT, 1};
                return r;
            }
            prev = entry;
            entry = entry->next;
        }
        Value r = {TYPE_INT, 0};
        return r;
    }

    Value r = {TYPE_INT, 0};
    return r;
}

Value math_fn(Value op, Value a, Value b, int arg_count) {
    if (op.type != TYPE_STRING) {
        fprintf(stderr, "math() first argument must be operation string\n");
        exit(1);
    }
    char *name = (char*)op.data;

    if (strcmp(name, "sin") == 0 || strcmp(name, "cos") == 0 ||
        strcmp(name, "asin") == 0 || strcmp(name, "acos") == 0 ||
        strcmp(name, "log") == 0 || strcmp(name, "exp") == 0 ||
        strcmp(name, "ceil") == 0 || strcmp(name, "floor") == 0 ||
        strcmp(name, "round") == 0) {
        if (arg_count != 1) {
            fprintf(stderr, "math(%s) requires 1 argument\n", name);
            exit(1);
        }
        double val = value_to_double(a);
        double res;
        if (strcmp(name, "sin") == 0) res = sin(val);
        else if (strcmp(name, "cos") == 0) res = cos(val);
        else if (strcmp(name, "asin") == 0) res = asin(val);
        else if (strcmp(name, "acos") == 0) res = acos(val);
        else if (strcmp(name, "log") == 0) res = log(val);
        else if (strcmp(name, "exp") == 0) res = exp(val);
        else if (strcmp(name, "ceil") == 0) res = ceil(val);
        else if (strcmp(name, "floor") == 0) res = floor(val);
        else res = round(val);
        Value result = {TYPE_FLOAT, *(long*)&res};
        return result;
    }

    if (strcmp(name, "pow") == 0) {
        if (arg_count != 2) {
            fprintf(stderr, "math(pow) requires 2 arguments\n");
            exit(1);
        }
        double va = value_to_double(a);
        double vb = value_to_double(b);
        double res = pow(va, vb);
        Value result = {TYPE_FLOAT, *(long*)&res};
        return result;
    }

    if (strcmp(name, "random") == 0) {
        if (arg_count == 0) {
            double r = (double)rand() / (double)RAND_MAX;
            Value result = {TYPE_FLOAT, *(long*)&r};
            return result;
        }
        if (arg_count == 2) {
            double va = value_to_double(a);
            double vb = value_to_double(b);
            double r = (double)rand() / (double)RAND_MAX;
            double res = va + (vb - va) * r;
            Value result = {TYPE_FLOAT, *(long*)&res};
            return result;
        }
        fprintf(stderr, "math(random) requires 0 or 2 arguments\n");
        exit(1);
    }

    fprintf(stderr, "math(): unsupported op %s\n", name);
    exit(1);
}

// ===== JSON Helpers =====
static int json_error_rt = 0;

static const char* skip_ws_c(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static Value parse_json_value_rt(const char **p);

static Value parse_json_string_rt(const char **p) {
    char quote = **p;
    (*p)++;
    char buf[4096];
    int idx = 0;
    while (**p && **p != quote) {
        if (**p == '\\') {
            (*p)++;
            char esc = **p;
            switch (esc) {
                case '"': buf[idx++] = '"'; break;
                case '\'': buf[idx++] = '\''; break;
                case '\\': buf[idx++] = '\\'; break;
                case 'n': buf[idx++] = '\n'; break;
                case 't': buf[idx++] = '\t'; break;
                case 'r': buf[idx++] = '\r'; break;
                default: buf[idx++] = esc; break;
            }
        } else {
            buf[idx++] = **p;
        }
        (*p)++;
        if (idx >= 4095) break;
    }
    buf[idx] = '\0';
    if (**p == quote) {
        (*p)++;
    } else {
        json_error_rt = 1; // unterminated string
    }
    char *res = strdup(buf);
    Value v = {TYPE_STRING, (long)res};
    return v;
}

static Value parse_json_number_rt(const char **p) {
    char *endptr;
    double d = strtod(*p, &endptr);
    int is_float = 0;
    for (const char *q = *p; q < endptr; q++) {
        if (*q == '.' || *q == 'e' || *q == 'E') { is_float = 1; break; }
    }
    *p = endptr;
    if (is_float) {
        Value v = {TYPE_FLOAT, *(long*)&d};
        return v;
    } else {
        Value v = {TYPE_INT, (long)d};
        return v;
    }
}

static int match_word_rt(const char **p, const char *word) {
    const char *s = *p;
    while (*word) {
        if (tolower((unsigned char)*s) != tolower((unsigned char)*word)) return 0;
        s++; word++;
    }
    *p = s;
    return 1;
}

static Value parse_json_array_rt(const char **p) {
    if (**p != '[') { json_error_rt = 1; Value v = {TYPE_INT,0}; return v; }
    (*p)++; // skip '['
    Value arr = make_array();
    *p = skip_ws_c(*p);
    if (**p == ']') { (*p)++; return arr; }
    int closed = 0;
    while (**p) {
        Value v = parse_json_value_rt(p);
        append(arr, v);
        *p = skip_ws_c(*p);
        if (**p == ',') {
            (*p)++;
            *p = skip_ws_c(*p);
            if (**p == ']') { (*p)++; closed = 1; break; }
        } else if (**p == ']') { (*p)++; closed = 1; break; }
        else { json_error_rt = 1; break; }
    }
    if (!closed) json_error_rt = 1;
    return arr;
}

static Value parse_json_object_rt(const char **p) {
    if (**p != '{') { json_error_rt = 1; Value v = {TYPE_INT,0}; return v; }
    (*p)++; // skip '{'
    Value dict = make_dict();
    *p = skip_ws_c(*p);
    if (**p == '}') { (*p)++; return dict; }
    int closed = 0;
    while (**p) {
        *p = skip_ws_c(*p);
        if (**p != '"' && **p != '\'') { json_error_rt = 1; break; }
        Value key = parse_json_string_rt(p);
        *p = skip_ws_c(*p);
        if (**p == ':') (*p)++;
        else { json_error_rt = 1; break; }
        *p = skip_ws_c(*p);
        Value val = parse_json_value_rt(p);
        dict_set(dict, key, val);
        *p = skip_ws_c(*p);
        if (**p == ',') {
            (*p)++;
            *p = skip_ws_c(*p);
            if (**p == '}') { (*p)++; closed = 1; break; }
        } else if (**p == '}') { (*p)++; closed = 1; break; }
        else { json_error_rt = 1; break; }
    }
    if (!closed) json_error_rt = 1;
    return dict;
}

static Value parse_json_value_rt(const char **p) {
    *p = skip_ws_c(*p);
    char c = **p;
    if (c == '"' || c == '\'') return parse_json_string_rt(p);
    if (c == '[') return parse_json_array_rt(p);
    if (c == '{') return parse_json_object_rt(p);
    if (isdigit((unsigned char)c) || c == '-') return parse_json_number_rt(p);
    if (match_word_rt(p, "true")) { Value v = {TYPE_BOOL, 1}; return v; }
    if (match_word_rt(p, "false")) { Value v = {TYPE_BOOL, 0}; return v; }
    if (match_word_rt(p, "null")) { Value v = {TYPE_NULL, 0}; return v; }
    json_error_rt = 1;
    Value v = {TYPE_INT, 0};
    return v;
}

static void sb_rt_append(char **buf, int *len, int *cap, const char *s) {
    int sl = strlen(s);
    if (*len + sl + 1 > *cap) {
        *cap = (*cap + sl + 256);
        *buf = realloc(*buf, *cap);
    }
    memcpy(*buf + *len, s, sl);
    *len += sl;
    (*buf)[*len] = '\0';
}

static void json_serialize_value_rt(Value v, char **buf, int *len, int *cap);

static void json_serialize_string_rt(const char *s, char **buf, int *len, int *cap) {
    sb_rt_append(buf, len, cap, "\"");
    while (*s) {
        if (*s == '"' || *s == '\\') {
            char esc[3] = {'\\', *s, '\0'};
            sb_rt_append(buf, len, cap, esc);
        } else if (*s == '\n') sb_rt_append(buf, len, cap, "\\n");
        else if (*s == '\t') sb_rt_append(buf, len, cap, "\\t");
        else {
            char ch[2] = {*s, '\0'};
            sb_rt_append(buf, len, cap, ch);
        }
        s++;
    }
    sb_rt_append(buf, len, cap, "\"");
}

static void json_serialize_value_rt(Value v, char **buf, int *len, int *cap) {
    switch (v.type) {
        case TYPE_INT: {
            char tmp[64]; snprintf(tmp, sizeof(tmp), "%ld", v.data);
            sb_rt_append(buf, len, cap, tmp);
            break;
        }
        case TYPE_FLOAT: {
            double d = *(double*)&v.data;
            char tmp[64]; snprintf(tmp, sizeof(tmp), "%g", d);
            sb_rt_append(buf, len, cap, tmp);
            break;
        }
        case TYPE_STRING:
            json_serialize_string_rt((char*)v.data, buf, len, cap);
            break;
        case TYPE_ARRAY: {
            sb_rt_append(buf, len, cap, "[");
            Array *a = (Array*)v.data;
            Value *ele = (Value*)a->data;
            for (int i = 0; i < a->size; i++) {
                json_serialize_value_rt(ele[i], buf, len, cap);
                if (i < a->size - 1) sb_rt_append(buf, len, cap, ",");
            }
            sb_rt_append(buf, len, cap, "]");
            break;
        }
        case TYPE_DICT: {
            sb_rt_append(buf, len, cap, "{");
            Dict *d = (Dict*)v.data;
            int first = 1;
            for (int i = 0; i < HASH_SIZE; i++) {
                DictEntry *entry = d->buckets[i];
                while (entry) {
                    if (!first) sb_rt_append(buf, len, cap, ",");
                    first = 0;
                    json_serialize_string_rt(entry->key, buf, len, cap);
                    sb_rt_append(buf, len, cap, ":");
                    json_serialize_value_rt(entry->value, buf, len, cap);
                    entry = entry->next;
                }
            }
            sb_rt_append(buf, len, cap, "}");
            break;
        }
        case TYPE_NULL:
            sb_rt_append(buf, len, cap, "null");
            break;
        default:
            sb_rt_append(buf, len, cap, "null");
    }
}

Value json_decode_ctx(Value json_str, int line, char *file) {
    if (json_str.type != TYPE_STRING) {
        Value msg = {TYPE_STRING, (long)"json_decode expects a string"};
        __raise(msg, line, file);
    }
    json_error_rt = 0;
    const char *p = (char*)json_str.data;
    Value v = parse_json_value_rt(&p);
    p = skip_ws_c(p);
    if (*p != '\0') json_error_rt = 1;
    if (json_error_rt) {
        Value msg = {TYPE_STRING, (long)"Invalid JSON string"};
        __raise(msg, line, file);
    }
    return v;
}

Value json_decode(Value json_str) {
    // Default wrapper when no source context is provided
    return json_decode_ctx(json_str, 0, NULL);
}

Value json_encode(Value v) {
    char *buf = malloc(256);
    buf[0] = '\0';
    int len = 0, cap = 256;
    json_serialize_value_rt(v, &buf, &len, &cap);
    Value res = {TYPE_STRING, (long)buf};
    return res;
}

// ===== Exceptions =====
void* __try_push_buf(void) {
    if (try_top >= 256) {
        fprintf(stderr, "Exception stack overflow\n");
        exit(1);
    }
    return (void*)&try_stack[try_top++];
}

void __try_pop(void) {
    if (try_top > 0) try_top--;
}

void __raise(Value msg, int line, char *file) {
    char *mstr;
    if (msg.type == TYPE_STRING) {
        mstr = strdup((char*)msg.data);
    } else {
        Value s = to_string(msg);
        mstr = strdup((char*)s.data);
    }
    char buf[512];
    snprintf(buf, sizeof(buf), "%s:%d: %s", file ? file : "<input>", line, mstr);
    free(mstr);
    char *full = strdup(buf);
    Value v = {TYPE_STRING, (long)full};
    current_exception = v;
    if (try_top > 0) {
        longjmp(try_stack[try_top - 1], 1);
    }
    fprintf(stderr, "%s\n", full);
    exit(1);
}

Value __get_exception(void) {
    return current_exception;
}

// ===== Class/Object Runtime =====

static MethodEntry* find_method_entry(Class *cls, const char *name) {
    for (int i = 0; i < cls->method_count; i++) {
        if (strcmp(cls->methods[i].name, name) == 0) {
            return &cls->methods[i];
        }
    }
    return NULL;
}

Value make_class(char *name) {
    Class *cls = gc_alloc(TYPE_CLASS, sizeof(Class));
    cls->name = strdup(name);  // Keep strdup for simplicity
    cls->methods = NULL;
    cls->method_count = 0;
    cls->method_capacity = 0;
    cls->fields = NULL;
    cls->field_count = 0;
    cls->field_capacity = 0;
    Value v = {TYPE_CLASS, (long)cls};
    return v;
}

void class_add_field(Value class_val, char *name, FieldInitFn init_fn, int is_private) {
    if (class_val.type != TYPE_CLASS) return;
    Class *cls = (Class*)class_val.data;
    if (cls->field_count >= cls->field_capacity) {
        cls->field_capacity = cls->field_capacity == 0 ? 4 : cls->field_capacity * 2;
        cls->fields = realloc(cls->fields, cls->field_capacity * sizeof(FieldEntry));
    }
    cls->fields[cls->field_count].name = strdup(name);
    cls->fields[cls->field_count].init_fn = init_fn;
    cls->fields[cls->field_count].is_private = is_private;
    cls->field_count++;
}

void class_add_method(Value class_val, char *name, MethodFn fn, int arity, int is_private) {
    if (class_val.type != TYPE_CLASS) return;
    Class *cls = (Class*)class_val.data;
    if (cls->method_count >= cls->method_capacity) {
        cls->method_capacity = cls->method_capacity == 0 ? 4 : cls->method_capacity * 2;
        cls->methods = realloc(cls->methods, cls->method_capacity * sizeof(MethodEntry));
    }
    cls->methods[cls->method_count].name = strdup(name);
    cls->methods[cls->method_count].fn = fn;
    cls->methods[cls->method_count].arity = arity;
    cls->methods[cls->method_count].is_private = is_private;
    cls->method_count++;
}

Value instantiate_class(Value class_val, Value *args, int arg_count) {
    if (class_val.type != TYPE_CLASS) {
        Value result = {TYPE_INT, 0};
        return result;
    }
    Class *cls = (Class*)class_val.data;

    Instance *inst = gc_alloc(TYPE_INSTANCE, sizeof(Instance));
    inst->cls = cls;
    inst->fields = make_dict();

    Value inst_val = {TYPE_INSTANCE, (long)inst};

    // Initialize fields
    push_this(inst);
    for (int i = 0; i < cls->field_count; i++) {
        FieldEntry *f = &cls->fields[i];
        Value key = {TYPE_STRING, (long)f->name};
        Value val = f->init_fn ? f->init_fn(inst_val) : (Value){TYPE_INT, 0};
        dict_set(inst->fields, key, val);
    }
    pop_this();

    // Call constructor init if present
    MethodEntry *init_m = find_method_entry(cls, "init");
    if (init_m != NULL) {
        if (arg_count != init_m->arity) {
            fprintf(stderr, "Constructor for %s expects %d arguments, got %d\n", cls->name, init_m->arity, arg_count);
            exit(1);
        }
        method_call(inst_val, "init", args, arg_count);
    } else if (arg_count > 0) {
        fprintf(stderr, "Class %s constructor does not take arguments\n", cls->name);
        exit(1);
    }

    return inst_val;
}

Value member_get(Value instance, char *name) {
    if (instance.type != TYPE_INSTANCE) {
        Value result = {TYPE_INT, 0};
        return result;
    }
    Instance *inst = (Instance*)instance.data;

    int is_private = name[0] == '_';
    if (is_private && !is_internal_access(inst)) {
        fprintf(stderr, "Cannot access private member '%s' of class %s\n", name, inst->cls->name);
        exit(1);
    }

    Value key = {TYPE_STRING, (long)name};
    Value has = dict_has(inst->fields, key);
    if ((has.type == TYPE_INT || has.type == TYPE_BOOL) && has.data == 1) {
        return dict_get(inst->fields, key);
    }

    MethodEntry *m = find_method_entry(inst->cls, name);
    if (m != NULL) {
        // Return a placeholder; actual call should go through method_call
        Value result = {TYPE_INT, 0};
        return result;
    }

    fprintf(stderr, "Member '%s' not found on class %s\n", name, inst->cls->name);
    exit(1);
}

Value member_set(Value instance, char *name, Value val) {
    if (instance.type != TYPE_INSTANCE) {
        Value result = {TYPE_INT, 0};
        return result;
    }
    Instance *inst = (Instance*)instance.data;

    int is_private = name[0] == '_';
    if (is_private && !is_internal_access(inst)) {
        fprintf(stderr, "Cannot access private member '%s' of class %s\n", name, inst->cls->name);
        exit(1);
    }

    Value key = {TYPE_STRING, (long)name};
    dict_set(inst->fields, key, val);
    return val;
}

Value method_call(Value instance, char *name, Value *args, int arg_count) {
    if (instance.type != TYPE_INSTANCE) {
        Value result = {TYPE_INT, 0};
        return result;
    }
    Instance *inst = (Instance*)instance.data;

    MethodEntry *m = find_method_entry(inst->cls, name);
    if (m == NULL) {
        fprintf(stderr, "Method '%s' not found on class %s\n", name, inst->cls->name);
        exit(1);
    }

    if (m->is_private && !is_internal_access(inst)) {
        fprintf(stderr, "Cannot access private method '%s' of class %s\n", name, inst->cls->name);
        exit(1);
    }

    if (arg_count != m->arity) {
        fprintf(stderr, "Method %s expects %d arguments, got %d\n", name, m->arity, arg_count);
        exit(1);
    }

    push_this(inst);
    Value result = m->fn(instance, args, arg_count);
    pop_this();
    return result;
}

// Recursive print helper for arrays and dicts
static void print_value_recursive(Value v) {
    switch (v.type) {
        case TYPE_INT:
            printf("%ld", v.data);
            break;
        case TYPE_FLOAT: {
            double *fp = (double*)&v.data;
            printf("%g", *fp);
            break;
        }
        case TYPE_BOOL:
            printf("%s", v.data ? "true" : "false");
            break;
        case TYPE_STRING:
            printf("\"%s\"", (char*)v.data);
            break;
        case TYPE_ARRAY: {
            printf("[");
            Array *arr = (Array*)v.data;
            Value *elements = (Value*)arr->data;
            for (int j = 0; j < arr->size; j++) {
                print_value_recursive(elements[j]);
                if (j < arr->size - 1) printf(", ");
            }
            printf("]");
            break;
        }
        case TYPE_DICT: {
            printf("{");
            Dict *dict = (Dict*)v.data;
            int count = 0;
            for (int j = 0; j < HASH_SIZE; j++) {
                DictEntry *entry = dict->buckets[j];
                while (entry != NULL) {
                    if (count > 0) printf(", ");
                    printf("\"%s\": ", entry->key);
                    print_value_recursive(entry->value);
                    entry = entry->next;
                    count++;
                }
            }
            printf("}");
            break;
        }
        case TYPE_NULL:
            printf("null");
            break;
        default:
            printf("<object>");
    }
}

// Print a value (used by LLVM-generated code)
void print_value(Value v) {
    // For non-string types, print without outer quotes
    if (v.type == TYPE_STRING) {
        printf("%s", (char*)v.data);
    } else if (v.type == TYPE_ARRAY || v.type == TYPE_DICT) {
        print_value_recursive(v);
    } else {
        switch (v.type) {
            case TYPE_INT:
                printf("%ld", v.data);
                break;
            case TYPE_FLOAT: {
                double *fp = (double*)&v.data;
                printf("%g", *fp);
                break;
            }
            case TYPE_BOOL:
                printf("%s", v.data ? "true" : "false");
                break;
            case TYPE_NULL:
                printf("null");
                break;
            default:
                printf("<object>");
        }
    }
}

// Set command line arguments (called from main)
void set_cmd_args(int argc, char **argv) {
    g_argc = argc;
    g_argv = argv;
}

// Get command line arguments (called from LLVM generated code)
Value cmd_args(void) {
    Array *arr = new_array();
    // Note: set_cmd_args should be called with pre-adjusted argc/argv
    // that already excludes the executable name and script file
    for (int i = 0; i < g_argc; i++) {
        int len = strlen(g_argv[i]);
        char *str_copy = malloc(len + 1);
        strcpy(str_copy, g_argv[i]);
        Value str_val = {TYPE_STRING, (long)str_copy};

        Value *elements = (Value*)arr->data;
        if (arr->size >= arr->capacity) {
            int old_capacity = arr->capacity;
            arr->capacity *= 2;
            arr->data = gc_realloc(arr->data, TYPE_ARRAY,
                                   old_capacity * sizeof(Value),
                                   arr->capacity * sizeof(Value));
            elements = (Value*)arr->data;
        }
        elements[arr->size++] = str_val;
    }
    Value result = {TYPE_ARRAY, (long)arr};
    return result;
}

// GC statistics function - callable from TL scripts
Value gc_stat(void) {
    gc_print_stats();
    Value result = {TYPE_NULL, 0};
    return result;
}

// Force garbage collection - callable from TL scripts
Value gc_run(void) {
    gc_collect();
    Value result = {TYPE_NULL, 0};
    return result;
}
