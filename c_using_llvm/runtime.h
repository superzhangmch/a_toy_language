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

// Value structure matching LLVM IR
typedef struct {
    int type;
    long data;
} Value;

// Runtime functions
Value append(Value arr, Value val);
Value array_get(Value arr, Value index);
Value array_set(Value arr, Value index, Value val);
Value len(Value v);
Value to_int(Value v);
Value to_float(Value v);
Value to_string(Value v);

#endif
