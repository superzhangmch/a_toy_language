#ifndef RUNTIME_H
#define RUNTIME_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Type tags
#define TYPE_INT 0
#define TYPE_FLOAT 1
#define TYPE_STRING 2
#define TYPE_ARRAY 3
#define TYPE_DICT 4
#define TYPE_CLASS 5
#define TYPE_INSTANCE 6
#define TYPE_NULL 7
#define TYPE_BOOL 8

// Value structure matching LLVM IR
typedef struct {
    int type;
    long data;
} Value;

// Array structure
typedef struct Array {
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

typedef struct Dict {
    DictEntry **buckets;
    int size;
} Dict;

// Class/instance helpers
typedef Value (*MethodFn)(Value this_val, Value *args, int arg_count);
typedef Value (*FieldInitFn)(Value this_val);

typedef struct MethodEntry {
    char *name;
    MethodFn fn;
    int arity;
    int is_private;
} MethodEntry;

typedef struct FieldEntry {
    char *name;
    FieldInitFn init_fn;
    int is_private;
} FieldEntry;

typedef struct Class {
    char *name;
    MethodEntry *methods;
    int method_count;
    int method_capacity;
    FieldEntry *fields;
    int field_count;
    int field_capacity;
} Class;

typedef struct Instance {
    Class *cls;
    Value fields; // dict value storing member fields
} Instance;

// Runtime functions
Value make_array(void);
Value append(Value arr, Value val);
Value array_get(Value arr, Value index);
Value array_set(Value arr, Value index, Value val);
Value index_get(Value obj, Value index);  // Generic index access for array/dict/string
Value index_set(Value obj, Value index, Value val);  // Generic index assignment for array/dict
Value len(Value v);
Value to_int(Value v);
Value to_float(Value v);
Value to_string(Value v);
Value str(Value v);
Value make_null(void);
Value type(Value v);
Value slice_access(Value obj, Value start_v, Value end_v);
Value input(Value prompt);
Value file_read(Value filename);
Value file_write(Value content, Value filename);
Value file_append(Value content, Value filename);
Value file_size(Value filename);
Value file_exist(Value filename);

// Dict functions
Value make_dict(void);
Value dict_set(Value dict, Value key, Value val);
Value dict_get(Value dict, Value key);
Value dict_has(Value dict, Value key);
Value dict_keys(Value dict);
Value keys(Value dict);  // Alias for dict_keys (matches builtin name)

// IN operator (element in array, key in dict, substring in string)
Value in_operator(Value left, Value right, int line, const char *file);
Value not_in_operator(Value left, Value right, int line, const char *file);

// Binary operations (handles all types including string concatenation)
Value binary_op(Value left, int op, Value right, int line, const char *file);

// Regular expression functions
Value regexp_match(Value pattern, Value str);
Value regexp_find(Value pattern, Value str);
Value regexp_replace(Value pattern, Value str, Value replacement);

// String functions
Value str_split(Value str, Value separator);
Value str_join(Value arr, Value separator);
Value str_trim(Value str, Value chars);
Value str_format(Value fmt, Value *args, int arg_count);
void set_source_ctx(int line, const char *file);

// Math functions
Value math_sin(Value a);
Value math_cos(Value a);
Value math_asin(Value a);
Value math_acos(Value a);
Value math_log(Value a);
Value math_exp(Value a);
Value math_ceil(Value a);
Value math_floor(Value a);
Value math_round(Value a);
Value math_pow_val(Value a, Value b);
Value math_random_val(Value a, Value b, int arg_count);
Value math_sqrt(Value a);

// JSON helpers
Value json_encode(Value json_str);
Value json_decode_ctx(Value v, int line, char *file);
Value json_decode(Value v);

// Exception helpers
void* __try_push_buf(void);
void __try_pop(void);
void __raise(Value msg, int line, char *file);
Value __get_exception(void);

// Print function (for LLVM codegen)
void print_value(Value v);

// Misc helpers
Value remove_entry(Value obj, Value key_or_index);

// Class/object runtime
Value make_class(char *name);
void class_add_field(Value class_val, char *name, FieldInitFn init_fn, int is_private);
void class_add_method(Value class_val, char *name, MethodFn fn, int arity, int is_private);
Value instantiate_class(Value class_val, Value *args, int arg_count);
Value member_get(Value instance, char *name);
Value member_set(Value instance, char *name, Value val);
Value method_call(Value instance, char *name, Value *args, int arg_count);

// Command line arguments
void set_cmd_args(int argc, char **argv);
Value cmd_args(void);

#endif
