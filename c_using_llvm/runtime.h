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

#endif
