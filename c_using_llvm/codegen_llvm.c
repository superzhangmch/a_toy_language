#include "codegen_llvm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void llvm_codegen_init(LLVMCodeGen *gen, FILE *out) {
    gen->out = out;
    gen->indent_level = 0;
    gen->temp_counter = 0;
    gen->label_counter = 0;
    gen->string_counter = 0;
    gen->strings = NULL;
}

static void emit_indent(LLVMCodeGen *gen) {
    for (int i = 0; i < gen->indent_level; i++) {
        fprintf(gen->out, "  ");
    }
}

static char* new_temp(LLVMCodeGen *gen) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%%t%d", gen->temp_counter++);
    return buf;
}

// Forward declarations
static void gen_expr(LLVMCodeGen *gen, ASTNode *node, char *result_var);
static void gen_statement(LLVMCodeGen *gen, ASTNode *node);

// Helper to register a string literal and return its global name
static const char* register_string_literal(LLVMCodeGen *gen, const char *str) {
    // Check if string already registered
    StringLiteral *s = gen->strings;
    while (s != NULL) {
        if (strcmp(s->value, str) == 0) {
            return s->global_name;
        }
        s = s->next;
    }

    // Create new string literal entry
    StringLiteral *new_str = malloc(sizeof(StringLiteral));
    new_str->value = strdup(str);
    new_str->global_name = malloc(64);
    snprintf(new_str->global_name, 64, "@.str_%d", gen->string_counter++);
    new_str->next = gen->strings;
    gen->strings = new_str;

    return new_str->global_name;
}

// Emit all collected string literals
static void emit_string_literals(LLVMCodeGen *gen) {
    StringLiteral *s = gen->strings;
    while (s != NULL) {
        int len = strlen(s->value) + 1;
        fprintf(gen->out, "%s = private unnamed_addr constant [%d x i8] c\"", s->global_name, len);

        // Emit escaped string content
        for (const char *p = s->value; *p; p++) {
            switch (*p) {
                case '\n': fprintf(gen->out, "\\0A"); break;
                case '\r': fprintf(gen->out, "\\0D"); break;
                case '\t': fprintf(gen->out, "\\09"); break;
                case '\\': fprintf(gen->out, "\\\\"); break;
                case '"':  fprintf(gen->out, "\\22"); break;
                default:
                    if (*p >= 32 && *p <= 126) {
                        fprintf(gen->out, "%c", *p);
                    } else {
                        fprintf(gen->out, "\\%02X", (unsigned char)*p);
                    }
            }
        }
        fprintf(gen->out, "\\00\", align 1\n");
        s = s->next;
    }
}

// Pre-pass to collect all string literals
static void collect_strings_expr(LLVMCodeGen *gen, ASTNode *node);
static void collect_strings_stmt(LLVMCodeGen *gen, ASTNode *node);

static void collect_strings_expr(LLVMCodeGen *gen, ASTNode *node) {
    if (node == NULL) return;

    switch (node->type) {
        case NODE_STRING_LITERAL:
            register_string_literal(gen, node->data.string_literal.value);
            break;
        case NODE_BINARY_OP:
            collect_strings_expr(gen, node->data.binary_op.left);
            collect_strings_expr(gen, node->data.binary_op.right);
            break;
        case NODE_FUNC_CALL: {
            ASTNodeList *arg = node->data.func_call.arguments;
            while (arg != NULL) {
                collect_strings_expr(gen, arg->node);
                arg = arg->next;
            }
            break;
        }
        case NODE_INDEX_ACCESS:
            collect_strings_expr(gen, node->data.index_access.object);
            collect_strings_expr(gen, node->data.index_access.index);
            break;
        case NODE_SLICE_ACCESS:
            collect_strings_expr(gen, node->data.slice_access.object);
            collect_strings_expr(gen, node->data.slice_access.start);
            collect_strings_expr(gen, node->data.slice_access.end);
            break;
        case NODE_ARRAY_LITERAL: {
            ASTNodeList *elem = node->data.array_literal.elements;
            while (elem != NULL) {
                collect_strings_expr(gen, elem->node);
                elem = elem->next;
            }
            break;
        }
        default:
            break;
    }
}

