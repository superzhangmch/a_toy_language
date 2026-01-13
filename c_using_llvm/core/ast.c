#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "tiny.tab.h"
#include "preprocess.h"

extern int yylineno;
extern PreprocessResult g_pp_result;

ASTNode *create_node(NodeType type) {
    ASTNode *node = (ASTNode *)malloc(sizeof(ASTNode));
    node->type = type;
    node->line = yylineno;
    const char *fname = "<input>";
    int mapped_line = yylineno;
    if (g_pp_result.mappings != NULL) {
        map_line(&g_pp_result, yylineno, &fname, &mapped_line);
    }
    node->line = mapped_line;
    node->file = strdup(fname);
    return node;
}

ASTNode *create_program(ASTNodeList *statements) {
    ASTNode *node = create_node(NODE_PROGRAM);
    node->data.program.statements = statements;
    return node;
}

ASTNode *create_int_literal(int value) {
    ASTNode *node = create_node(NODE_INT_LITERAL);
    node->data.int_literal.value = value;
    return node;
}

ASTNode *create_float_literal(double value) {
    ASTNode *node = create_node(NODE_FLOAT_LITERAL);
    node->data.float_literal.value = value;
    return node;
}

ASTNode *create_string_literal(char *value) {
    ASTNode *node = create_node(NODE_STRING_LITERAL);
    node->data.string_literal.value = strdup(value);
    return node;
}

ASTNode *create_bool_literal(int value) {
    ASTNode *node = create_node(NODE_BOOL_LITERAL);
    node->data.bool_literal.value = value;
    return node;
}

ASTNode *create_null_literal() {
    ASTNode *node = create_node(NODE_NULL_LITERAL);
    return node;
}

ASTNode *create_try_catch(ASTNodeList *try_block, char *catch_var, ASTNodeList *catch_block) {
    ASTNode *node = create_node(NODE_TRY_CATCH);
    node->data.try_catch.try_block = try_block;
    node->data.try_catch.catch_var = strdup(catch_var);
    node->data.try_catch.catch_block = catch_block;
    return node;
}

ASTNode *create_raise(ASTNode *expr) {
    ASTNode *node = create_node(NODE_RAISE);
    node->data.raise_stmt.expr = expr;
    return node;
}

ASTNode *create_assert(ASTNode *expr, ASTNode *msg) {
    ASTNode *node = create_node(NODE_ASSERT);
    node->data.assert_stmt.expr = expr;
    node->data.assert_stmt.msg = msg;
    return node;
}

ASTNode *create_identifier(char *name) {
    ASTNode *node = create_node(NODE_IDENTIFIER);
    node->data.identifier.name = strdup(name);
    return node;
}

ASTNode *create_binary_op(ASTNode *left, Operator op, ASTNode *right) {
    ASTNode *node = create_node(NODE_BINARY_OP);
    node->data.binary_op.left = left;
    node->data.binary_op.op = op;
    node->data.binary_op.right = right;
    return node;
}

ASTNode *create_unary_op(Operator op, ASTNode *operand) {
    ASTNode *node = create_node(NODE_UNARY_OP);
    node->data.unary_op.op = op;
    node->data.unary_op.operand = operand;
    return node;
}

ASTNode *create_var_decl(char *name, ASTNode *value) {
    ASTNode *node = create_node(NODE_VAR_DECL);
    node->data.var_decl.name = strdup(name);
    node->data.var_decl.value = value;
    return node;
}

ASTNode *create_assignment(ASTNode *target, ASTNode *value) {
    ASTNode *node = create_node(NODE_ASSIGNMENT);
    node->data.assignment.target = target;
    node->data.assignment.value = value;
    return node;
}

// Clone AST node (for compound assignment operations)
// Only clones common node types used in lvalues
ASTNode *clone_ast_node(ASTNode *node) {
    if (node == NULL) return NULL;

    switch (node->type) {
        case NODE_IDENTIFIER:
            return create_identifier(node->data.identifier.name);

        case NODE_INDEX_ACCESS:
            return create_index_access(
                clone_ast_node(node->data.index_access.object),
                clone_ast_node(node->data.index_access.index)
            );

        case NODE_SLICE_ACCESS:
            return create_slice_access(
                clone_ast_node(node->data.slice_access.object),
                clone_ast_node(node->data.slice_access.start),
                clone_ast_node(node->data.slice_access.end)
            );

        case NODE_MEMBER_ACCESS:
            return create_member_access(
                clone_ast_node(node->data.member_access.object),
                node->data.member_access.member
            );

        case NODE_INT_LITERAL:
            return create_int_literal(node->data.int_literal.value);

        case NODE_STRING_LITERAL:
            return create_string_literal(node->data.string_literal.value);

        default:
            fprintf(stderr, "Error: Cannot clone AST node type %d\n", node->type);
            exit(1);
    }
}

ASTNode *create_func_def(char *name, ASTNodeList *params, ASTNodeList *body) {
    ASTNode *node = create_node(NODE_FUNC_DEF);
    node->data.func_def.name = strdup(name);
    node->data.func_def.params = params;
    node->data.func_def.body = body;
    return node;
}

