#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ast.h"
#include "runtime.h"  // Use runtime.h's Value, Array, Dict, etc.
#include "gc.h"       // Use GC for memory management

// ============================================================================
// Interpreter-specific structures (not in runtime.h)
// ============================================================================

// InterpreterFunction represents a user-defined function in the interpreter
// This stores the AST for the function body, which is needed for interpretation
typedef struct InterpreterFunction {
    char *name;
    ASTNodeList *params;
    ASTNodeList *body;
    struct Environment *env;  // Closure environment
} InterpreterFunction;

// ClassValue represents a class definition
typedef struct ClassValue {
    char *name;
    ASTNodeList *members;   // Variable declarations
    ASTNodeList *methods;   // InterpreterFunction definitions
    struct Environment *env;
} ClassValue;

// Environment for variable scoping
typedef struct EnvEntry {
    char *name;
    Value value;  // Now stores Value directly (not Value*)
    struct EnvEntry *next;
} EnvEntry;

typedef struct Environment {
    EnvEntry **buckets;
    int size;
    struct Environment *parent;
} Environment;

// ============================================================================
// Environment functions
// ============================================================================

Environment *create_environment(struct Environment *parent);
void env_define(Environment *env, char *name, Value val);
Value env_get(Environment *env, char *name);
void env_set(Environment *env, char *name, Value val);
int env_exists(Environment *env, char *name);

// ============================================================================
// Interpreter functions
// ============================================================================

void interpret(ASTNode *root);
void interpret_init(void);
void interpret_interactive(ASTNode *root);
void* get_interactive_error_jmpbuf(void);  // Get setjmp buffer for interactive mode error handling

#endif /* INTERPRETER_H */
