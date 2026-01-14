/*
 * Interpreter for Tiny Language
 *
 * This version uses runtime.c for all Value operations, built-in functions,
 * and memory management (GC). The interpreter only needs to handle:
 * - AST traversal and evaluation
 * - Variable scoping (Environment)
 * - Control flow (if/while/for/break/continue/return)
 * - Function calls and class instantiation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdarg.h>
#include <math.h>
#include "interpreter.h"
#include "ast.h"
#include "runtime.h"
#include "gc.h"

// ============================================================================
// Global state
// ============================================================================

// Custom interpreter-only types (not in runtime.h)
#define TYPE_FUNC 100  // User-defined functions

// Control flow
static jmp_buf break_jmp;
static jmp_buf continue_jmp;
static int has_returned;
static Value return_value;

// Exception handling
static jmp_buf exception_stack[256];
static int exception_top = 0;
static Value exception_value;

// Environment
static Environment *global_env;
static Environment *current_env;

// Loop and class context
static Environment *loop_env_stack[256];
static int loop_env_top = 0;
static Instance *this_stack[256];
static int this_stack_top = 0;

// Error context
static int err_line = -1;
static const char *err_file = NULL;

// ============================================================================
// Forward declarations
// ============================================================================

static Value eval_expression(ASTNode *node);
static void eval_statement(ASTNode *node);
static void execute_block(ASTNodeList *stmts);
static Value call_function(Function *func, Value *args, int arg_count);
static Value call_method_internal(Value instance_val, const char *method_name, Value *args, int arg_count);

// GC support: Mark all values in an environment
static void mark_environment(Environment *env) {
    if (!env) return;

    // Mark all values in this environment
    for (int i = 0; i < HASH_SIZE; i++) {
        for (EnvEntry *e = env->buckets[i]; e != NULL; e = e->next) {
            gc_mark_value(&e->value);
        }
    }

    // Recursively mark parent environment
    mark_environment(env->parent);
}

// GC support: Mark global roots (called before GC collection)
void gc_mark_interpreter_roots(void) {
    // Mark global and current environments
    if (global_env) {
        mark_environment(global_env);
    }
    if (current_env && current_env != global_env) {
        mark_environment(current_env);
    }

    // Mark loop environment stack
    for (int i = 0; i < loop_env_top; i++) {
        if (loop_env_stack[i]) {
            mark_environment(loop_env_stack[i]);
        }
    }

    // Mark this_stack (instance objects)
    for (int i = 0; i < this_stack_top; i++) {
        if (this_stack[i]) {
            gc_mark_value(&this_stack[i]->fields);
        }
    }

    // Mark return value if set
    if (has_returned) {
        gc_mark_value(&return_value);
    }

    // Mark exception value if set
    if (exception_top > 0) {
        gc_mark_value(&exception_value);
    }
}

// ============================================================================
// Utility functions
// ============================================================================

static void set_error_ctx(int line, const char *file) {
    err_line = line;
    err_file = file ? file : "<input>";
}

static __attribute__((noreturn)) void runtime_error(const char *fmt, ...) {
    fprintf(stderr, "Error at %s:%d: ", err_file ? err_file : "<input>", err_line >= 0 ? err_line : 0);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % 256;  // Use a power of 2 for environments
}

// ============================================================================
// Environment implementation
// ============================================================================

Environment *create_environment(Environment *parent) {
    Environment *env = malloc(sizeof(Environment));
    env->buckets = calloc(HASH_SIZE, sizeof(EnvEntry*));
    env->size = 0;
    env->parent = parent;
    return env;
}

void env_define(Environment *env, char *name, Value val) {
    unsigned int idx = hash_string(name);

    // Check if already defined in current scope
    for (EnvEntry *e = env->buckets[idx]; e != NULL; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            runtime_error("Redefinition of '%s' in the same scope", name);
        }
    }

    // Add new entry
    EnvEntry *entry = malloc(sizeof(EnvEntry));
    entry->name = strdup(name);
    entry->value = val;
    entry->next = env->buckets[idx];
    env->buckets[idx] = entry;
    env->size++;
}

int env_exists(Environment *env, char *name) {
    unsigned int idx = hash_string(name);
    for (EnvEntry *e = env->buckets[idx]; e != NULL; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            return 1;
        }
    }
    if (env->parent) {
        return env_exists(env->parent, name);
    }
    return 0;
}

Value env_get(Environment *env, char *name) {
    unsigned int idx = hash_string(name);
    for (EnvEntry *e = env->buckets[idx]; e != NULL; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            return e->value;
        }
    }
    if (env->parent) {
        return env_get(env->parent, name);
    }
    runtime_error("Undefined variable: %s", name);
}

void env_set(Environment *env, char *name, Value val) {
    unsigned int idx = hash_string(name);
    for (EnvEntry *e = env->buckets[idx]; e != NULL; e = e->next) {
        if (strcmp(e->name, name) == 0) {
            e->value = val;
            return;
        }
    }
    if (env->parent) {
        env_set(env->parent, name, val);
        return;
    }
    runtime_error("Undefined variable: %s", name);
}

// ============================================================================
// Expression evaluation
// ============================================================================

static Value eval_literal(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    switch (node->type) {
        case NODE_INT_LITERAL:
            return (Value){TYPE_INT, node->data.int_literal.value};

        case NODE_FLOAT_LITERAL: {
            double d = node->data.float_literal.value;
            return (Value){TYPE_FLOAT, *(long*)&d};
        }

        case NODE_STRING_LITERAL: {
            int len = strlen(node->data.string_literal.value);
            char *str = gc_alloc(TYPE_STRING, len + 1);
            strcpy(str, node->data.string_literal.value);
            return (Value){TYPE_STRING, (long)str};
        }

        case NODE_BOOL_LITERAL:
            return (Value){TYPE_BOOL, node->data.bool_literal.value ? 1 : 0};

        case NODE_NULL_LITERAL:
            return (Value){TYPE_NULL, 0};

        default:
            runtime_error("Unknown literal type");
    }
}

static Value eval_array_literal(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    Value arr = make_array();
    ASTNodeList *elem = node->data.array_literal.elements;

    while (elem) {
        Value val = eval_expression(elem->node);
        arr = append(arr, val);
        elem = elem->next;
    }

    return arr;
}

static Value eval_dict_literal(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    Value dict = make_dict();
    ASTNodeList *pair = node->data.dict_literal.pairs;

    while (pair) {
        ASTNode *pair_node = pair->node;

        // Evaluate key (must be string)
        Value key = eval_expression(pair_node->data.dict_pair.key);
        if (key.type != TYPE_STRING) {
            runtime_error("Dictionary key must be a string");
        }

        // Evaluate value
        Value val = eval_expression(pair_node->data.dict_pair.value);

        // Set in dict
        dict = dict_set(dict, key, val);
        pair = pair->next;
    }

    return dict;
}

static Value eval_identifier(ASTNode *node) {
    set_error_ctx(node->line, node->file);
    return env_get(current_env, node->data.identifier.name);
}

static int is_truthy(Value v) {
    if (v.type == TYPE_BOOL) return (int)v.data;
    if (v.type == TYPE_NULL) return 0;
    if (v.type == TYPE_INT) return v.data != 0;
    if (v.type == TYPE_FLOAT) {
        double d = *(double*)&v.data;
        return d != 0.0;
    }
    if (v.type == TYPE_STRING) {
        char *str = (char*)v.data;
        return str && strlen(str) > 0;
    }
    if (v.type == TYPE_ARRAY) {
        Array *arr = (Array*)v.data;
        return arr && arr->size > 0;
    }
    if (v.type == TYPE_DICT) {
        Dict *dict = (Dict*)v.data;
        return dict && dict->size > 0;
    }
    return 1;
}

static Value eval_binary_op(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    Operator op = node->data.binary_op.op;

    // Short-circuit evaluation for logical operators
    if (op == OP_AND) {
        Value left = eval_expression(node->data.binary_op.left);
        if (!is_truthy(left)) {
            return (Value){TYPE_BOOL, 0};
        }
        Value right = eval_expression(node->data.binary_op.right);
        return (Value){TYPE_BOOL, is_truthy(right) ? 1 : 0};
    }

    if (op == OP_OR) {
        Value left = eval_expression(node->data.binary_op.left);
        if (is_truthy(left)) {
            return (Value){TYPE_BOOL, 1};
        }
        Value right = eval_expression(node->data.binary_op.right);
        return (Value){TYPE_BOOL, is_truthy(right) ? 1 : 0};
    }

    // Handle IN and NOT_IN operators
    if (op == OP_IN) {
        Value left = eval_expression(node->data.binary_op.left);
        Value right = eval_expression(node->data.binary_op.right);
        return in_operator(left, right, node->line, node->file);
    }

    if (op == OP_NOT_IN) {
        Value left = eval_expression(node->data.binary_op.left);
        Value right = eval_expression(node->data.binary_op.right);
        return not_in_operator(left, right, node->line, node->file);
    }

    // Regular operators - evaluate both sides
    Value left = eval_expression(node->data.binary_op.left);
    Value right = eval_expression(node->data.binary_op.right);

    // Use runtime.c's binary_op function
    return binary_op(left, (int)op, right, node->line, node->file);
}

static Value eval_unary_op(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    Operator op = node->data.unary_op.op;
    Value operand = eval_expression(node->data.unary_op.operand);

    if (op == OP_NEG) {
        if (operand.type == TYPE_INT) {
            return (Value){TYPE_INT, -operand.data};
        } else if (operand.type == TYPE_FLOAT) {
            double d = *(double*)&operand.data;
            d = -d;
            return (Value){TYPE_FLOAT, *(long*)&d};
        } else {
            runtime_error("Unary minus requires a number");
        }
    } else if (op == OP_NOT) {
        return (Value){TYPE_BOOL, is_truthy(operand) ? 0 : 1};
    }

    runtime_error("Unknown unary operator");
}

static Value eval_index_access(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    Value obj = eval_expression(node->data.index_access.object);
    Value index = eval_expression(node->data.index_access.index);

    // Use runtime.c's index_get
    return index_get(obj, index);
}

static Value eval_slice_access(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    Value obj = eval_expression(node->data.slice_access.object);
    Value start = eval_expression(node->data.slice_access.start);
    Value end = eval_expression(node->data.slice_access.end);

    // Use runtime.c's slice_access
    return slice_access(obj, start, end);
}

static Value eval_member_access(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    Value obj = eval_expression(node->data.member_access.object);

    if (obj.type != TYPE_INSTANCE) {
        runtime_error("Member access requires an instance");
    }

    Instance *inst = (Instance*)obj.data;
    char *member_name = node->data.member_access.member;

    // Create a string Value for the field name
    char *field_key = gc_alloc(TYPE_STRING, strlen(member_name) + 1);
    strcpy(field_key, member_name);
    Value field_name = {TYPE_STRING, (long)field_key};

    // Use index_get on the fields dict
    return index_get(inst->fields, field_name);
}

static Value eval_function_call(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    char *func_name = node->data.func_call.name;

    // Count arguments
    int arg_count = 0;
    ASTNodeList *arg_node = node->data.func_call.arguments;
    while (arg_node) {
        arg_count++;
        arg_node = arg_node->next;
    }

    // Evaluate arguments
    Value *args = NULL;
    if (arg_count > 0) {
        args = gc_alloc(TYPE_ARRAY, arg_count * sizeof(Value));
        arg_node = node->data.func_call.arguments;
        for (int i = 0; i < arg_count; i++) {
            args[i] = eval_expression(arg_node->node);
            arg_node = arg_node->next;
        }
    }

    // Map built-in function names to runtime.c functions
    #define BUILTIN1(name, func) \
        if (strcmp(func_name, name) == 0) { \
            if (arg_count != 1) runtime_error("%s requires 1 argument", name); \
            return func(args[0]); \
        }

    #define BUILTIN2(name, func) \
        if (strcmp(func_name, name) == 0) { \
            if (arg_count != 2) runtime_error("%s requires 2 arguments", name); \
            return func(args[0], args[1]); \
        }

    #define BUILTIN3(name, func) \
        if (strcmp(func_name, name) == 0) { \
            if (arg_count != 3) runtime_error("%s requires 3 arguments", name); \
            return func(args[0], args[1], args[2]); \
        }

    // I/O functions
    if (strcmp(func_name, "print") == 0) {
        for (int i = 0; i < arg_count; i++) {
            print_value(args[i]);
            if (i < arg_count - 1) printf(" ");
        }
        return make_null();
    }
    if (strcmp(func_name, "println") == 0) {
        for (int i = 0; i < arg_count; i++) {
            print_value(args[i]);
            if (i < arg_count - 1) printf(" ");
        }
        printf("\n");
        return make_null();
    }

    // Type conversion (1 arg)
    BUILTIN1("int", to_int)
    BUILTIN1("float", to_float)
    BUILTIN1("str", to_string)
    BUILTIN1("type", type)

    // Array/Dict/String operations
    BUILTIN1("len", len)
    BUILTIN2("append", append)
    BUILTIN2("remove", remove_entry)
    BUILTIN2("split", str_split)
    BUILTIN2("str_split", str_split)
    BUILTIN2("join", str_join)
    BUILTIN2("str_join", str_join)
    BUILTIN1("keys", keys)

    // str_trim (1 or 2 args)
    if (strcmp(func_name, "str_trim") == 0) {
        if (arg_count == 1) {
            return str_trim(args[0], make_null());
        } else if (arg_count == 2) {
            return str_trim(args[0], args[1]);
        } else {
            runtime_error("str_trim requires 1 or 2 arguments");
        }
    }

    // str_format (variable args)
    if (strcmp(func_name, "str_format") == 0) {
        if (arg_count == 0) runtime_error("str_format requires at least 1 argument");
        return str_format(args[0], args + 1, arg_count - 1);
    }

    // I/O
    BUILTIN1("input", input)
    BUILTIN1("read", file_read)
    BUILTIN2("write", file_write)
    BUILTIN1("file_read", file_read)
    BUILTIN2("file_write", file_write)
    BUILTIN2("file_append", file_append)
    BUILTIN1("file_size", file_size)
    BUILTIN1("file_exist", file_exist)

    // Math (1 arg)
    BUILTIN1("sin", math_sin)
    BUILTIN1("cos", math_cos)
    BUILTIN1("asin", math_asin)
    BUILTIN1("acos", math_acos)
    BUILTIN1("log", math_log)
    BUILTIN1("sqrt", math_sqrt)
    BUILTIN1("exp", math_exp)
    BUILTIN1("ceil", math_ceil)
    BUILTIN1("floor", math_floor)

    // Round (1 or 2 args)
    if (strcmp(func_name, "round") == 0) {
        if (arg_count == 1) {
            return math_round(args[0]);
        } else if (arg_count == 2) {
            // Round with precision
            double val = (args[0].type == TYPE_FLOAT) ? *(double*)&args[0].data : (double)args[0].data;
            long precision = (args[1].type == TYPE_INT) ? args[1].data : 0;
            double multiplier = pow(10.0, precision);
            double rounded = round(val * multiplier) / multiplier;
            return (Value){TYPE_FLOAT, *(long*)&rounded};
        } else {
            runtime_error("round requires 1 or 2 arguments");
        }
    }

    // Math (2 args)
    BUILTIN2("pow", math_pow_val)

    // Random (0 or 2 args)
    if (strcmp(func_name, "random") == 0) {
        if (arg_count == 0) {
            return math_random_val(make_null(), make_null(), 0);
        } else if (arg_count == 2) {
            return math_random_val(args[0], args[1], 2);
        } else {
            runtime_error("random requires 0 or 2 arguments");
        }
    }

    // JSON (1 arg)
    BUILTIN1("json_parse", json_decode)
    BUILTIN1("json_decode", json_decode)
    BUILTIN1("json_stringify", json_encode)
    BUILTIN1("json_encode", json_encode)

    // Regex (2 args for match/find, 3 for replace)
    BUILTIN2("regex_match", regexp_match)
    BUILTIN2("regexp_match", regexp_match)
    BUILTIN3("regex_replace", regexp_replace)
    BUILTIN3("regexp_replace", regexp_replace)
    BUILTIN2("regex_find", regexp_find)
    BUILTIN2("regexp_find", regexp_find)

    // GC
    if (strcmp(func_name, "gc_run") == 0) {
        return gc_run();
    }
    if (strcmp(func_name, "gc_stat") == 0 || strcmp(func_name, "gc_stats") == 0) {
        gc_print_stats();
        return make_null();
    }

    // User-defined function
    if (env_exists(current_env, func_name)) {
        Value func_val = env_get(current_env, func_name);

        if (func_val.type == TYPE_CLASS) {
            // Constructor call (via function syntax)
            runtime_error("Use 'new' to instantiate a class");
        }

        // It's a user function stored in the environment
        Function *func = (Function*)func_val.data;
        return call_function(func, args, arg_count);
    }

    runtime_error("Undefined function: %s", func_name);
}

static Value call_function(Function *func, Value *args, int arg_count) {
    // Count expected parameters
    int param_count = 0;
    ASTNodeList *param = func->params;
    while (param) {
        param_count++;
        param = param->next;
    }

    if (arg_count != param_count) {
        runtime_error("Function '%s' expects %d arguments, got %d",
                     func->name, param_count, arg_count);
    }

    // Create new environment for function
    Environment *func_env = create_environment(func->env);
    Environment *saved_env = current_env;
    current_env = func_env;

    // Bind parameters
    param = func->params;
    for (int i = 0; i < arg_count; i++) {
        env_define(func_env, param->node->data.identifier.name, args[i]);
        param = param->next;
    }

    // Execute function body
    has_returned = 0;
    execute_block(func->body);

    Value result = has_returned ? return_value : make_null();
    has_returned = 0;

    current_env = saved_env;
    return result;
}

// Continue with rest of eval functions in next part...
static Value eval_method_call(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    Value obj = eval_expression(node->data.method_call.object);
    char *method_name = node->data.method_call.method;

    // Count and evaluate arguments
    int arg_count = 0;
    ASTNodeList *arg_node = node->data.method_call.arguments;
    while (arg_node) {
        arg_count++;
        arg_node = arg_node->next;
    }

    Value *args = NULL;
    if (arg_count > 0) {
        args = gc_alloc(TYPE_ARRAY, arg_count * sizeof(Value));
        arg_node = node->data.method_call.arguments;
        for (int i = 0; i < arg_count; i++) {
            args[i] = eval_expression(arg_node->node);
            arg_node = arg_node->next;
        }
    }

    return call_method_internal(obj, method_name, args, arg_count);
}

static Value call_method_internal(Value instance_val, const char *method_name, Value *args, int arg_count) {
    if (instance_val.type != TYPE_INSTANCE) {
        runtime_error("Method call requires an instance");
    }

    Instance *inst = (Instance*)instance_val.data;
    ClassValue *cls = (ClassValue*)inst->cls;

    // Find method in class
    ASTNodeList *method = cls->methods;
    while (method) {
        if (method->node->type == NODE_FUNC_DEF) {
            if (strcmp(method->node->data.func_def.name, method_name) == 0) {
                // Found the method
                Function func;
                func.name = method->node->data.func_def.name;
                func.params = method->node->data.func_def.params;
                func.body = method->node->data.func_def.body;
                func.env = cls->env;

                // Push 'this' context
                this_stack[this_stack_top++] = inst;

                // Create method environment with 'this'
                Environment *method_env = create_environment(func.env);
                env_define(method_env, "this", instance_val);

                Environment *saved_env = current_env;
                current_env = method_env;

                // Bind parameters
                ASTNodeList *param = func.params;
                for (int i = 0; i < arg_count; i++) {
                    if (!param) {
                        runtime_error("Too many arguments for method '%s'", method_name);
                    }
                    env_define(method_env, param->node->data.identifier.name, args[i]);
                    param = param->next;
                }

                if (param) {
                    runtime_error("Not enough arguments for method '%s'", method_name);
                }

                // Execute method
                has_returned = 0;
                execute_block(func.body);

                Value result = has_returned ? return_value : make_null();
                has_returned = 0;

                current_env = saved_env;
                this_stack_top--;

                return result;
            }
        }
        method = method->next;
    }

    runtime_error("Method '%s' not found", method_name);
}

static Value eval_new_expr(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    char *class_name = node->data.new_expr.class_name;

    // Get class from environment
    if (!env_exists(current_env, class_name)) {
        runtime_error("Undefined class: %s", class_name);
    }

    Value class_val = env_get(current_env, class_name);
    if (class_val.type != TYPE_CLASS) {
        runtime_error("'%s' is not a class", class_name);
    }

    ClassValue *cls = (ClassValue*)class_val.data;

    // Create instance
    Instance *inst = gc_alloc(TYPE_INSTANCE, sizeof(Instance));
    inst->cls = (Class*)cls;  // Store ClassValue as Class (interpreter-specific)
    inst->fields = make_dict();

    Value instance_val = {TYPE_INSTANCE, (long)inst};

    // Initialize fields from class members
    ASTNodeList *member = cls->members;
    while (member) {
        if (member->node->type == NODE_VAR_DECL) {
            char *field_name = member->node->data.var_decl.name;
            Value field_val = member->node->data.var_decl.value ?
                             eval_expression(member->node->data.var_decl.value) :
                             make_null();

            Value key = {TYPE_STRING, (long)field_name};
            dict_set(inst->fields, key, field_val);
        }
        member = member->next;
    }

    // Count and evaluate constructor arguments
    int arg_count = 0;
    ASTNodeList *arg_node = node->data.new_expr.arguments;
    while (arg_node) {
        arg_count++;
        arg_node = arg_node->next;
    }

    Value *args = NULL;
    if (arg_count > 0) {
        args = gc_alloc(TYPE_ARRAY, arg_count * sizeof(Value));
        arg_node = node->data.new_expr.arguments;
        for (int i = 0; i < arg_count; i++) {
            args[i] = eval_expression(arg_node->node);
            arg_node = arg_node->next;
        }
    }

    // Call init method if it exists
    ASTNodeList *method = cls->methods;
    while (method) {
        if (method->node->type == NODE_FUNC_DEF) {
            if (strcmp(method->node->data.func_def.name, "init") == 0) {
                call_method_internal(instance_val, "init", args, arg_count);
                break;
            }
        }
        method = method->next;
    }

    return instance_val;
}

static Value eval_expression(ASTNode *node) {
    if (!node) {
        return make_null();
    }

    switch (node->type) {
        case NODE_INT_LITERAL:
        case NODE_FLOAT_LITERAL:
        case NODE_STRING_LITERAL:
        case NODE_BOOL_LITERAL:
        case NODE_NULL_LITERAL:
            return eval_literal(node);

        case NODE_IDENTIFIER:
            return eval_identifier(node);

        case NODE_BINARY_OP:
            return eval_binary_op(node);

        case NODE_UNARY_OP:
            return eval_unary_op(node);

        case NODE_ARRAY_LITERAL:
            return eval_array_literal(node);

        case NODE_DICT_LITERAL:
            return eval_dict_literal(node);

        case NODE_INDEX_ACCESS:
            return eval_index_access(node);

        case NODE_SLICE_ACCESS:
            return eval_slice_access(node);

        case NODE_MEMBER_ACCESS:
            return eval_member_access(node);

        case NODE_FUNC_CALL:
            return eval_function_call(node);

        case NODE_METHOD_CALL:
            return eval_method_call(node);

        case NODE_NEW_EXPR:
            return eval_new_expr(node);

        default:
            runtime_error("Unknown expression node type: %d", node->type);
    }
}

// ============================================================================
// Statement execution
// ============================================================================

static void eval_var_decl(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    char *var_name = node->data.var_decl.name;
    Value val = node->data.var_decl.value ?
                eval_expression(node->data.var_decl.value) :
                make_null();

    env_define(current_env, var_name, val);
}

static void eval_assignment(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    ASTNode *target = node->data.assignment.target;
    Value val = eval_expression(node->data.assignment.value);

    if (target->type == NODE_IDENTIFIER) {
        // Simple variable assignment
        env_set(current_env, target->data.identifier.name, val);
    }
    else if (target->type == NODE_INDEX_ACCESS) {
        // Array/dict element assignment
        Value obj = eval_expression(target->data.index_access.object);
        Value index = eval_expression(target->data.index_access.index);
        index_set(obj, index, val);
    }
    else if (target->type == NODE_MEMBER_ACCESS) {
        // Instance field assignment
        Value obj = eval_expression(target->data.member_access.object);
        if (obj.type != TYPE_INSTANCE) {
            runtime_error("Member assignment requires an instance");
        }
        Instance *inst = (Instance*)obj.data;
        Value field_name = {TYPE_STRING, (long)target->data.member_access.member};
        index_set(inst->fields, field_name, val);
    }
    else {
        runtime_error("Invalid assignment target");
    }
}

static void eval_if_stmt(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    Value cond = eval_expression(node->data.if_stmt.condition);

    if (is_truthy(cond)) {
        // Create new scope for then block
        Environment *then_env = create_environment(current_env);
        Environment *saved_env = current_env;
        current_env = then_env;
        execute_block(node->data.if_stmt.then_block);
        current_env = saved_env;
    } else if (node->data.if_stmt.else_block) {
        // Create new scope for else block
        Environment *else_env = create_environment(current_env);
        Environment *saved_env = current_env;
        current_env = else_env;
        execute_block(node->data.if_stmt.else_block);
        current_env = saved_env;
    }
}

static void eval_while_stmt(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    loop_env_stack[loop_env_top++] = current_env;

    if (setjmp(break_jmp) == 0) {
        while (1) {
            Value cond = eval_expression(node->data.while_stmt.condition);
            if (!is_truthy(cond)) break;

            // Create new scope for each iteration
            Environment *iter_env = create_environment(current_env);
            Environment *saved_env = current_env;
            current_env = iter_env;

            if (setjmp(continue_jmp) == 0) {
                execute_block(node->data.while_stmt.body);
            }

            current_env = saved_env;
        }
    }

    loop_env_top--;
}

static void eval_for_stmt(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    char *var_name = node->data.for_stmt.index_var;
    Value start = eval_expression(node->data.for_stmt.start);
    Value end = eval_expression(node->data.for_stmt.end);

    if (start.type != TYPE_INT || end.type != TYPE_INT) {
        runtime_error("For loop range must be integers");
    }

    long start_val = start.data;
    long end_val = end.data;

    // Create loop scope
    Environment *loop_env = create_environment(current_env);
    Environment *saved_env = current_env;
    current_env = loop_env;
    loop_env_stack[loop_env_top++] = loop_env;

    // Define the loop variable once
    env_define(loop_env, var_name, (Value){TYPE_INT, start_val});

    if (setjmp(break_jmp) == 0) {
        if (start_val <= end_val) {
            for (long i = start_val; i <= end_val; i++) {
                env_set(loop_env, var_name, (Value){TYPE_INT, i});

                if (setjmp(continue_jmp) == 0) {
                    execute_block(node->data.for_stmt.body);
                }
            }
        } else {
            for (long i = start_val; i >= end_val; i--) {
                env_set(loop_env, var_name, (Value){TYPE_INT, i});

                if (setjmp(continue_jmp) == 0) {
                    execute_block(node->data.for_stmt.body);
                }
            }
        }
    }

    loop_env_top--;
    current_env = saved_env;
}

static void eval_foreach_stmt(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    char *key_var = node->data.foreach_stmt.key_var;
    char *value_var = node->data.foreach_stmt.value_var;
    Value collection = eval_expression(node->data.foreach_stmt.collection);

    // Create loop scope
    Environment *loop_env = create_environment(current_env);
    Environment *saved_env = current_env;
    current_env = loop_env;
    loop_env_stack[loop_env_top++] = loop_env;

    // Define loop variables once
    env_define(loop_env, key_var, make_null());
    env_define(loop_env, value_var, make_null());

    if (collection.type == TYPE_ARRAY) {
        Array *arr = (Array*)collection.data;
        Value *elements = (Value*)arr->data;

        if (setjmp(break_jmp) == 0) {
            for (int i = 0; i < arr->size; i++) {
                env_set(loop_env, key_var, (Value){TYPE_INT, i});
                env_set(loop_env, value_var, elements[i]);

                if (setjmp(continue_jmp) == 0) {
                    execute_block(node->data.foreach_stmt.body);
                }
            }
        }
    } else if (collection.type == TYPE_DICT) {
        Dict *dict = (Dict*)collection.data;

        if (setjmp(break_jmp) == 0) {
            for (int i = 0; i < HASH_SIZE; i++) {
                DictEntry *entry = dict->buckets[i];
                while (entry) {
                    Value key_val = {TYPE_STRING, (long)entry->key};
                    env_set(loop_env, key_var, key_val);
                    env_set(loop_env, value_var, entry->value);

                    if (setjmp(continue_jmp) == 0) {
                        execute_block(node->data.foreach_stmt.body);
                    }

                    entry = entry->next;
                }
            }
        }
    } else {
        runtime_error("foreach requires an array or dict");
    }

    loop_env_top--;
    current_env = saved_env;
}

static void eval_break(ASTNode *node) {
    set_error_ctx(node->line, node->file);
    longjmp(break_jmp, 1);
}

static void eval_continue(ASTNode *node) {
    set_error_ctx(node->line, node->file);
    longjmp(continue_jmp, 1);
}

static void eval_return(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    return_value = node->data.return_stmt.value ?
                   eval_expression(node->data.return_stmt.value) :
                   make_null();
    has_returned = 1;
}

static void eval_func_def(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    Function *func = malloc(sizeof(Function));
    func->name = node->data.func_def.name;
    func->params = node->data.func_def.params;
    func->body = node->data.func_def.body;
    func->env = current_env;

    Value func_val = {TYPE_FUNC, (long)func};
    env_define(current_env, func->name, func_val);
}

static void eval_class_def(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    ClassValue *cls = malloc(sizeof(ClassValue));
    cls->name = node->data.class_def.name;
    cls->members = node->data.class_def.members;
    cls->methods = node->data.class_def.methods;
    cls->env = current_env;

    Value class_val = {TYPE_CLASS, (long)cls};
    env_define(current_env, cls->name, class_val);
}

static void eval_try_catch(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    // ===========================================================================
    // CRITICAL: Dual Exception System Integration
    // ===========================================================================
    // This interpreter must handle exceptions from TWO independent systems:
    //
    // 1. INTERPRETER exceptions (eval_raise):
    //    - Uses exception_stack[] + longjmp()
    //    - Triggered by: raise statement in user code
    //
    // 2. RUNTIME exceptions (__raise in runtime.c):
    //    - Uses try_stack[] + longjmp()
    //    - Triggered by: json_decode, assert, type errors in built-in functions
    //    - Originally designed for LLVM codegen, not interpreter
    //
    // PROBLEM: If we only register with interpreter's exception_stack,
    //          runtime exceptions (like json_decode errors) will call __raise(),
    //          which checks try_top==0, sees no handler, and exit(1) immediately!
    //          The interpreter's try-catch will NEVER see these exceptions.
    //
    // SOLUTION: Register with BOTH exception systems by:
    //           - Pushing a handler onto runtime's try_stack (__try_push_buf)
    //           - Setting up nested setjmp for both stacks
    //           This creates a "bridge" between the two exception worlds.
    // ===========================================================================

    void *runtime_buf = __try_push_buf();  // Register handler in runtime's try_stack
    int caught_exception = 0;  // 0 = no exception, 1 = interpreter, 2 = runtime
    Environment *saved_env = current_env;  // Save env (longjmp doesn't restore locals)

    // Nested setjmp: outer catches interpreter exceptions, inner catches runtime exceptions
    if (setjmp(exception_stack[exception_top++]) == 0) {
        // Set up runtime exception handler (catches json_decode, assert, etc.)
        if (setjmp(*(jmp_buf*)runtime_buf) == 0) {
            // Try block - NO new scope (per language spec)
            execute_block(node->data.try_catch.try_block);
            // Normal completion - no exception
        } else {
            // Caught runtime exception (from json_decode, assert, built-in type errors)
            caught_exception = 2;
            exception_value = __get_exception();  // Get exception from runtime system
        }
    } else {
        // Caught interpreter exception (from raise statement)
        caught_exception = 1;
        // exception_value already set by eval_raise()
    }

    // Clean up exception handlers from both systems
    exception_top--;   // Pop from interpreter stack
    __try_pop();       // Pop from runtime stack

    // Restore environment (longjmp may have left it in inconsistent state)
    current_env = saved_env;

    // If exception was caught, execute catch block
    if (caught_exception) {
        // Save exception value in a local variable to protect it during GC
        // NOTE: GC may be triggered during gc_alloc() below. The exception_value
        // global is only marked when exception_top > 0, but we just decremented it.
        // So we need to protect it by storing in a stack variable (conservative
        // stack scanning will find it) and also explicitly marking it.
        Value exc_val = exception_value;

        // Register exception value as a temporary GC root
        gc_push_root(&exc_val);

        // Format exception message to match LLVM backend output
        // LLVM format: [caught in file:line] original_message
        if (exc_val.type == TYPE_STRING) {
            char buf[1024];
            snprintf(buf, sizeof(buf), "[caught in %s:%d] %s",
                     node->file, node->line, (char*)exc_val.data);
            char *full = gc_alloc(TYPE_STRING, strlen(buf) + 1);  // May trigger GC!
            strcpy(full, buf);
            exc_val = (Value){TYPE_STRING, (long)full};
        }

        // Bind exception to catch variable
        // Allow reusing same catch var name across multiple try-catch blocks
        if (node->data.try_catch.catch_var) {
            if (env_exists(current_env, node->data.try_catch.catch_var)) {
                env_set(current_env, node->data.try_catch.catch_var, exc_val);
            } else {
                env_define(current_env, node->data.try_catch.catch_var, exc_val);  // May trigger GC!
            }
        }

        // Unregister temporary root before executing catch block
        gc_pop_root();

        execute_block(node->data.try_catch.catch_block);
    }
}

static void eval_raise(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    Value msg = eval_expression(node->data.raise_stmt.expr);

    // Format exception message to match LLVM backend format
    // Format: file:line: message
    // (The "[caught in ...]" prefix will be added by eval_try_catch when caught)
    char buf[1024];
    char *msg_str;
    if (msg.type == TYPE_STRING) {
        msg_str = (char*)msg.data;
    } else {
        Value s = to_string(msg);
        msg_str = (char*)s.data;
    }

    snprintf(buf, sizeof(buf), "%s:%d: %s", node->file, node->line, msg_str);
    char *full = gc_alloc(TYPE_STRING, strlen(buf) + 1);
    strcpy(full, buf);
    exception_value = (Value){TYPE_STRING, (long)full};

    // Throw exception on interpreter's exception_stack
    if (exception_top > 0) {
        longjmp(exception_stack[exception_top - 1], 1);
    } else {
        // No try-catch handler - unhandled exception
        printf("Uncaught exception: %s\n", full);
        exit(1);
    }
}

static void eval_assert(ASTNode *node) {
    set_error_ctx(node->line, node->file);

    Value cond = eval_expression(node->data.assert_stmt.expr);
    if (!is_truthy(cond)) {
        Value msg;
        if (node->data.assert_stmt.msg) {
            msg = eval_expression(node->data.assert_stmt.msg);
        } else {
            char buf[256];
            snprintf(buf, sizeof(buf), "Assertion failed");
            char *msg_str = gc_alloc(TYPE_STRING, strlen(buf) + 1);
            strcpy(msg_str, buf);
            msg = (Value){TYPE_STRING, (long)msg_str};
        }

        // Raise exception instead of exiting
        exception_value = msg;
        if (exception_top > 0) {
            longjmp(exception_stack[exception_top - 1], 1);
        } else {
            // Unhandled exception
            printf("Assertion failed: ");
            print_value(msg);
            printf("\n");
            exit(1);
        }
    }
}

static void eval_statement(ASTNode *node) {
    if (!node) return;

    // Check for return
    if (has_returned) return;

    switch (node->type) {
        case NODE_VAR_DECL:
            eval_var_decl(node);
            break;

        case NODE_ASSIGNMENT:
            eval_assignment(node);
            break;

        case NODE_IF_STMT:
            eval_if_stmt(node);
            break;

        case NODE_WHILE_STMT:
            eval_while_stmt(node);
            break;

        case NODE_FOR_STMT:
            eval_for_stmt(node);
            break;

        case NODE_FOREACH_STMT:
            eval_foreach_stmt(node);
            break;

        case NODE_BREAK:
            eval_break(node);
            break;

        case NODE_CONTINUE:
            eval_continue(node);
            break;

        case NODE_RETURN:
            eval_return(node);
            break;

        case NODE_FUNC_DEF:
            eval_func_def(node);
            break;

        case NODE_CLASS_DEF:
            eval_class_def(node);
            break;

        case NODE_TRY_CATCH:
            eval_try_catch(node);
            break;

        case NODE_RAISE:
            eval_raise(node);
            break;

        case NODE_ASSERT:
            eval_assert(node);
            break;

        // Expression statements (allow any expression as a statement)
        case NODE_FUNC_CALL:
        case NODE_METHOD_CALL:
        case NODE_BINARY_OP:
        case NODE_UNARY_OP:
        case NODE_ARRAY_LITERAL:
        case NODE_DICT_LITERAL:
        case NODE_INDEX_ACCESS:
        case NODE_SLICE_ACCESS:
        case NODE_MEMBER_ACCESS:
        case NODE_NEW_EXPR:
        case NODE_IDENTIFIER:
        case NODE_INT_LITERAL:
        case NODE_FLOAT_LITERAL:
        case NODE_STRING_LITERAL:
        case NODE_BOOL_LITERAL:
        case NODE_NULL_LITERAL:
            // Any expression can be a statement (result is discarded)
            eval_expression(node);
            break;

        default:
            runtime_error("Unknown statement type: %d", node->type);
    }
}

static void execute_block(ASTNodeList *stmts) {
    while (stmts && !has_returned) {
        eval_statement(stmts->node);
        stmts = stmts->next;
    }
}

// ============================================================================
// Main interpreter entry point
// ============================================================================

void interpret(ASTNode *root) {
    if (!root || root->type != NODE_PROGRAM) {
        fprintf(stderr, "Error: Invalid program node\n");
        return;
    }

    // Create global environment
    global_env = create_environment(NULL);
    current_env = global_env;

    // Execute program statements
    ASTNodeList *stmt = root->data.program.statements;
    while (stmt) {
        eval_statement(stmt->node);
        stmt = stmt->next;
    }
}

// Initialize interpreter for interactive mode (call once at startup)
void interpret_init(void) {
    global_env = create_environment(NULL);
    current_env = global_env;
}

// Execute statements in interactive mode (preserves global environment)
void interpret_interactive(ASTNode *root) {
    if (!root) {
        return;
    }

    // Initialize environment if not already done
    if (!global_env) {
        interpret_init();
    }

    // Execute based on node type
    if (root->type == NODE_PROGRAM) {
        // Execute all statements in the program
        ASTNodeList *stmt = root->data.program.statements;
        while (stmt) {
            eval_statement(stmt->node);
            stmt = stmt->next;
        }
    } else {
        // Execute single statement
        eval_statement(root);
    }
}
