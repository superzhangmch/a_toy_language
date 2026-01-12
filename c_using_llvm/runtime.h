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

// Value structure matching LLVM IR
typedef struct {
    int type;
    long data;
} Value;

// Class/instance helpers
typedef Value (*MethodFn)(Value this_val, Value *args, int arg_count);
typedef Value (*FieldInitFn)(Value this_val);

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
Value read(Value filename);
Value write(Value content, Value filename);

// Dict functions
Value make_dict(void);
Value dict_set(Value dict, Value key, Value val);
Value dict_get(Value dict, Value key);
Value dict_has(Value dict, Value key);
Value dict_keys(Value dict);
Value keys(Value dict);  // Alias for dict_keys (matches builtin name)

// IN operator (element in array, key in dict, substring in string)
Value in_operator(Value left, Value right);

// Binary operations (handles all types including string concatenation)
Value binary_op(Value left, int op, Value right);

// Regular expression functions
Value regexp_match(Value pattern, Value str);
Value regexp_find(Value pattern, Value str);
Value regexp_replace(Value pattern, Value str, Value replacement);

// String functions
Value str_split(Value str, Value separator);
Value str_join(Value arr, Value separator);

// JSON helpers
Value json_encode(Value json_str);
Value json_decode(Value v);
Value json_decode_ctx(Value v, int line, char *file);

// Exception helpers
void* __try_push_buf(void);
void __try_pop(void);
void __raise(Value msg, int line, char *file);
Value __get_exception(void);

// Print function (for LLVM codegen)
void print_value(Value v);

// Misc helpers
Value remove_entry(Value obj, Value key_or_index);
Value math_fn(Value op, Value a, Value b, int arg_count);

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
