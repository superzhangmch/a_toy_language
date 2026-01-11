#include "runtime.h"
#include <regex.h>

// Array structure
typedef struct {
    int size;
    int capacity;
    void *data;
} Array;

// Dict structure
#define HASH_SIZE 256

typedef struct DictEntry {
    char *key;
    Value value;
    struct DictEntry *next;
} DictEntry;

typedef struct {
    DictEntry **buckets;
    int size;
} Dict;

// Helper to create array
static Array* new_array() {
    Array *a = malloc(sizeof(Array));
    a->size = 0;
    a->capacity = 8;
    a->data = malloc(8 * sizeof(Value));
    return a;
}

// Create empty array
Value make_array(void) {
    Array *a = new_array();
    Value result = {TYPE_ARRAY, (long)a};
    return result;
}

// Append value to array
Value append(Value arr, Value val) {
    Array *a = (Array*)(arr.data);
    if (a->size >= a->capacity) {
        a->capacity *= 2;
        a->data = realloc(a->data, a->capacity * sizeof(Value));
    }
    ((Value*)a->data)[a->size++] = val;
    Value result = {TYPE_INT, 0};
    return result;
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
    }
    Value result = {TYPE_INT, 0};
    return result;
}

// Type conversion functions
Value to_int(Value v) {
    if (v.type == TYPE_INT) return v;
    if (v.type == TYPE_FLOAT) {
        double f = *(double*)&v.data;
        Value result = {TYPE_INT, (long)f};
        return result;
    }
    Value result = {TYPE_INT, 0};
    return result;
}

Value to_float(Value v) {
    if (v.type == TYPE_FLOAT) return v;
    if (v.type == TYPE_INT) {
        double f = (double)v.data;
        Value result = {TYPE_FLOAT, *(long*)&f};
        return result;
    }
    double f = 0.0;
    Value result = {TYPE_FLOAT, *(long*)&f};
    return result;
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
    }
    Value result = {TYPE_STRING, (long)""};
    return result;
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
                new_a->capacity *= 2;
                new_a->data = realloc(new_a->data, new_a->capacity * sizeof(Value));
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
Value read(Value filename) {
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
Value write(Value content, Value filename) {
    char *fname = (char*)(filename.data);
    FILE *f = fopen(fname, "w");

    if (f == NULL) {
        Value result = {TYPE_INT, 0};
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
    Dict *d = malloc(sizeof(Dict));
    d->buckets = calloc(HASH_SIZE, sizeof(DictEntry*));
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
            return val;
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

    return val;
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
            Value result = {TYPE_INT, 1};  // true
            return result;
        }
        entry = entry->next;
    }

    Value result = {TYPE_INT, 0};  // false
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

// IN operator: check if left is in right (element in array, key in dict, substring in string)
Value in_operator(Value left, Value right) {
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

        Value result = {TYPE_INT, 0};  // false
        return result;

    } else if (right.type == TYPE_DICT) {
        // Check if key is in dict (use dict_has)
        return dict_has(right, left);

    } else if (right.type == TYPE_STRING) {
        // Check if substring is in string
        if (left.type != TYPE_STRING) {
            fprintf(stderr, "Can only check if string is in string\n");
            exit(1);
        }

        char *left_str = (char*)(left.data);
        char *right_str = (char*)(right.data);

        if (strstr(right_str, left_str) != NULL) {
            Value result = {TYPE_INT, 1};  // true
            return result;
        } else {
            Value result = {TYPE_INT, 0};  // false
            return result;
        }

    } else {
        fprintf(stderr, "IN operator requires array, dict, or string on the right side\n");
        exit(1);
    }
}