static void collect_strings_stmt(LLVMCodeGen *gen, ASTNode *node) {
    if (node == NULL) return;

    switch (node->type) {
        case NODE_VAR_DECL:
            collect_strings_expr(gen, node->data.var_decl.value);
            break;
        case NODE_ASSIGNMENT:
            collect_strings_expr(gen, node->data.assignment.target);
            collect_strings_expr(gen, node->data.assignment.value);
            break;
        case NODE_IF_STMT:
            collect_strings_expr(gen, node->data.if_stmt.condition);
            {
                ASTNodeList *s = node->data.if_stmt.then_block;
                while (s != NULL) {
                    collect_strings_stmt(gen, s->node);
                    s = s->next;
                }
                s = node->data.if_stmt.else_block;
                while (s != NULL) {
                    collect_strings_stmt(gen, s->node);
                    s = s->next;
                }
            }
            break;
        case NODE_WHILE_STMT:
            collect_strings_expr(gen, node->data.while_stmt.condition);
            {
                ASTNodeList *stmt = node->data.while_stmt.body;
                while (stmt != NULL) {
                    collect_strings_stmt(gen, stmt->node);
                    stmt = stmt->next;
                }
            }
            break;
        case NODE_RETURN:
            collect_strings_expr(gen, node->data.return_stmt.value);
            break;
        case NODE_FUNC_DEF: {
            ASTNodeList *stmt = node->data.func_def.body;
            while (stmt != NULL) {
                collect_strings_stmt(gen, stmt->node);
                stmt = stmt->next;
            }
            break;
        }
        default:
            // For expression statements
            collect_strings_expr(gen, node);
            break;
    }
}

static void emit_runtime_decls(LLVMCodeGen *gen) {
    fprintf(gen->out,
        "; Runtime type definition\n"
        "%%Value = type { i32, i64 }  ; { type_tag, data }\n\n"

        "; Type tags\n"
        "@TYPE_INT = constant i32 0\n"
        "@TYPE_FLOAT = constant i32 1\n"
        "@TYPE_STRING = constant i32 2\n"
        "@TYPE_ARRAY = constant i32 3\n\n"

        "; Operator tags\n"
        "@OP_ADD = constant i32 0\n"
        "@OP_SUB = constant i32 1\n"
        "@OP_MUL = constant i32 2\n"
        "@OP_DIV = constant i32 3\n"
        "@OP_MOD = constant i32 4\n"
        "@OP_EQ = constant i32 5\n"
        "@OP_NE = constant i32 6\n"
        "@OP_LT = constant i32 7\n"
        "@OP_LE = constant i32 8\n"
        "@OP_GT = constant i32 9\n"
        "@OP_GE = constant i32 10\n\n"

        "; String literals\n"
        "@empty_str = private unnamed_addr constant [1 x i8] c\"\\00\", align 1\n\n"

        "; Runtime function declarations\n"
        "declare %%Value @make_array()\n"
        "declare %%Value @append(%%Value, %%Value)\n"
        "declare %%Value @array_get(%%Value, %%Value)\n"
        "declare %%Value @array_set(%%Value, %%Value, %%Value)\n"
        "declare %%Value @len(%%Value)\n"
        "declare %%Value @str(%%Value)\n"
        "declare %%Value @type(%%Value)\n"
        "declare %%Value @to_int(%%Value)\n"
        "declare %%Value @to_float(%%Value)\n"
        "declare %%Value @to_string(%%Value)\n"
        "declare %%Value @slice_access(%%Value, %%Value, %%Value)\n"
        "declare %%Value @input(%%Value)\n"
        "declare %%Value @read(%%Value)\n"
        "declare %%Value @write(%%Value, %%Value)\n\n"
    );
}