ASTNode *create_func_call(char *name, ASTNodeList *arguments) {
    ASTNode *node = create_node(NODE_FUNC_CALL);
    node->data.func_call.name = strdup(name);
    node->data.func_call.arguments = arguments;
    return node;
}

ASTNode *create_return(ASTNode *value) {
    ASTNode *node = create_node(NODE_RETURN);
    node->data.return_stmt.value = value;
    return node;
}

ASTNode *create_if_stmt(ASTNode *condition, ASTNodeList *then_block, ASTNodeList *else_block) {
    ASTNode *node = create_node(NODE_IF_STMT);
    node->data.if_stmt.condition = condition;
    node->data.if_stmt.then_block = then_block;
    node->data.if_stmt.else_block = else_block;
    return node;
}

ASTNode *create_while_stmt(ASTNode *condition, ASTNodeList *body) {
    ASTNode *node = create_node(NODE_WHILE_STMT);
    node->data.while_stmt.condition = condition;
    node->data.while_stmt.body = body;
    return node;
}

ASTNode *create_for_stmt(char *index_var, ASTNode *start, ASTNode *end, ASTNodeList *body) {
    ASTNode *node = create_node(NODE_FOR_STMT);
    node->data.for_stmt.index_var = strdup(index_var);
    node->data.for_stmt.start = start;
    node->data.for_stmt.end = end;
    node->data.for_stmt.body = body;
    return node;
}

ASTNode *create_foreach_stmt(char *key_var, char *value_var, ASTNode *collection, ASTNodeList *body) {
    ASTNode *node = create_node(NODE_FOREACH_STMT);
    node->data.foreach_stmt.key_var = strdup(key_var);
    node->data.foreach_stmt.value_var = strdup(value_var);
    node->data.foreach_stmt.collection = collection;
    node->data.foreach_stmt.body = body;
    return node;
}

ASTNode *create_break() {
    return create_node(NODE_BREAK);
}

ASTNode *create_continue() {
    return create_node(NODE_CONTINUE);
}

ASTNode *create_array_literal(ASTNodeList *elements) {
    ASTNode *node = create_node(NODE_ARRAY_LITERAL);
    node->data.array_literal.elements = elements;
    return node;
}

ASTNode *create_dict_literal(ASTNodeList *pairs) {
    ASTNode *node = create_node(NODE_DICT_LITERAL);
    node->data.dict_literal.pairs = pairs;
    return node;
}

ASTNode *create_dict_pair(ASTNode *key, ASTNode *value) {
    ASTNode *node = create_node(NODE_DICT_PAIR);
    node->data.dict_pair.key = key;
    node->data.dict_pair.value = value;
    return node;
}

ASTNode *create_index_access(ASTNode *object, ASTNode *index) {
    ASTNode *node = create_node(NODE_INDEX_ACCESS);
    node->data.index_access.object = object;
    node->data.index_access.index = index;
    return node;
}

ASTNode *create_slice_access(ASTNode *object, ASTNode *start, ASTNode *end) {
    ASTNode *node = create_node(NODE_SLICE_ACCESS);
    node->data.slice_access.object = object;
    node->data.slice_access.start = start;
    node->data.slice_access.end = end;
    return node;
}

ASTNode *create_class_def(char *name, ASTNodeList *members, ASTNodeList *methods) {
    ASTNode *node = create_node(NODE_CLASS_DEF);
    node->data.class_def.name = strdup(name);
    node->data.class_def.members = members;
    node->data.class_def.methods = methods;
    return node;
}

ASTNode *create_member_access(ASTNode *object, char *member) {
    ASTNode *node = create_node(NODE_MEMBER_ACCESS);
    node->data.member_access.object = object;
    node->data.member_access.member = strdup(member);
    return node;
}

ASTNode *create_method_call(ASTNode *object, char *method, ASTNodeList *arguments) {
    ASTNode *node = create_node(NODE_METHOD_CALL);
    node->data.method_call.object = object;
    node->data.method_call.method = strdup(method);
    node->data.method_call.arguments = arguments;
    return node;
}

ASTNode *create_new_expression(char *class_name, ASTNodeList *arguments) {
    ASTNode *node = create_node(NODE_NEW_EXPR);
    node->data.new_expr.class_name = strdup(class_name);
    node->data.new_expr.arguments = arguments;
    return node;
}

ASTNodeList *create_node_list(ASTNode *node) {
    ASTNodeList *list = (ASTNodeList *)malloc(sizeof(ASTNodeList));
    list->node = node;
    list->next = NULL;
    return list;
}

ASTNodeList *append_node_list(ASTNodeList *list, ASTNode *node) {
    if (list == NULL) {
        return create_node_list(node);
    }

    ASTNodeList *current = list;
    while (current->next != NULL) {
        current = current->next;
    }

    current->next = create_node_list(node);
    return list;
}