// Binary operations - handles all types including string concatenation
Value binary_op(Value left, int op, Value right) {
    // OP codes: ADD=0, SUB=1, MUL=2, DIV=3, MOD=4, EQ=5, NE=6, LT=7, LE=8, GT=9, GE=10
    
    switch (op) {
        case 0: { // ADD
            // String concatenation
            if (left.type == TYPE_STRING || right.type == TYPE_STRING) {
                char buf[1024];
                char left_str[512], right_str[512];
                
                // Convert left to string
                if (left.type == TYPE_STRING) {
                    strncpy(left_str, (char*)left.data, sizeof(left_str) - 1);
                    left_str[sizeof(left_str) - 1] = '\0';
                } else if (left.type == TYPE_INT) {
                    snprintf(left_str, sizeof(left_str), "%ld", left.data);
                } else if (left.type == TYPE_FLOAT) {
                    double f = *(double*)&left.data;
                    snprintf(left_str, sizeof(left_str), "%g", f);
                } else {
                    left_str[0] = '\0';
                }
                
                // Convert right to string
                if (right.type == TYPE_STRING) {
                    strncpy(right_str, (char*)right.data, sizeof(right_str) - 1);
                    right_str[sizeof(right_str) - 1] = '\0';
                } else if (right.type == TYPE_INT) {
                    snprintf(right_str, sizeof(right_str), "%ld", right.data);
                } else if (right.type == TYPE_FLOAT) {
                    double f = *(double*)&right.data;
                    snprintf(right_str, sizeof(right_str), "%g", f);
                } else {
                    right_str[0] = '\0';
                }
                
                // Concatenate
                snprintf(buf, sizeof(buf), "%s%s", left_str, right_str);
                char *result_str = strdup(buf);
                Value result = {TYPE_STRING, (long)result_str};
                return result;
            }
            
            // Numeric addition
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
            if (left.type == TYPE_INT && right.type == TYPE_INT) {
                long quot = left.data / right.data;
                Value result = {TYPE_INT, quot};
                return result;
            } else {
                double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
                double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
                double quot = l / r;
                Value result = {TYPE_FLOAT, *(long*)&quot};
                return result;
            }
        }
        
        case 4: { // MOD
            long rem = left.data % right.data;
            Value result = {TYPE_INT, rem};
            return result;
        }
        
        case 5: { // EQ
            if (left.type != right.type) {
                Value result = {TYPE_INT, 0};
                return result;
            }
            if (left.type == TYPE_STRING) {
                int eq = strcmp((char*)left.data, (char*)right.data) == 0;
                Value result = {TYPE_INT, eq};
                return result;
            } else {
                int eq = (left.data == right.data);
                Value result = {TYPE_INT, eq};
                return result;
            }
        }
        
        case 6: { // NE
            if (left.type != right.type) {
                Value result = {TYPE_INT, 1};
                return result;
            }
            if (left.type == TYPE_STRING) {
                int ne = strcmp((char*)left.data, (char*)right.data) != 0;
                Value result = {TYPE_INT, ne};
                return result;
            } else {
                int ne = (left.data != right.data);
                Value result = {TYPE_INT, ne};
                return result;
            }
        }
        
        case 7: { // LT
            if (left.type == TYPE_STRING && right.type == TYPE_STRING) {
                int lt = strcmp((char*)left.data, (char*)right.data) < 0;
                Value result = {TYPE_INT, lt};
                return result;
            } else if (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) {
                double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
                double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
                Value result = {TYPE_INT, l < r};
                return result;
            } else {
                Value result = {TYPE_INT, left.data < right.data};
                return result;
            }
        }
        
        case 8: { // LE
            if (left.type == TYPE_STRING && right.type == TYPE_STRING) {
                int le = strcmp((char*)left.data, (char*)right.data) <= 0;
                Value result = {TYPE_INT, le};
                return result;
            } else if (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) {
                double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
                double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
                Value result = {TYPE_INT, l <= r};
                return result;
            } else {
                Value result = {TYPE_INT, left.data <= right.data};
                return result;
            }
        }
        
        case 9: { // GT
            if (left.type == TYPE_STRING && right.type == TYPE_STRING) {
                int gt = strcmp((char*)left.data, (char*)right.data) > 0;
                Value result = {TYPE_INT, gt};
                return result;
            } else if (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) {
                double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
                double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
                Value result = {TYPE_INT, l > r};
                return result;
            } else {
                Value result = {TYPE_INT, left.data > right.data};
                return result;
            }
        }
        
        case 10: { // GE
            if (left.type == TYPE_STRING && right.type == TYPE_STRING) {
                int ge = strcmp((char*)left.data, (char*)right.data) >= 0;
                Value result = {TYPE_INT, ge};
                return result;
            } else if (left.type == TYPE_FLOAT || right.type == TYPE_FLOAT) {
                double l = (left.type == TYPE_FLOAT) ? *(double*)&left.data : (double)left.data;
                double r = (right.type == TYPE_FLOAT) ? *(double*)&right.data : (double)right.data;
                Value result = {TYPE_INT, l >= r};
                return result;
            } else {
                Value result = {TYPE_INT, left.data >= right.data};
                return result;
            }
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
                result_arr->capacity *= 2;
                result_arr->data = realloc(result_arr->data, result_arr->capacity * sizeof(Value));
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
        fprintf(stderr, "regexp_replace requires three string arguments\n");
        exit(1);
    }

    char *pattern = (char*)pattern_val.data;
    char *str = (char*)str_val.data;
    char *replacement = (char*)replacement_val.data;

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        fprintf(stderr, "Failed to compile regex: %s\n", pattern);
        regfree(&regex);
        // Return original string
        Value result = {TYPE_STRING, (long)strdup(str)};
        return result;
    }

    // Build result string
    char *result_str = (char*)malloc(4096);
    result_str[0] = '\0';
    int result_pos = 0;

    regmatch_t match;
    char *search_str = str;
    int offset = 0;

    while (regexec(&regex, search_str, 1, &match, 0) == 0) {
        // Copy text before match
        int pre_len = match.rm_so;
        if (result_pos + pre_len < 4096) {
            strncat(result_str + result_pos, search_str, pre_len);
            result_pos += pre_len;
        }

        // Copy replacement
        int repl_len = strlen(replacement);
        if (result_pos + repl_len < 4096) {
            strcat(result_str + result_pos, replacement);
            result_pos += repl_len;
        }

        // Move to next position
        search_str += match.rm_eo;

        // Prevent infinite loop on zero-length matches
        if (match.rm_eo == 0) {
            if (*search_str == '\0') break;
            if (result_pos < 4095) {
                result_str[result_pos++] = *search_str;
                result_str[result_pos] = '\0';
            }
            search_str++;
        }
    }

    // Copy remaining text
    if (result_pos < 4096) {
        strcat(result_str + result_pos, search_str);
    }

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
            result_arr->capacity *= 2;
            result_arr->data = realloc(result_arr->data, result_arr->capacity * sizeof(Value));
        }

        Value part_val = {TYPE_STRING, (long)part};
        ((Value*)result_arr->data)[result_arr->size++] = part_val;

        // Move to position after separator
        current = next + sep_len;
    }

    // Add remaining part after last separator
    char *last_part = strdup(current);
    if (result_arr->size >= result_arr->capacity) {
        result_arr->capacity *= 2;
        result_arr->data = realloc(result_arr->data, result_arr->capacity * sizeof(Value));
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
            default:
                printf("<object>");
        }
    }
}