static void emit_runtime_impl(LLVMCodeGen *gen) {
    fprintf(gen->out,
        "; ===== Runtime Implementation =====\n\n"

        "define %%Value @make_int(i64 %%val) {\n"
        "  %%result = insertvalue %%Value { i32 0, i64 0 }, i32 0, 0\n"
        "  %%result2 = insertvalue %%Value %%result, i64 %%val, 1\n"
        "  ret %%Value %%result2\n"
        "}\n\n"

        "define %%Value @make_float(double %%val) {\n"
        "  %%as_int = bitcast double %%val to i64\n"
        "  %%result = insertvalue %%Value { i32 1, i64 0 }, i32 1, 0\n"
        "  %%result2 = insertvalue %%Value %%result, i64 %%as_int, 1\n"
        "  ret %%Value %%result2\n"
        "}\n\n"

        "define %%Value @make_string(i8* %%val) {\n"
        "  %%as_int = ptrtoint i8* %%val to i64\n"
        "  %%result = insertvalue %%Value { i32 2, i64 0 }, i32 2, 0\n"
        "  %%result2 = insertvalue %%Value %%result, i64 %%as_int, 1\n"
        "  ret %%Value %%result2\n"
        "}\n\n"

        "define i32 @is_truthy(%%Value %%v) {\n"
        "  %%type = extractvalue %%Value %%v, 0\n"
        "  %%data = extractvalue %%Value %%v, 1\n"
        "  %%is_zero = icmp eq i64 %%data, 0\n"
        "  %%is_nonzero = icmp ne i64 %%data, 0\n"
        "  %%result = select i1 %%is_nonzero, i32 1, i32 0\n"
        "  ret i32 %%result\n"
        "}\n\n"

        "define %%Value @binary_op(%%Value %%left, i32 %%op, %%Value %%right) {\n"
        "entry:\n"
        "  %%l_data = extractvalue %%Value %%left, 1\n"
        "  %%r_data = extractvalue %%Value %%right, 1\n"
        "  switch i32 %%op, label %%default [\n"
        "    i32 0, label %%op_add\n"
        "    i32 1, label %%op_sub\n"
        "    i32 2, label %%op_mul\n"
        "    i32 3, label %%op_div\n"
        "    i32 4, label %%op_mod\n"
        "    i32 5, label %%op_eq\n"
        "    i32 6, label %%op_ne\n"
        "    i32 7, label %%op_lt\n"
        "    i32 8, label %%op_le\n"
        "    i32 9, label %%op_gt\n"
        "    i32 10, label %%op_ge\n"
        "  ]\n"
        "op_add:\n"
        "  %%add_result = add i64 %%l_data, %%r_data\n"
        "  br label %%done\n"
        "op_sub:\n"
        "  %%sub_result = sub i64 %%l_data, %%r_data\n"
        "  br label %%done\n"
        "op_mul:\n"
        "  %%mul_result = mul i64 %%l_data, %%r_data\n"
        "  br label %%done\n"
        "op_div:\n"
        "  %%div_result = sdiv i64 %%l_data, %%r_data\n"
        "  br label %%done\n"
        "op_mod:\n"
        "  %%mod_result = srem i64 %%l_data, %%r_data\n"
        "  br label %%done\n"
        "op_eq:\n"
        "  %%eq_cmp = icmp eq i64 %%l_data, %%r_data\n"
        "  %%eq_val = select i1 %%eq_cmp, i64 1, i64 0\n"
        "  br label %%done\n"
        "op_ne:\n"
        "  %%ne_cmp = icmp ne i64 %%l_data, %%r_data\n"
        "  %%ne_val = select i1 %%ne_cmp, i64 1, i64 0\n"
        "  br label %%done\n"
        "op_lt:\n"
        "  %%lt_cmp = icmp slt i64 %%l_data, %%r_data\n"
        "  %%lt_val = select i1 %%lt_cmp, i64 1, i64 0\n"
        "  br label %%done\n"
        "op_le:\n"
        "  %%le_cmp = icmp sle i64 %%l_data, %%r_data\n"
        "  %%le_val = select i1 %%le_cmp, i64 1, i64 0\n"
        "  br label %%done\n"
        "op_gt:\n"
        "  %%gt_cmp = icmp sgt i64 %%l_data, %%r_data\n"
        "  %%gt_val = select i1 %%gt_cmp, i64 1, i64 0\n"
        "  br label %%done\n"
        "op_ge:\n"
        "  %%ge_cmp = icmp sge i64 %%l_data, %%r_data\n"
        "  %%ge_val = select i1 %%ge_cmp, i64 1, i64 0\n"
        "  br label %%done\n"
        "default:\n"
        "  br label %%done\n"
        "done:\n"
        "  %%final = phi i64 [ %%add_result, %%op_add ], [ %%sub_result, %%op_sub ], [ %%mul_result, %%op_mul ], [ %%div_result, %%op_div ], [ %%mod_result, %%op_mod ], [ %%eq_val, %%op_eq ], [ %%ne_val, %%op_ne ], [ %%lt_val, %%op_lt ], [ %%le_val, %%op_le ], [ %%gt_val, %%op_gt ], [ %%ge_val, %%op_ge ], [ 0, %%default ]\n"
        "  %%result = call %%Value @make_int(i64 %%final)\n"
        "  ret %%Value %%result\n"
        "}\n\n"

        "declare i32 @printf(i8*, ...)\n"
        "declare i8* @malloc(i64)\n"
        "declare void @free(i8*)\n"
        "declare i64 @strlen(i8*)\n"
        "declare i8* @strcpy(i8*, i8*)\n"
        "declare i8* @strcat(i8*, i8*)\n\n"

        "@.str_int = private unnamed_addr constant [5 x i8] c\"%%ld \\00\", align 1\n"
        "@.str_str = private unnamed_addr constant [3 x i8] c\"%%s\\00\", align 1\n\n"

        "define void @print_value(%%Value %%v) {\n"
        "  %%type = extractvalue %%Value %%v, 0\n"
        "  %%data = extractvalue %%Value %%v, 1\n"
        "  %%is_int = icmp eq i32 %%type, 0\n"
        "  %%is_string = icmp eq i32 %%type, 2\n"
        "  br i1 %%is_int, label %%print_int, label %%check_string\n"
        "\n"
        "print_int:\n"
        "  %%fmt_int = getelementptr [5 x i8], [5 x i8]* @.str_int, i64 0, i64 0\n"
        "  call i32 (i8*, ...) @printf(i8* %%fmt_int, i64 %%data)\n"
        "  ret void\n"
        "\n"
        "check_string:\n"
        "  br i1 %%is_string, label %%print_string, label %%print_end\n"
        "\n"
        "print_string:\n"
        "  %%str_ptr = inttoptr i64 %%data to i8*\n"
        "  %%fmt_str = getelementptr [3 x i8], [3 x i8]* @.str_str, i64 0, i64 0\n"
        "  call i32 (i8*, ...) @printf(i8* %%fmt_str, i8* %%str_ptr)\n"
        "  ret void\n"
        "\n"
        "print_end:\n"
        "  ret void\n"
        "}\n\n"
    );
}

