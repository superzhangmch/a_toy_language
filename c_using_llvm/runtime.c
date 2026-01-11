#include "runtime.h"

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
