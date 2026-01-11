#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "c_codegen.h"
#include "ast.h"

static void emit(CCodeGen *gen, const char *fmt, ...) {
    for (int i = 0; i < gen->indent_level; i++) {
        fprintf(gen->out, "    ");
    }
    va_list args;
    va_start(args, fmt);
    vfprintf(gen->out, fmt, args);
    va_end(args);
}

static void emit_escaped_string(CCodeGen *gen, const char *str) {
    fprintf(gen->out, "\"");
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '\n': fprintf(gen->out, "\\n"); break;
            case '\t': fprintf(gen->out, "\\t"); break;
            case '\r': fprintf(gen->out, "\\r"); break;
            case '\\': fprintf(gen->out, "\\\\"); break;
            case '"':  fprintf(gen->out, "\\\""); break;
            default:   fprintf(gen->out, "%c", *p); break;
        }
    }
    fprintf(gen->out, "\"");
}

static void gen_expr(CCodeGen *gen, ASTNode *node);
static void gen_statement(CCodeGen *gen, ASTNode *node);

void ccodegen_init(CCodeGen *gen, FILE *out) {
    gen->out = out;
    gen->label_counter = 0;
    gen->temp_counter = 0;
    gen->indent_level = 0;
}

static void gen_expr(CCodeGen *gen, ASTNode *node) {
    switch (node->type) {
        case NODE_INT_LITERAL:
            fprintf(gen->out, "make_int(%d)", node->data.int_literal.value);
            break;

        case NODE_FLOAT_LITERAL:
            fprintf(gen->out, "make_float(%g)", node->data.float_literal.value);
            break;

        case NODE_STRING_LITERAL:
            fprintf(gen->out, "make_string(");
            emit_escaped_string(gen, node->data.string_literal.value);
            fprintf(gen->out, ")");
            break;

        case NODE_BOOL_LITERAL:
            fprintf(gen->out, "make_int(%d)", node->data.bool_literal.value);
            break;

        case NODE_IDENTIFIER:
            fprintf(gen->out, "%s", node->data.identifier.name);
            break;

        case NODE_ARRAY_LITERAL: {
            fprintf(gen->out, "make_array()");
            break;
        }

        case NODE_BINARY_OP: {
            // Special handling for IN operator
            if (node->data.binary_op.op == OP_IN) {
                fprintf(gen->out, "in_operator(");
                gen_expr(gen, node->data.binary_op.left);
                fprintf(gen->out, ", ");
                gen_expr(gen, node->data.binary_op.right);
                fprintf(gen->out, ")");
                break;
            }

            fprintf(gen->out, "binary_op(");
            gen_expr(gen, node->data.binary_op.left);
            fprintf(gen->out, ", ");

            switch (node->data.binary_op.op) {
                case OP_ADD: fprintf(gen->out, "OP_ADD"); break;
                case OP_SUB: fprintf(gen->out, "OP_SUB"); break;
                case OP_MUL: fprintf(gen->out, "OP_MUL"); break;
                case OP_DIV: fprintf(gen->out, "OP_DIV"); break;
                case OP_MOD: fprintf(gen->out, "OP_MOD"); break;
                case OP_EQ: fprintf(gen->out, "OP_EQ"); break;
                case OP_NE: fprintf(gen->out, "OP_NE"); break;
                case OP_LT: fprintf(gen->out, "OP_LT"); break;
                case OP_LE: fprintf(gen->out, "OP_LE"); break;
                case OP_GT: fprintf(gen->out, "OP_GT"); break;
                case OP_GE: fprintf(gen->out, "OP_GE"); break;
                case OP_AND: fprintf(gen->out, "OP_AND"); break;
                case OP_OR: fprintf(gen->out, "OP_OR"); break;
                default: fprintf(gen->out, "0"); break;
            }

            fprintf(gen->out, ", ");
            gen_expr(gen, node->data.binary_op.right);
            fprintf(gen->out, ")");
            break;
        }

        case NODE_UNARY_OP:
            fprintf(gen->out, "unary_op(");
            if (node->data.unary_op.op == OP_NEG) {
                fprintf(gen->out, "OP_NEG");
            } else if (node->data.unary_op.op == OP_NOT) {
                fprintf(gen->out, "OP_NOT");
            }
            fprintf(gen->out, ", ");
            gen_expr(gen, node->data.unary_op.operand);
            fprintf(gen->out, ")");
            break;

        case NODE_INDEX_ACCESS:
            fprintf(gen->out, "index_access(");
            gen_expr(gen, node->data.index_access.object);
            fprintf(gen->out, ", ");
            gen_expr(gen, node->data.index_access.index);
            fprintf(gen->out, ")");
            break;

        case NODE_SLICE_ACCESS:
            fprintf(gen->out, "slice_access(");
            gen_expr(gen, node->data.slice_access.object);
            fprintf(gen->out, ", ");
            gen_expr(gen, node->data.slice_access.start);
            fprintf(gen->out, ", ");
            gen_expr(gen, node->data.slice_access.end);
            fprintf(gen->out, ")");
            break;

        case NODE_FUNC_CALL: {
            // Map int() and float() to to_int() and to_float() to avoid C keyword conflicts
            const char *func_name = node->data.func_call.name;
            if (strcmp(func_name, "int") == 0) {
                func_name = "to_int";
            } else if (strcmp(func_name, "float") == 0) {
                func_name = "to_float";
            }
            fprintf(gen->out, "%s(", func_name);
            ASTNodeList *arg = node->data.func_call.arguments;
            while (arg != NULL) {
                gen_expr(gen, arg->node);
                if (arg->next != NULL) {
                    fprintf(gen->out, ", ");
                }
                arg = arg->next;
            }
            fprintf(gen->out, ")");
            break;
        }

        default:
            fprintf(gen->out, "make_int(0)");
            break;
    }
}