static void gen_expr(LLVMCodeGen *gen, ASTNode *node, char *result_var) {
    switch (node->type) {
        case NODE_INT_LITERAL: {
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @make_int(i64 %d)\n",
                    result_var, node->data.int_literal.value);
            break;
        }

        case NODE_STRING_LITERAL: {
            const char *global_name = register_string_literal(gen, node->data.string_literal.value);
            int len = strlen(node->data.string_literal.value) + 1;
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @make_string(i8* getelementptr inbounds ([%d x i8], [%d x i8]* %s, i32 0, i32 0))\n",
                    result_var, len, len, global_name);
            break;
        }

        case NODE_IDENTIFIER: {
            emit_indent(gen);
            fprintf(gen->out, "%s = load %%Value, %%Value* %%%s\n",
                    result_var, node->data.identifier.name);
            break;
        }

        case NODE_BINARY_OP: {
            char left_temp[32];
            char right_temp[32];
            snprintf(left_temp, sizeof(left_temp), "%%t%d", gen->temp_counter++);
            snprintf(right_temp, sizeof(right_temp), "%%t%d", gen->temp_counter++);

            gen_expr(gen, node->data.binary_op.left, left_temp);
            gen_expr(gen, node->data.binary_op.right, right_temp);

            int op_code = 0;
            switch (node->data.binary_op.op) {
                case OP_ADD: op_code = 0; break;
                case OP_SUB: op_code = 1; break;
                case OP_MUL: op_code = 2; break;
                case OP_DIV: op_code = 3; break;
                case OP_MOD: op_code = 4; break;
                case OP_EQ: op_code = 5; break;
                case OP_NE: op_code = 6; break;
                case OP_LT: op_code = 7; break;
                case OP_LE: op_code = 8; break;
                case OP_GT: op_code = 9; break;
                case OP_GE: op_code = 10; break;
                default: op_code = 0; break;
            }

            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @binary_op(%%Value %s, i32 %d, %%Value %s)\n",
                    result_var, left_temp, op_code, right_temp);
            break;
        }

        case NODE_ARRAY_LITERAL: {
            // For array literals with elements, we need to:
            // 1. Create a temporary variable to hold the array
            // 2. Create empty array and store it
            // 3. Append each element
            // 4. Load final array value into result_var

            char temp_var[32];
            snprintf(temp_var, sizeof(temp_var), "%%arr_lit_%d", gen->temp_counter++);

            // Allocate temporary variable
            emit_indent(gen);
            fprintf(gen->out, "%s = alloca %%Value\n", temp_var);

            // Create empty array
            char arr_init[32];
            snprintf(arr_init, sizeof(arr_init), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @make_array()\n", arr_init);

            // Store initial empty array
            emit_indent(gen);
            fprintf(gen->out, "store %%Value %s, %%Value* %s\n", arr_init, temp_var);

            // Append each element
            ASTNodeList *elem = node->data.array_literal.elements;
            while (elem != NULL) {
                char elem_temp[32];
                char arr_load[32];
                char append_result[32];
                snprintf(elem_temp, sizeof(elem_temp), "%%t%d", gen->temp_counter++);
                snprintf(arr_load, sizeof(arr_load), "%%t%d", gen->temp_counter++);
                snprintf(append_result, sizeof(append_result), "%%t%d", gen->temp_counter++);

                // Load current array value
                emit_indent(gen);
                fprintf(gen->out, "%s = load %%Value, %%Value* %s\n", arr_load, temp_var);

                // Generate element expression
                gen_expr(gen, elem->node, elem_temp);

                // Call append
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @append(%%Value %s, %%Value %s)\n",
                        append_result, arr_load, elem_temp);

                elem = elem->next;
            }

            // Load final array value into result_var
            emit_indent(gen);
            fprintf(gen->out, "%s = load %%Value, %%Value* %s\n", result_var, temp_var);
            break;
        }

        case NODE_INDEX_ACCESS: {
            char obj_temp[32];
            char idx_temp[32];
            snprintf(obj_temp, sizeof(obj_temp), "%%t%d", gen->temp_counter++);
            snprintf(idx_temp, sizeof(idx_temp), "%%t%d", gen->temp_counter++);

            gen_expr(gen, node->data.index_access.object, obj_temp);
            gen_expr(gen, node->data.index_access.index, idx_temp);

            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @array_get(%%Value %s, %%Value %s)\n",
                    result_var, obj_temp, idx_temp);
            break;
        }

        case NODE_SLICE_ACCESS: {
            char obj_temp[32];
            char start_temp[32];
            char end_temp[32];
            snprintf(obj_temp, sizeof(obj_temp), "%%t%d", gen->temp_counter++);
            snprintf(start_temp, sizeof(start_temp), "%%t%d", gen->temp_counter++);
            snprintf(end_temp, sizeof(end_temp), "%%t%d", gen->temp_counter++);

            gen_expr(gen, node->data.slice_access.object, obj_temp);
            gen_expr(gen, node->data.slice_access.start, start_temp);
            gen_expr(gen, node->data.slice_access.end, end_temp);

            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @slice_access(%%Value %s, %%Value %s, %%Value %s)\n",
                    result_var, obj_temp, start_temp, end_temp);
            break;
        }

        case NODE_FUNC_CALL: {
            // Evaluate arguments
            int arg_count = 0;
            ASTNodeList *arg = node->data.func_call.arguments;
            while (arg != NULL) {
                arg_count++;
                arg = arg->next;
            }

            char **arg_temps = malloc(arg_count * sizeof(char*));
            arg = node->data.func_call.arguments;
            for (int i = 0; i < arg_count; i++) {
                arg_temps[i] = strdup(new_temp(gen));
                gen_expr(gen, arg->node, arg_temps[i]);
                arg = arg->next;
            }

            // Check for built-in functions
            if (strcmp(node->data.func_call.name, "print") == 0) {
                emit_indent(gen);
                fprintf(gen->out, "call void @print_value(%%Value %s)\n", arg_temps[0]);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @make_int(i64 0)\n", result_var);
            } else {
                // User function call
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @%s(", result_var, node->data.func_call.name);
                for (int i = 0; i < arg_count; i++) {
                    if (i > 0) fprintf(gen->out, ", ");
                    fprintf(gen->out, "%%Value %s", arg_temps[i]);
                }
                fprintf(gen->out, ")\n");
            }

            for (int i = 0; i < arg_count; i++) {
                free(arg_temps[i]);
            }
            free(arg_temps);
            break;
        }

        default:
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @make_int(i64 0) ; unhandled expr\n", result_var);
            break;
    }
}

