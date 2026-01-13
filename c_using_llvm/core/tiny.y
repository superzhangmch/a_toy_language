%{
/*
 * Parser for tiny language
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "preprocess.h"

extern int yylex();
extern int yylineno;
extern FILE *yyin;

void yyerror(const char *s);

ASTNode *root = NULL;
PreprocessResult g_pp_result;
%}

%token <ival> INTEGER
%token <fval> FLOAT
%token <sval> STRING IDENTIFIER
%token <bval> TRUE FALSE
%token NULL_LITERAL

%token VAR FUNC RETURN IF ELSE WHILE FOR FOREACH IN NOT_IN BREAK CONTINUE CLASS NEW
%token TRY CATCH RAISE ASSERT
%token AND OR NOT
%token PLUS MINUS MULTIPLY DIVIDE MODULO
%token EQ NE LT LE GT GE
%token ASSIGN PLUS_ASSIGN MINUS_ASSIGN MULTIPLY_ASSIGN DIVIDE_ASSIGN ARROW DOT DOTDOT
%token LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET
%token COMMA COLON SEMICOLON

%type <node> program statement expression primary_expr postfix_expr
%type <node> unary_expr multiplicative_expr additive_expr comparison_expr
%type <node> equality_expr logical_and_expr logical_or_expr
%type <node> var_decl func_def return_stmt if_stmt while_stmt foreach_stmt for_stmt class_def
%type <node> assignment array_literal dict_literal
%type <list> statement_list expr_list param_list array_expr_list dict_pair_list_opt
%type <class_parts> class_member_list

%left OR
%left AND
%left EQ NE
%left LT LE GT GE IN NOT_IN
%left PLUS MINUS
%left MULTIPLY DIVIDE MODULO
%right NOT
%left LBRACKET

%union {
    int ival;
    double fval;
    char *sval;
    int bval;
    ASTNode *node;
    ASTNodeList *list;
    struct ClassParts { ASTNodeList *members; ASTNodeList *methods; } class_parts;
}

%%

program:
    statement_list {
        root = create_program($1);
    }
    ;

statement_list:
    /* empty */ {
        $$ = NULL;
    }
    | statement_list statement {
        $$ = append_node_list($1, $2);
    }
    ;

statement:
    var_decl opt_semicolon
    | func_def
    | return_stmt opt_semicolon
    | if_stmt
    | while_stmt
    | foreach_stmt
    | for_stmt
    | class_def
    | TRY LBRACE statement_list RBRACE CATCH IDENTIFIER LBRACE statement_list RBRACE {
        $$ = create_try_catch($3, $6, $8);
    }
    | RAISE expression opt_semicolon {
        $$ = create_raise($2);
    }
    | ASSERT LPAREN expression RPAREN opt_semicolon {
        $$ = create_assert($3, NULL);
    }
    | ASSERT LPAREN expression COMMA expression RPAREN opt_semicolon {
        $$ = create_assert($3, $5);
    }
    | BREAK opt_semicolon {
        $$ = create_break();
    }
    | CONTINUE opt_semicolon {
        $$ = create_continue();
    }
    | assignment opt_semicolon
    | expression opt_semicolon
    ;

opt_semicolon:
    /* empty */
    | SEMICOLON
    ;

var_decl:
    VAR IDENTIFIER ASSIGN expression {
        $$ = create_var_decl($2, $4);
    }
    ;

func_def:
    FUNC IDENTIFIER LPAREN param_list RPAREN LBRACE statement_list RBRACE {
        $$ = create_func_def($2, $4, $7);
    }
    ;

class_def:
    CLASS IDENTIFIER LBRACE class_member_list RBRACE {
        $$ = create_class_def($2, $4.members, $4.methods);
    }
    ;

class_member_list:
    /* empty */ {
        $$.members = NULL;
        $$.methods = NULL;
    }
    | class_member_list VAR IDENTIFIER ASSIGN expression opt_semicolon {
        ASTNode *member = create_var_decl($3, $5);
        $$.members = append_node_list($1.members, member);
        $$.methods = $1.methods;
    }
    | class_member_list func_def {
        $$.members = $1.members;
        $$.methods = append_node_list($1.methods, $2);
    }
    ;

param_list:
    /* empty */ {
        $$ = NULL;
    }
    | IDENTIFIER {
        $$ = create_node_list(create_identifier($1));
    }
    | param_list COMMA IDENTIFIER {
        $$ = append_node_list($1, create_identifier($3));
    }
    ;

return_stmt:
    RETURN {
        $$ = create_return(NULL);
    }
    | RETURN expression {
        $$ = create_return($2);
    }
    ;

if_stmt:
    IF LPAREN expression RPAREN LBRACE statement_list RBRACE {
        $$ = create_if_stmt($3, $6, NULL);
    }
    | IF LPAREN expression RPAREN LBRACE statement_list RBRACE ELSE LBRACE statement_list RBRACE {
        $$ = create_if_stmt($3, $6, $10);
    }
    | IF LPAREN expression RPAREN LBRACE statement_list RBRACE ELSE if_stmt {
        ASTNodeList *else_list = create_node_list($9);
        $$ = create_if_stmt($3, $6, else_list);
    }
    ;

while_stmt:
    WHILE LPAREN expression RPAREN LBRACE statement_list RBRACE {
        $$ = create_while_stmt($3, $6);
    }
    ;

foreach_stmt:
        FOREACH LPAREN IDENTIFIER ARROW IDENTIFIER IN expression RPAREN LBRACE statement_list RBRACE {
        $$ = create_foreach_stmt($3, $5, $7, $10);
    }
    ;

for_stmt:
    FOR LPAREN IDENTIFIER ASSIGN expression range_op expression RPAREN LBRACE statement_list RBRACE {
        $$ = create_for_stmt($3, $5, $7, $10);
    }
    ;

range_op:
    DOTDOT
    | DOT DOT
    ;

assignment:
    postfix_expr ASSIGN expression {
        $$ = create_assignment($1, $3);
    }
    | postfix_expr PLUS_ASSIGN expression {
        ASTNode *target_copy = clone_ast_node($1);
        ASTNode *binary = create_binary_op(target_copy, OP_ADD, $3);
        $$ = create_assignment($1, binary);
    }
    | postfix_expr MINUS_ASSIGN expression {
        ASTNode *target_copy = clone_ast_node($1);
        ASTNode *binary = create_binary_op(target_copy, OP_SUB, $3);
        $$ = create_assignment($1, binary);
    }
    | postfix_expr MULTIPLY_ASSIGN expression {
        ASTNode *target_copy = clone_ast_node($1);
        ASTNode *binary = create_binary_op(target_copy, OP_MUL, $3);
        $$ = create_assignment($1, binary);
    }
    | postfix_expr DIVIDE_ASSIGN expression {
        ASTNode *target_copy = clone_ast_node($1);
        ASTNode *binary = create_binary_op(target_copy, OP_DIV, $3);
        $$ = create_assignment($1, binary);
    }
    ;

expression:
    logical_or_expr
    ;

logical_or_expr:
    logical_and_expr
    | logical_or_expr OR logical_and_expr {
        $$ = create_binary_op($1, OP_OR, $3);
    }
    ;

logical_and_expr:
    equality_expr
    | logical_and_expr AND equality_expr {
        $$ = create_binary_op($1, OP_AND, $3);
    }
    ;

equality_expr:
    comparison_expr
    | equality_expr EQ comparison_expr {
        $$ = create_binary_op($1, OP_EQ, $3);
    }
    | equality_expr NE comparison_expr {
        $$ = create_binary_op($1, OP_NE, $3);
    }
    ;

comparison_expr:
    additive_expr
    | comparison_expr LT additive_expr {
        $$ = create_binary_op($1, OP_LT, $3);
    }
    | comparison_expr LE additive_expr {
        $$ = create_binary_op($1, OP_LE, $3);
    }
    | comparison_expr GT additive_expr {
        $$ = create_binary_op($1, OP_GT, $3);
    }
    | comparison_expr GE additive_expr {
        $$ = create_binary_op($1, OP_GE, $3);
    }
    | additive_expr IN additive_expr {
        $$ = create_binary_op($1, OP_IN, $3);
    }
    | additive_expr NOT_IN additive_expr {
        $$ = create_binary_op($1, OP_NOT_IN, $3);
    }
    ;

additive_expr:
    multiplicative_expr
    | additive_expr PLUS multiplicative_expr {
        $$ = create_binary_op($1, OP_ADD, $3);
    }
    | additive_expr MINUS multiplicative_expr {
        $$ = create_binary_op($1, OP_SUB, $3);
    }
    ;

multiplicative_expr:
    unary_expr
    | multiplicative_expr MULTIPLY unary_expr {
        $$ = create_binary_op($1, OP_MUL, $3);
    }
    | multiplicative_expr DIVIDE unary_expr {
        $$ = create_binary_op($1, OP_DIV, $3);
    }
    | multiplicative_expr MODULO unary_expr {
        $$ = create_binary_op($1, OP_MOD, $3);
    }
    ;

unary_expr:
    postfix_expr
    | MINUS unary_expr {
        $$ = create_unary_op(OP_NEG, $2);
    }
    | NOT unary_expr {
        $$ = create_unary_op(OP_NOT, $2);
    }
    ;

postfix_expr:
    primary_expr
    | postfix_expr LBRACKET expression RBRACKET {
        $$ = create_index_access($1, $3);
    }
    | postfix_expr LBRACKET expression COLON expression RBRACKET {
        $$ = create_slice_access($1, $3, $5);
    }
    | postfix_expr DOT IDENTIFIER {
        $$ = create_member_access($1, $3);
    }
    | postfix_expr LPAREN expr_list RPAREN {
        if ($1->type == NODE_IDENTIFIER) {
            $$ = create_func_call($1->data.identifier.name, $3);
        } else if ($1->type == NODE_MEMBER_ACCESS) {
            $$ = create_method_call($1->data.member_access.object, $1->data.member_access.member, $3);
        } else {
            yyerror("Only identifiers or object members can be called as functions");
        }
    }
    ;

primary_expr:
    INTEGER {
        $$ = create_int_literal($1);
    }
    | FLOAT {
        $$ = create_float_literal($1);
    }
    | STRING {
        $$ = create_string_literal($1);
    }
    | TRUE {
        $$ = create_bool_literal(1);
    }
    | FALSE {
        $$ = create_bool_literal(0);
    }
    | NULL_LITERAL {
        $$ = create_null_literal();
    }
    | IDENTIFIER {
        $$ = create_identifier($1);
    }
    | NEW IDENTIFIER LPAREN expr_list RPAREN {
        $$ = create_new_expression($2, $4);
    }
    | array_literal
    | dict_literal
    | LPAREN expression RPAREN {
        $$ = $2;
    }
    ;

array_literal:
    LBRACKET array_expr_list RBRACKET {
        $$ = create_array_literal($2);
    }
    ;

dict_literal:
    LBRACE dict_pair_list_opt RBRACE {
        $$ = create_dict_literal($2);
    }
    ;

expr_list:
    /* empty */ {
        $$ = NULL;
    }
    | expression {
        $$ = create_node_list($1);
    }
    | expr_list COMMA expression {
        $$ = append_node_list($1, $3);
    }
    ;

array_expr_list:
    /* empty */ { $$ = NULL; }
    | expression { $$ = create_node_list($1); }
    | array_expr_list COMMA expression { $$ = append_node_list($1, $3); }
    | array_expr_list COMMA { $$ = $1; }
    ;

dict_pair_list_opt:
    /* empty */ { $$ = NULL; }
    | STRING COLON expression {
        ASTNode *pair = create_dict_pair(create_string_literal($1), $3);
        $$ = create_node_list(pair);
    }
    | dict_pair_list_opt COMMA STRING COLON expression {
        ASTNode *pair = create_dict_pair(create_string_literal($3), $5);
        $$ = append_node_list($1, pair);
    }
    | dict_pair_list_opt COMMA { $$ = $1; }
    ;

%%

void yyerror(const char *s) {
    const char *file = "<unknown>";
    int line = yylineno;
    map_line(&g_pp_result, yylineno, &file, &line);
    fprintf(stderr, "Parse error at %s:%d: %s\n", file, line, s);
}
