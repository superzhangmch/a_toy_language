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

// Value structure matching LLVM IR
typedef struct {
    int type;
    long data;
} Value;

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

// Print function (for LLVM codegen)
void print_value(Value v);

// Command line arguments
void set_cmd_args(int argc, char **argv);
Value cmd_args(void);

#endif
