#ifndef AST_H
#define AST_H

typedef enum {
    NODE_PROGRAM,
    NODE_INT_LITERAL,
    NODE_FLOAT_LITERAL,
    NODE_STRING_LITERAL,
    NODE_BOOL_LITERAL,
    NODE_IDENTIFIER,
    NODE_BINARY_OP,
    NODE_UNARY_OP,
    NODE_VAR_DECL,
    NODE_ASSIGNMENT,
    NODE_FUNC_DEF,
    NODE_FUNC_CALL,
    NODE_RETURN,
    NODE_IF_STMT,
    NODE_WHILE_STMT,
    NODE_FOREACH_STMT,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_ARRAY_LITERAL,
    NODE_DICT_LITERAL,
    NODE_DICT_PAIR,
    NODE_INDEX_ACCESS,
    NODE_SLICE_ACCESS
} NodeType;

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_AND, OP_OR, OP_NOT, OP_NEG, OP_IN
} Operator;

typedef struct ASTNode ASTNode;
typedef struct ASTNodeList ASTNodeList;

struct ASTNodeList {
    ASTNode *node;
    ASTNodeList *next;
};

struct ASTNode {
    NodeType type;
    union {
        struct {
            ASTNodeList *statements;
        } program;

        struct {
            int value;
        } int_literal;

        struct {
            double value;
        } float_literal;

        struct {
            char *value;
        } string_literal;

        struct {
            int value;
        } bool_literal;

        struct {
            char *name;
        } identifier;

        struct {
            ASTNode *left;
            Operator op;
            ASTNode *right;
        } binary_op;

        struct {
            Operator op;
            ASTNode *operand;
        } unary_op;

        struct {
            char *name;
            ASTNode *value;
        } var_decl;

        struct {
            ASTNode *target;
            ASTNode *value;
        } assignment;

        struct {
            char *name;
            ASTNodeList *params;
            ASTNodeList *body;
        } func_def;

        struct {
            char *name;
            ASTNodeList *arguments;
        } func_call;

        struct {
            ASTNode *value;
        } return_stmt;

        struct {
            ASTNode *condition;
            ASTNodeList *then_block;
            ASTNodeList *else_block;
        } if_stmt;

        struct {
            ASTNode *condition;
            ASTNodeList *body;
        } while_stmt;

        struct {
            char *key_var;
            char *value_var;
            ASTNode *collection;
            ASTNodeList *body;
        } foreach_stmt;

        struct {
            ASTNodeList *elements;
        } array_literal;

        struct {
            ASTNodeList *pairs;
        } dict_literal;

        struct {
            ASTNode *key;
            ASTNode *value;
        } dict_pair;

        struct {
            ASTNode *object;
            ASTNode *index;
        } index_access;

        struct {
            ASTNode *object;
            ASTNode *start;
            ASTNode *end;
        } slice_access;
    } data;
};

/* AST construction functions */
ASTNode *create_program(ASTNodeList *statements);
ASTNode *create_int_literal(int value);
ASTNode *create_float_literal(double value);
ASTNode *create_string_literal(char *value);
ASTNode *create_bool_literal(int value);
ASTNode *create_identifier(char *name);
ASTNode *create_binary_op(ASTNode *left, Operator op, ASTNode *right);
ASTNode *create_unary_op(Operator op, ASTNode *operand);
ASTNode *create_var_decl(char *name, ASTNode *value);
ASTNode *create_assignment(ASTNode *target, ASTNode *value);
ASTNode *clone_ast_node(ASTNode *node);  // Clone AST node for compound assignment
ASTNode *create_func_def(char *name, ASTNodeList *params, ASTNodeList *body);
ASTNode *create_func_call(char *name, ASTNodeList *arguments);
ASTNode *create_return(ASTNode *value);
ASTNode *create_if_stmt(ASTNode *condition, ASTNodeList *then_block, ASTNodeList *else_block);
ASTNode *create_while_stmt(ASTNode *condition, ASTNodeList *body);
ASTNode *create_foreach_stmt(char *key_var, char *value_var, ASTNode *collection, ASTNodeList *body);
ASTNode *create_break();
ASTNode *create_continue();
ASTNode *create_array_literal(ASTNodeList *elements);
ASTNode *create_dict_literal(ASTNodeList *pairs);
ASTNode *create_dict_pair(ASTNode *key, ASTNode *value);
ASTNode *create_index_access(ASTNode *object, ASTNode *index);
ASTNode *create_slice_access(ASTNode *object, ASTNode *start, ASTNode *end);

/* List functions */
ASTNodeList *create_node_list(ASTNode *node);
ASTNodeList *append_node_list(ASTNodeList *list, ASTNode *node);

/* Interpreter */
void interpret(ASTNode *root);

#endif /* AST_H */