static void gen_statement(LLVMCodeGen *gen, ASTNode *node) {
    switch (node->type) {
        case NODE_VAR_DECL: {
            // Allocate space on stack
            emit_indent(gen);
            fprintf(gen->out, "%%%s = alloca %%Value\n", node->data.var_decl.name);

            // Evaluate initial value
            char val_temp[32];
            snprintf(val_temp, sizeof(val_temp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.var_decl.value, val_temp);

            // Store to variable
            emit_indent(gen);
            fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n",
                    val_temp, node->data.var_decl.name);
            break;
        }

        case NODE_ASSIGNMENT: {
            // Evaluate value
            char val_temp[32];
            snprintf(val_temp, sizeof(val_temp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.assignment.value, val_temp);

            // Store to variable or array element
            if (node->data.assignment.target->type == NODE_IDENTIFIER) {
                emit_indent(gen);
                fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n",
                        val_temp, node->data.assignment.target->data.identifier.name);
            } else if (node->data.assignment.target->type == NODE_INDEX_ACCESS) {
                // Array assignment: array_set(arr, idx, val)
                char obj_temp[32];
                char idx_temp[32];
                snprintf(obj_temp, sizeof(obj_temp), "%%t%d", gen->temp_counter++);
                snprintf(idx_temp, sizeof(idx_temp), "%%t%d", gen->temp_counter++);

                gen_expr(gen, node->data.assignment.target->data.index_access.object, obj_temp);
                gen_expr(gen, node->data.assignment.target->data.index_access.index, idx_temp);

                char result_temp[32];
                snprintf(result_temp, sizeof(result_temp), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @array_set(%%Value %s, %%Value %s, %%Value %s)\n",
                        result_temp, obj_temp, idx_temp, val_temp);
            }
            break;
        }

        case NODE_FUNC_CALL: {
            char temp[32];
            snprintf(temp, sizeof(temp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node, temp);
            break;
        }

        case NODE_IF_STMT: {
            char cond_temp[32];
            snprintf(cond_temp, sizeof(cond_temp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.if_stmt.condition, cond_temp);

            char truthy_temp[32];
            snprintf(truthy_temp, sizeof(truthy_temp), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = call i32 @is_truthy(%%Value %s)\n", truthy_temp, cond_temp);

            char cmp_temp[32];
            snprintf(cmp_temp, sizeof(cmp_temp), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = icmp ne i32 %s, 0\n", cmp_temp, truthy_temp);

            char then_label[32], else_label[32], end_label[32];
            snprintf(then_label, sizeof(then_label), "label%d", gen->label_counter++);
            if (node->data.if_stmt.else_block) {
                snprintf(else_label, sizeof(else_label), "label%d", gen->label_counter++);
            }
            snprintf(end_label, sizeof(end_label), "label%d", gen->label_counter++);

            emit_indent(gen);
            fprintf(gen->out, "br i1 %s, label %%%s, label %%%s\n",
                    cmp_temp, then_label, node->data.if_stmt.else_block ? else_label : end_label);

            // Then block
            fprintf(gen->out, "\n%s:\n", then_label);
            gen->indent_level++;
            ASTNodeList *stmt = node->data.if_stmt.then_block;
            while (stmt != NULL) {
                gen_statement(gen, stmt->node);
                stmt = stmt->next;
            }
            emit_indent(gen);
            fprintf(gen->out, "br label %%%s\n", end_label);
            gen->indent_level--;

            // Else block
            if (node->data.if_stmt.else_block) {
                fprintf(gen->out, "\n%s:\n", else_label);
                gen->indent_level++;
                stmt = node->data.if_stmt.else_block;
                while (stmt != NULL) {
                    gen_statement(gen, stmt->node);
                    stmt = stmt->next;
                }
                emit_indent(gen);
                fprintf(gen->out, "br label %%%s\n", end_label);
                gen->indent_level--;
            }

            fprintf(gen->out, "\n%s:\n", end_label);
            break;
        }

        case NODE_WHILE_STMT: {
            char cond_label[32], body_label[32], end_label[32];
            snprintf(cond_label, sizeof(cond_label), "label%d", gen->label_counter++);
            snprintf(body_label, sizeof(body_label), "label%d", gen->label_counter++);
            snprintf(end_label, sizeof(end_label), "label%d", gen->label_counter++);

            emit_indent(gen);
            fprintf(gen->out, "br label %%%s\n", cond_label);

            // Condition check
            fprintf(gen->out, "\n%s:\n", cond_label);
            gen->indent_level++;
            char cond_temp[32];
            snprintf(cond_temp, sizeof(cond_temp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.while_stmt.condition, cond_temp);

            char truthy_temp[32];
            snprintf(truthy_temp, sizeof(truthy_temp), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = call i32 @is_truthy(%%Value %s)\n", truthy_temp, cond_temp);

            char cmp_temp[32];
            snprintf(cmp_temp, sizeof(cmp_temp), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = icmp ne i32 %s, 0\n", cmp_temp, truthy_temp);

            emit_indent(gen);
            fprintf(gen->out, "br i1 %s, label %%%s, label %%%s\n", cmp_temp, body_label, end_label);
            gen->indent_level--;

            // Loop body
            fprintf(gen->out, "\n%s:\n", body_label);
            gen->indent_level++;
            ASTNodeList *stmt = node->data.while_stmt.body;
            while (stmt != NULL) {
                gen_statement(gen, stmt->node);
                stmt = stmt->next;
            }
            emit_indent(gen);
            fprintf(gen->out, "br label %%%s\n", cond_label);
            gen->indent_level--;

            fprintf(gen->out, "\n%s:\n", end_label);
            break;
        }

        case NODE_RETURN: {
            if (node->data.return_stmt.value) {
                char val_temp[32];
                snprintf(val_temp, sizeof(val_temp), "%%t%d", gen->temp_counter++);
                gen_expr(gen, node->data.return_stmt.value, val_temp);
                emit_indent(gen);
                fprintf(gen->out, "ret %%Value %s\n", val_temp);
            } else {
                emit_indent(gen);
                fprintf(gen->out, "ret %%Value { i32 0, i64 0 }\n");
            }
            break;
        }

        case NODE_FUNC_DEF:
            // Functions are handled separately
            break;

        default:
            break;
    }
}

void llvm_codegen_program(LLVMCodeGen *gen, ASTNode *root) {
    if (root->type != NODE_PROGRAM) {
        fprintf(stderr, "Error: Expected program node\n");
        return;
    }

    // Pre-pass: collect all string literals
    ASTNodeList *s = root->data.program.statements;
    while (s != NULL) {
        collect_strings_stmt(gen, s->node);
        s = s->next;
    }

    // Emit string literals
    fprintf(gen->out, "; String literals\n");
    emit_string_literals(gen);
    fprintf(gen->out, "\n");

    // Emit declarations
    emit_runtime_decls(gen);

    // Emit runtime implementation
    emit_runtime_impl(gen);

    // Generate user function implementations
    fprintf(gen->out, "; ===== User Function Implementations =====\n\n");
    ASTNodeList *stmt = root->data.program.statements;
    while (stmt != NULL) {
        if (stmt->node->type == NODE_FUNC_DEF) {
            fprintf(gen->out, "define %%Value @%s(", stmt->node->data.func_def.name);

            ASTNodeList *param = stmt->node->data.func_def.params;
            int first = 1;
            while (param != NULL) {
                if (!first) fprintf(gen->out, ", ");
                fprintf(gen->out, "%%Value %%param_%s", param->node->data.identifier.name);
                first = 0;
                param = param->next;
            }

            fprintf(gen->out, ") {\n");
            gen->indent_level = 1;

            // Allocate and initialize parameters
            param = stmt->node->data.func_def.params;
            while (param != NULL) {
                emit_indent(gen);
                fprintf(gen->out, "%%%s = alloca %%Value\n", param->node->data.identifier.name);
                emit_indent(gen);
                fprintf(gen->out, "store %%Value %%param_%s, %%Value* %%%s\n",
                        param->node->data.identifier.name, param->node->data.identifier.name);
                param = param->next;
            }

            // Generate function body
            ASTNodeList *body_stmt = stmt->node->data.func_def.body;
            while (body_stmt != NULL) {
                gen_statement(gen, body_stmt->node);
                body_stmt = body_stmt->next;
            }

            // Default return if no explicit return
            emit_indent(gen);
            fprintf(gen->out, "ret %%Value { i32 0, i64 0 }\n");

            fprintf(gen->out, "}\n\n");
            gen->indent_level = 0;
        }
        stmt = stmt->next;
    }

    // Generate main function
    fprintf(gen->out, "; ===== Main Function =====\n\n");
    fprintf(gen->out, "define i32 @main() {\n");
    gen->indent_level = 1;

    stmt = root->data.program.statements;
    while (stmt != NULL) {
        if (stmt->node->type != NODE_FUNC_DEF) {
            gen_statement(gen, stmt->node);
        }
        stmt = stmt->next;
    }

    emit_indent(gen);
    fprintf(gen->out, "ret i32 0\n");
    fprintf(gen->out, "}\n");
}