static void gen_statement(CCodeGen *gen, ASTNode *node) {
    switch (node->type) {
        case NODE_VAR_DECL:
            emit(gen, "Value %s = ", node->data.var_decl.name);
            gen_expr(gen, node->data.var_decl.value);
            fprintf(gen->out, ";\n");
            break;

        case NODE_ASSIGNMENT:
            emit(gen, "");
            if (node->data.assignment.target->type == NODE_IDENTIFIER) {
                fprintf(gen->out, "%s", node->data.assignment.target->data.identifier.name);
            } else if (node->data.assignment.target->type == NODE_INDEX_ACCESS) {
                fprintf(gen->out, "set_index(");
                gen_expr(gen, node->data.assignment.target->data.index_access.object);
                fprintf(gen->out, ", ");
                gen_expr(gen, node->data.assignment.target->data.index_access.index);
                fprintf(gen->out, ", ");
                gen_expr(gen, node->data.assignment.value);
                fprintf(gen->out, ");\n");
                return;
            }
            fprintf(gen->out, " = ");
            gen_expr(gen, node->data.assignment.value);
            fprintf(gen->out, ";\n");
            break;

        case NODE_FUNC_CALL:
            emit(gen, "");
            gen_expr(gen, node);
            fprintf(gen->out, ";\n");
            break;

        case NODE_IF_STMT:
            emit(gen, "if (is_truthy(");
            gen_expr(gen, node->data.if_stmt.condition);
            fprintf(gen->out, ")) {\n");

            gen->indent_level++;
            ASTNodeList *stmt = node->data.if_stmt.then_block;
            while (stmt != NULL) {
                gen_statement(gen, stmt->node);
                stmt = stmt->next;
            }
            gen->indent_level--;

            if (node->data.if_stmt.else_block) {
                emit(gen, "} else {\n");
                gen->indent_level++;
                stmt = node->data.if_stmt.else_block;
                while (stmt != NULL) {
                    gen_statement(gen, stmt->node);
                    stmt = stmt->next;
                }
                gen->indent_level--;
            }

            emit(gen, "}\n");
            break;

        case NODE_WHILE_STMT:
            emit(gen, "while (is_truthy(");
            gen_expr(gen, node->data.while_stmt.condition);
            fprintf(gen->out, ")) {\n");

            gen->indent_level++;
            stmt = node->data.while_stmt.body;
            while (stmt != NULL) {
                gen_statement(gen, stmt->node);
                stmt = stmt->next;
            }
            gen->indent_level--;

            emit(gen, "}\n");
            break;

        case NODE_BREAK:
            emit(gen, "break;\n");
            break;

        case NODE_CONTINUE:
            emit(gen, "continue;\n");
            break;

        case NODE_RETURN:
            emit(gen, "return ");
            if (node->data.return_stmt.value) {
                gen_expr(gen, node->data.return_stmt.value);
            } else {
                fprintf(gen->out, "make_int(0)");
            }
            fprintf(gen->out, ";\n");
            break;

        case NODE_FUNC_DEF:
            // Function definitions are handled separately, skip here
            break;

        default:
            break;
    }
}

