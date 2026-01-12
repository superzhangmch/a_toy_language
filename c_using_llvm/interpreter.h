#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ast.h"

typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_STRING,
    VAL_BOOL,
    VAL_ARRAY,
    VAL_DICT,
    VAL_FUNC,
    VAL_BUILTIN,
    VAL_CLASS,
    VAL_INSTANCE,
    VAL_NULL
} ValueType;

typedef struct Value Value;
typedef struct Environment Environment;
typedef struct Function Function;
typedef struct Array Array;
typedef struct Dict Dict;
typedef struct ClassValue ClassValue;
typedef struct ClassInstance ClassInstance;

typedef Value *(*BuiltinFunc)(Value **args, int arg_count);

struct Value {
    ValueType type;
    union {
        int int_val;
        double float_val;
        char *string_val;
        int bool_val;
        Array *array_val;
        Dict *dict_val;
        Function *func_val;
        BuiltinFunc builtin_val;
        ClassValue *class_val;
        ClassInstance *instance_val;
    } data;
};

struct Array {
    Value **elements;
    int size;
    int capacity;
};

typedef struct DictEntry {
    char *key;
    Value *value;
    struct DictEntry *next;
} DictEntry;

struct Dict {
    DictEntry **buckets;
    int size;
};

struct Function {
    char *name;
    ASTNodeList *params;
    ASTNodeList *body;
    Environment *env;
};

struct ClassValue {
    char *name;
    ASTNodeList *members;  // Var declarations
    ASTNodeList *methods;  // Function definitions
    Environment *env;      // Defining environment
};

struct ClassInstance {
    ClassValue *class_val;
    Dict *fields;
};

typedef struct EnvEntry {
    char *name;
    Value *value;
    struct EnvEntry *next;
} EnvEntry;

struct Environment {
    EnvEntry **buckets;
    int size;
    Environment *parent;
};

/* Value functions */
Value *create_int_value(int val);
Value *create_float_value(double val);
Value *create_string_value(char *val);
Value *create_bool_value(int val);
Value *create_array_value();
Value *create_dict_value();
Value *create_func_value(char *name, ASTNodeList *params, ASTNodeList *body, Environment *env);
Value *create_builtin_value(BuiltinFunc func);
Value *create_null_value();

/* Array functions */
void array_append(Array *arr, Value *val);
Value *array_get(Array *arr, int index);
void array_set(Array *arr, int index, Value *val);

/* Dict functions */
void dict_set(Dict *dict, char *key, Value *val);
Value *dict_get(Dict *dict, char *key);
char **dict_keys(Dict *dict, int *count);

/* Environment functions */
Environment *create_environment(Environment *parent);
void env_define(Environment *env, char *name, Value *val);
Value *env_get(Environment *env, char *name);
void env_set(Environment *env, char *name, Value *val);

/* Interpreter functions */
void set_cmd_args(int argc, char **argv);
void interpret(ASTNode *root);

#endif /* INTERPRETER_H */