void ccodegen_program(CCodeGen *gen, ASTNode *root) {
    // Emit C header with runtime
    fprintf(gen->out, "#include <stdio.h>\n");
    fprintf(gen->out, "#include <stdlib.h>\n");
    fprintf(gen->out, "#include <string.h>\n\n");

    // Runtime type system
    fprintf(gen->out, "typedef enum { TYPE_INT, TYPE_FLOAT, TYPE_STRING, TYPE_ARRAY } ValueType;\n");
    fprintf(gen->out, "typedef struct Array { void **data; int size; int capacity; } Array;\n");
    fprintf(gen->out, "typedef struct { ValueType type; union { long i; double f; char *s; Array *a; } v; } Value;\n\n");

    // Operators enum
    fprintf(gen->out, "enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD, OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE, OP_AND, OP_OR, OP_NEG, OP_NOT };\n\n");

    // Runtime functions
    fprintf(gen->out,
        "Value make_int(long i) { Value v; v.type = TYPE_INT; v.v.i = i; return v; }\n"
        "Value make_float(double f) { Value v; v.type = TYPE_FLOAT; v.v.f = f; return v; }\n"
        "Value make_string(const char *s) { Value v; v.type = TYPE_STRING; v.v.s = strdup(s); return v; }\n"
        "Value make_array() { Value v; v.type = TYPE_ARRAY; v.v.a = malloc(sizeof(Array));\n"
        "  v.v.a->data = NULL; v.v.a->size = 0; v.v.a->capacity = 0; return v; }\n\n"

        "int is_truthy(Value v) {\n"
        "  if (v.type == TYPE_INT) return v.v.i != 0;\n"
        "  if (v.type == TYPE_FLOAT) return v.v.f != 0.0;\n"
        "  if (v.type == TYPE_STRING) return strlen(v.v.s) > 0;\n"
        "  if (v.type == TYPE_ARRAY) return v.v.a->size > 0;\n"
        "  return 0;\n"
        "}\n\n"

        "Value binary_op(Value l, int op, Value r) {\n"
        "  if (op == OP_ADD) {\n"
        "    if (l.type == TYPE_STRING || r.type == TYPE_STRING) {\n"
        "      char *result = malloc(strlen(l.v.s) + strlen(r.v.s) + 1);\n"
        "      strcpy(result, l.v.s); strcat(result, r.v.s); return make_string(result);\n"
        "    }\n"
        "    if (l.type == TYPE_INT && r.type == TYPE_INT) return make_int(l.v.i + r.v.i);\n"
        "    return make_float(l.v.f + r.v.f);\n"
        "  }\n"
        "  if (op == OP_SUB) return make_int(l.v.i - r.v.i);\n"
        "  if (op == OP_MUL) return make_int(l.v.i * r.v.i);\n"
        "  if (op == OP_DIV) return make_int(l.v.i / r.v.i);\n"
        "  if (op == OP_MOD) return make_int(l.v.i %% r.v.i);\n"
        "  if (op == OP_EQ) return make_int(l.v.i == r.v.i);\n"
        "  if (op == OP_NE) return make_int(l.v.i != r.v.i);\n"
        "  if (op == OP_LT) return make_int(l.v.i < r.v.i);\n"
        "  if (op == OP_LE) return make_int(l.v.i <= r.v.i);\n"
        "  if (op == OP_GT) return make_int(l.v.i > r.v.i);\n"
        "  if (op == OP_GE) return make_int(l.v.i >= r.v.i);\n"
        "  return make_int(0);\n"
        "}\n\n"

        "Value unary_op(int op, Value v) {\n"
        "  if (op == OP_NEG) return make_int(-v.v.i);\n"
        "  if (op == OP_NOT) return make_int(!is_truthy(v));\n"
        "  return v;\n"
        "}\n\n"

        "Value index_access(Value obj, Value idx) {\n"
        "  if (obj.type == TYPE_ARRAY) return ((Value*)obj.v.a->data)[idx.v.i];\n"
        "  if (obj.type == TYPE_STRING) { char s[2] = {obj.v.s[idx.v.i], 0}; return make_string(s); }\n"
        "  return make_int(0);\n"
        "}\n\n"

        "Value set_index(Value obj, Value idx, Value val) {\n"
        "  if (obj.type == TYPE_ARRAY) ((Value*)obj.v.a->data)[idx.v.i] = val;\n"
        "  return val;\n"
        "}\n\n"

        "Value print(Value v) {\n"
        "  if (v.type == TYPE_INT) printf(\"%%ld \", v.v.i);\n"
        "  else if (v.type == TYPE_FLOAT) printf(\"%%g \", v.v.f);\n"
        "  else if (v.type == TYPE_STRING) printf(\"%%s\", v.v.s);\n"
        "  else printf(\"<array>\");\n"
        "  return make_int(0);\n"
        "}\n\n"

        "Value append(Value arr, Value val) {\n"
        "  Array *a = arr.v.a;\n"
        "  if (a->size >= a->capacity) {\n"
        "    a->capacity = a->capacity == 0 ? 8 : a->capacity * 2;\n"
        "    a->data = realloc(a->data, a->capacity * sizeof(Value));\n"
        "  }\n"
        "  ((Value*)a->data)[a->size++] = val;\n"
        "  return make_int(0);\n"
        "}\n\n"

        "Value len(Value v) {\n"
        "  if (v.type == TYPE_STRING) return make_int(strlen(v.v.s));\n"
        "  if (v.type == TYPE_ARRAY) return make_int(v.v.a->size);\n"
        "  return make_int(0);\n"
        "}\n\n"

        "Value str(Value v) {\n"
        "  char buf[64];\n"
        "  if (v.type == TYPE_INT) { sprintf(buf, \"%%ld\", v.v.i); return make_string(buf); }\n"
        "  if (v.type == TYPE_FLOAT) { sprintf(buf, \"%%g\", v.v.f); return make_string(buf); }\n"
        "  if (v.type == TYPE_STRING) return v;\n"
        "  return make_string(\"\");\n"
        "}\n\n"

        "Value to_int(Value v) {\n"
        "  if (v.type == TYPE_INT) return v;\n"
        "  if (v.type == TYPE_FLOAT) return make_int((long)v.v.f);\n"
        "  if (v.type == TYPE_STRING) return make_int(atol(v.v.s));\n"
        "  return make_int(0);\n"
        "}\n\n"

        "Value to_float(Value v) {\n"
        "  if (v.type == TYPE_INT) return make_float((double)v.v.i);\n"
        "  if (v.type == TYPE_FLOAT) return v;\n"
        "  if (v.type == TYPE_STRING) return make_float(atof(v.v.s));\n"
        "  return make_float(0.0);\n"
        "}\n\n"

        "Value type(Value v) {\n"
        "  if (v.type == TYPE_INT) return make_string(\"int\");\n"
        "  if (v.type == TYPE_FLOAT) return make_string(\"float\");\n"
        "  if (v.type == TYPE_STRING) return make_string(\"string\");\n"
        "  if (v.type == TYPE_ARRAY) return make_string(\"array\");\n"
        "  return make_string(\"unknown\");\n"
        "}\n\n"

        "Value slice_access(Value obj, Value start_v, Value end_v) {\n"
        "  int start = (int)start_v.v.i;\n"
        "  int end = (int)end_v.v.i;\n"
        "  if (obj.type == TYPE_ARRAY) {\n"
        "    Array *arr = obj.v.a;\n"
        "    if (start < 0) start = 0;\n"
        "    if (end > arr->size) end = arr->size;\n"
        "    if (start > end) start = end;\n"
        "    Value result = make_array();\n"
        "    for (int i = start; i < end; i++) {\n"
        "      append(result, ((Value*)arr->data)[i]);\n"
        "    }\n"
        "    return result;\n"
        "  } else if (obj.type == TYPE_STRING) {\n"
        "    char *str = obj.v.s;\n"
        "    int len = strlen(str);\n"
        "    if (start < 0) start = 0;\n"
        "    if (end > len) end = len;\n"
        "    if (start > end) start = end;\n"
        "    int slice_len = end - start;\n"
        "    char *result_str = malloc(slice_len + 1);\n"
        "    strncpy(result_str, str + start, slice_len);\n"
        "    result_str[slice_len] = '\\0';\n"
        "    return make_string(result_str);\n"
        "  }\n"
        "  return make_int(0);\n"
        "}\n\n"

        "Value input(Value prompt) {\n"
        "  char buffer[1024];\n"
        "  if (prompt.type == TYPE_STRING) printf(\"%%s\", prompt.v.s);\n"
        "  if (fgets(buffer, sizeof(buffer), stdin) != NULL) {\n"
        "    int len = strlen(buffer);\n"
        "    if (len > 0 && buffer[len-1] == '\\n') buffer[len-1] = '\\0';\n"
        "    return make_string(buffer);\n"
        "  }\n"
        "  return make_string(\"\");\n"
        "}\n\n"

        "Value read(Value filename) {\n"
        "  FILE *fp = fopen(filename.v.s, \"r\");\n"
        "  if (fp == NULL) { fprintf(stderr, \"Error reading file\\n\"); exit(1); }\n"
        "  fseek(fp, 0, SEEK_END);\n"
        "  long fsize = ftell(fp);\n"
        "  fseek(fp, 0, SEEK_SET);\n"
        "  char *content = malloc(fsize + 1);\n"
        "  fread(content, 1, fsize, fp);\n"
        "  content[fsize] = '\\0';\n"
        "  fclose(fp);\n"
        "  return make_string(content);\n"
        "}\n\n"

        "Value write(Value content, Value filename) {\n"
        "  FILE *fp = fopen(filename.v.s, \"w\");\n"
        "  if (fp == NULL) { fprintf(stderr, \"Error writing file\\n\"); exit(1); }\n"
        "  if (content.type == TYPE_STRING) fprintf(fp, \"%%s\", content.v.s);\n"
        "  else if (content.type == TYPE_INT) fprintf(fp, \"%%ld\", content.v.i);\n"
        "  else if (content.type == TYPE_FLOAT) fprintf(fp, \"%%g\", content.v.f);\n"
        "  fclose(fp);\n"
        "  return make_int(0);\n"
        "}\n\n"
    );

    // First pass: Generate forward declarations for user-defined functions
    if (root->type == NODE_PROGRAM) {
        ASTNodeList *stmt = root->data.program.statements;
        while (stmt != NULL) {
            if (stmt->node->type == NODE_FUNC_DEF) {
                // Generate function signature
                fprintf(gen->out, "Value %s(", stmt->node->data.func_def.name);

                // Generate parameter list
                ASTNodeList *param = stmt->node->data.func_def.params;
                int first = 1;
                while (param != NULL) {
                    if (!first) fprintf(gen->out, ", ");
                    fprintf(gen->out, "Value %s", param->node->data.identifier.name);
                    first = 0;
                    param = param->next;
                }

                fprintf(gen->out, ");\n");
            }
            stmt = stmt->next;
        }
        fprintf(gen->out, "\n");
    }

    // Second pass: Generate function implementations
    if (root->type == NODE_PROGRAM) {
        ASTNodeList *stmt = root->data.program.statements;
        while (stmt != NULL) {
            if (stmt->node->type == NODE_FUNC_DEF) {
                // Generate function signature
                fprintf(gen->out, "Value %s(", stmt->node->data.func_def.name);

                // Generate parameter list
                ASTNodeList *param = stmt->node->data.func_def.params;
                int first = 1;
                while (param != NULL) {
                    if (!first) fprintf(gen->out, ", ");
                    fprintf(gen->out, "Value %s", param->node->data.identifier.name);
                    first = 0;
                    param = param->next;
                }

                fprintf(gen->out, ") {\n");

                // Generate function body
                gen->indent_level = 1;
                ASTNodeList *body_stmt = stmt->node->data.func_def.body;
                while (body_stmt != NULL) {
                    gen_statement(gen, body_stmt->node);
                    body_stmt = body_stmt->next;
                }

                // If no explicit return, return 0
                emit(gen, "return make_int(0);\n");
                fprintf(gen->out, "}\n\n");
            }
            stmt = stmt->next;
        }
    }

    // Main function
    fprintf(gen->out, "int main() {\n");
    gen->indent_level = 1;

    // Third pass: Generate code for non-function statements only
    if (root->type == NODE_PROGRAM) {
        ASTNodeList *stmt = root->data.program.statements;
        while (stmt != NULL) {
            if (stmt->node->type != NODE_FUNC_DEF) {
                gen_statement(gen, stmt->node);
            }
            stmt = stmt->next;
        }
    }

    emit(gen, "return 0;\n");
    fprintf(gen->out, "}\n");
}
