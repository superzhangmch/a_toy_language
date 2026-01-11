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
    gen->scope_counter = 0;
    gen->strings = NULL;
    gen->var_mappings = NULL;
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

// Get unique name for an existing variable (lookup only)
static const char* get_unique_var_name(LLVMCodeGen *gen, const char *original_name) {
    // Look up in reverse order (most recent first)
    VarMapping *m = gen->var_mappings;
    while (m != NULL) {
        if (strcmp(m->original_name, original_name) == 0) {
            return m->unique_name;
        }
        m = m->next;
    }

    // If not found, return NULL (this indicates a bug - variable not declared)
    fprintf(stderr, "Internal error: Variable '%s' not found in mapping\n", original_name);
    return original_name;
}

// Create a new unique name for a variable declaration (always creates new)
static const char* create_unique_var_name(LLVMCodeGen *gen, const char *original_name) {
    // Always create new mapping with unique suffix
    VarMapping *new_mapping = malloc(sizeof(VarMapping));
    new_mapping->original_name = strdup(original_name);
    new_mapping->unique_name = malloc(256);
    snprintf(new_mapping->unique_name, 256, "%s_%d", original_name, gen->scope_counter++);
    new_mapping->next = gen->var_mappings;
    gen->var_mappings = new_mapping;

    return new_mapping->unique_name;
}

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
        case NODE_DICT_LITERAL: {
            ASTNodeList *pair = node->data.dict_literal.pairs;
            while (pair != NULL) {
                ASTNode *pair_node = pair->node;
                collect_strings_expr(gen, pair_node->data.dict_pair.key);
                collect_strings_expr(gen, pair_node->data.dict_pair.value);
                pair = pair->next;
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
        case NODE_FOREACH_STMT:
            collect_strings_expr(gen, node->data.foreach_stmt.collection);
            {
                ASTNodeList *stmt = node->data.foreach_stmt.body;
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
        "@TYPE_ARRAY = constant i32 3\n"
        "@TYPE_DICT = constant i32 4\n\n"

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
        "declare %%Value @index_get(%%Value, %%Value)\n"
        "declare %%Value @index_set(%%Value, %%Value, %%Value)\n"
        "declare %%Value @len(%%Value)\n"
        "declare %%Value @str(%%Value)\n"
        "declare %%Value @type(%%Value)\n"
        "declare %%Value @to_int(%%Value)\n"
        "declare %%Value @to_float(%%Value)\n"
        "declare %%Value @to_string(%%Value)\n"
        "declare %%Value @slice_access(%%Value, %%Value, %%Value)\n"
        "declare %%Value @input(%%Value)\n"
        "declare %%Value @read(%%Value)\n"
        "declare %%Value @write(%%Value, %%Value)\n"
        "declare %%Value @make_dict()\n"
        "declare %%Value @dict_set(%%Value, %%Value, %%Value)\n"
        "declare %%Value @dict_get(%%Value, %%Value)\n"
        "declare %%Value @dict_has(%%Value, %%Value)\n"
        "declare %%Value @dict_keys(%%Value)\n"
        "declare %%Value @keys(%%Value)\n"
        "declare %%Value @in_operator(%%Value, %%Value)\n"
        "declare %%Value @binary_op(%%Value, i32, %%Value)\n"
        "declare %%Value @regexp_match(%%Value, %%Value)\n"
        "declare %%Value @regexp_find(%%Value, %%Value)\n"
        "declare %%Value @regexp_replace(%%Value, %%Value, %%Value)\n"
        "declare %%Value @str_split(%%Value, %%Value)\n"
        "declare %%Value @str_join(%%Value, %%Value)\n"
        "declare %%Value @cmd_args()\n\n"
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

        "declare i32 @printf(i8*, ...)\n"
        "declare i8* @malloc(i64)\n"
        "declare void @free(i8*)\n"
        "declare i64 @strlen(i8*)\n"
        "declare i8* @strcpy(i8*, i8*)\n"
        "declare i8* @strcat(i8*, i8*)\n"
        "declare void @print_value(%%Value)\n"
        "declare void @set_cmd_args(i32, i8**)\n\n"

        "@.str_newline = private unnamed_addr constant [2 x i8] c\"\\0A\\00\", align 1\n\n"
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
            const char *unique_name = get_unique_var_name(gen, node->data.identifier.name);
            emit_indent(gen);
            fprintf(gen->out, "%s = load %%Value, %%Value* %%%s\n",
                    result_var, unique_name);
            break;
        }

        case NODE_BINARY_OP: {
            char left_temp[32];
            char right_temp[32];
            snprintf(left_temp, sizeof(left_temp), "%%t%d", gen->temp_counter++);
            snprintf(right_temp, sizeof(right_temp), "%%t%d", gen->temp_counter++);

            gen_expr(gen, node->data.binary_op.left, left_temp);
            gen_expr(gen, node->data.binary_op.right, right_temp);

            // Special handling for IN operator
            if (node->data.binary_op.op == OP_IN) {
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @in_operator(%%Value %s, %%Value %s)\n",
                        result_var, left_temp, right_temp);
                break;
            }

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
                case OP_AND: op_code = 11; break;
                case OP_OR: op_code = 12; break;
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

        case NODE_DICT_LITERAL: {
            // For dict literals with key-value pairs:
            // 1. Create a temporary variable to hold the dict
            // 2. Create empty dict and store it
            // 3. Set each key-value pair
            // 4. Load final dict value into result_var

            char temp_var[32];
            snprintf(temp_var, sizeof(temp_var), "%%dict_lit_%d", gen->temp_counter++);

            // Allocate temporary variable
            emit_indent(gen);
            fprintf(gen->out, "%s = alloca %%Value\n", temp_var);

            // Create empty dict
            char dict_init[32];
            snprintf(dict_init, sizeof(dict_init), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @make_dict()\n", dict_init);

            // Store initial empty dict
            emit_indent(gen);
            fprintf(gen->out, "store %%Value %s, %%Value* %s\n", dict_init, temp_var);

            // Set each key-value pair
            ASTNodeList *pair = node->data.dict_literal.pairs;
            while (pair != NULL) {
                ASTNode *pair_node = pair->node;

                char key_temp[32], val_temp[32], dict_load[32], set_result[32];
                snprintf(key_temp, sizeof(key_temp), "%%t%d", gen->temp_counter++);
                snprintf(val_temp, sizeof(val_temp), "%%t%d", gen->temp_counter++);
                snprintf(dict_load, sizeof(dict_load), "%%t%d", gen->temp_counter++);
                snprintf(set_result, sizeof(set_result), "%%t%d", gen->temp_counter++);

                // Load current dict value
                emit_indent(gen);
                fprintf(gen->out, "%s = load %%Value, %%Value* %s\n", dict_load, temp_var);

                // Generate key expression
                gen_expr(gen, pair_node->data.dict_pair.key, key_temp);

                // Generate value expression
                gen_expr(gen, pair_node->data.dict_pair.value, val_temp);

                // Call dict_set
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @dict_set(%%Value %s, %%Value %s, %%Value %s)\n",
                        set_result, dict_load, key_temp, val_temp);

                pair = pair->next;
            }

            // Load final dict value into result_var
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
            // Use generic index_get which handles array, dict, and string
            fprintf(gen->out, "%s = call %%Value @index_get(%%Value %s, %%Value %s)\n",
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
                for (int i = 0; i < arg_count; i++) {
                    emit_indent(gen);
                    fprintf(gen->out, "call void @print_value(%%Value %s)\n", arg_temps[i]);
                }
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @make_int(i64 0)\n", result_var);
            } else if (strcmp(node->data.func_call.name, "println") == 0) {
                for (int i = 0; i < arg_count; i++) {
                    emit_indent(gen);
                    fprintf(gen->out, "call void @print_value(%%Value %s)\n", arg_temps[i]);
                }
                // Print newline after all arguments
                char newline_temp[32];
                snprintf(newline_temp, sizeof(newline_temp), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = getelementptr [2 x i8], [2 x i8]* @.str_newline, i64 0, i64 0\n", newline_temp);
                emit_indent(gen);
                fprintf(gen->out, "call i32 (i8*, ...) @printf(i8* %s)\n", newline_temp);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @make_int(i64 0)\n", result_var);
            } else {
                // Map builtin function names to runtime function names
                const char *runtime_name = node->data.func_call.name;
                if (strcmp(node->data.func_call.name, "int") == 0) {
                    runtime_name = "to_int";
                } else if (strcmp(node->data.func_call.name, "float") == 0) {
                    runtime_name = "to_float";
                }

                // User function call
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @%s(", result_var, runtime_name);
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
            // Create unique name for this new variable declaration
            const char *unique_name = create_unique_var_name(gen, node->data.var_decl.name);

            // Allocate space on stack
            emit_indent(gen);
            fprintf(gen->out, "%%%s = alloca %%Value\n", unique_name);

            // Evaluate initial value
            char val_temp[32];
            snprintf(val_temp, sizeof(val_temp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.var_decl.value, val_temp);

            // Store to variable
            emit_indent(gen);
            fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n",
                    val_temp, unique_name);
            break;
        }

        case NODE_ASSIGNMENT: {
            // Evaluate value
            char val_temp[32];
            snprintf(val_temp, sizeof(val_temp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.assignment.value, val_temp);

            // Store to variable or array element
            if (node->data.assignment.target->type == NODE_IDENTIFIER) {
                const char *unique_name = get_unique_var_name(gen, node->data.assignment.target->data.identifier.name);
                emit_indent(gen);
                fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n",
                        val_temp, unique_name);
            } else if (node->data.assignment.target->type == NODE_INDEX_ACCESS) {
                // Index assignment: index_set(obj, idx, val) - handles both array and dict
                char obj_temp[32];
                char idx_temp[32];
                snprintf(obj_temp, sizeof(obj_temp), "%%t%d", gen->temp_counter++);
                snprintf(idx_temp, sizeof(idx_temp), "%%t%d", gen->temp_counter++);

                gen_expr(gen, node->data.assignment.target->data.index_access.object, obj_temp);
                gen_expr(gen, node->data.assignment.target->data.index_access.index, idx_temp);

                char result_temp[32];
                snprintf(result_temp, sizeof(result_temp), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @index_set(%%Value %s, %%Value %s, %%Value %s)\n",
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

        case NODE_FOREACH_STMT: {
            // Generate foreach loop for arrays and dicts
            char collection_temp[32], type_temp[32], type_field_temp[32];
            snprintf(collection_temp, sizeof(collection_temp), "%%t%d", gen->temp_counter++);
            snprintf(type_temp, sizeof(type_temp), "%%t%d", gen->temp_counter++);
            snprintf(type_field_temp, sizeof(type_field_temp), "%%t%d", gen->temp_counter++);

            gen_expr(gen, node->data.foreach_stmt.collection, collection_temp);

            // Get type field
            emit_indent(gen);
            fprintf(gen->out, "%s = extractvalue %%Value %s, 0\n", type_field_temp, collection_temp);

            // Check if array (type == 3)
            emit_indent(gen);
            fprintf(gen->out, "%s = icmp eq i32 %s, 3\n", type_temp, type_field_temp);

            char array_label[32], dict_label[32], end_label[32];
            snprintf(array_label, sizeof(array_label), "label%d", gen->label_counter++);
            snprintf(dict_label, sizeof(dict_label), "label%d", gen->label_counter++);
            snprintf(end_label, sizeof(end_label), "label%d", gen->label_counter++);

            emit_indent(gen);
            fprintf(gen->out, "br i1 %s, label %%%s, label %%%s\n", type_temp, array_label, dict_label);

            // Array foreach
            fprintf(gen->out, "\n%s:\n", array_label);
            gen->indent_level++;
            {
                char len_temp[32], i_ptr[32];
                snprintf(len_temp, sizeof(len_temp), "%%t%d", gen->temp_counter++);
                snprintf(i_ptr, sizeof(i_ptr), "%%t%d", gen->temp_counter++);

                // Get array length
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @len(%%Value %s)\n", len_temp, collection_temp);

                // Allocate loop variables with unique names (these are new declarations)
                const char *key_var = create_unique_var_name(gen, node->data.foreach_stmt.key_var);
                const char *value_var = create_unique_var_name(gen, node->data.foreach_stmt.value_var);

                emit_indent(gen);
                fprintf(gen->out, "%%%s = alloca %%Value\n", key_var);
                emit_indent(gen);
                fprintf(gen->out, "%%%s = alloca %%Value\n", value_var);

                // Allocate loop counter
                emit_indent(gen);
                fprintf(gen->out, "%s = alloca i64\n", i_ptr);
                emit_indent(gen);
                fprintf(gen->out, "store i64 0, i64* %s\n", i_ptr);

                char loop_cond[32], loop_body[32], loop_incr[32], loop_end[32];
                snprintf(loop_cond, sizeof(loop_cond), "label%d", gen->label_counter++);
                snprintf(loop_body, sizeof(loop_body), "label%d", gen->label_counter++);
                snprintf(loop_incr, sizeof(loop_incr), "label%d", gen->label_counter++);
                snprintf(loop_end, sizeof(loop_end), "label%d", gen->label_counter++);

                emit_indent(gen);
                fprintf(gen->out, "br label %%%s\n", loop_cond);

                // Loop condition
                fprintf(gen->out, "\n%s:\n", loop_cond);
                gen->indent_level++;

                char i_val[32], len_val[32], cmp_result[32];
                snprintf(i_val, sizeof(i_val), "%%t%d", gen->temp_counter++);
                snprintf(len_val, sizeof(len_val), "%%t%d", gen->temp_counter++);
                snprintf(cmp_result, sizeof(cmp_result), "%%t%d", gen->temp_counter++);

                emit_indent(gen);
                fprintf(gen->out, "%s = load i64, i64* %s\n", i_val, i_ptr);
                emit_indent(gen);
                fprintf(gen->out, "%s = extractvalue %%Value %s, 1\n", len_val, len_temp);
                emit_indent(gen);
                fprintf(gen->out, "%s = icmp slt i64 %s, %s\n", cmp_result, i_val, len_val);
                emit_indent(gen);
                fprintf(gen->out, "br i1 %s, label %%%s, label %%%s\n", cmp_result, loop_body, loop_end);
                gen->indent_level--;

                // Loop body
                fprintf(gen->out, "\n%s:\n", loop_body);
                gen->indent_level++;

                // Set key_var = i
                char key_val_temp[32], value_val_temp[32], idx_val_temp[32];
                snprintf(key_val_temp, sizeof(key_val_temp), "%%t%d", gen->temp_counter++);
                snprintf(idx_val_temp, sizeof(idx_val_temp), "%%t%d", gen->temp_counter++);
                snprintf(value_val_temp, sizeof(value_val_temp), "%%t%d", gen->temp_counter++);

                emit_indent(gen);
                fprintf(gen->out, "%s = load i64, i64* %s\n", idx_val_temp, i_ptr);
                emit_indent(gen);
                fprintf(gen->out, "%s = insertvalue %%Value { i32 0, i64 0 }, i64 %s, 1\n", key_val_temp, idx_val_temp);

                // Store key_var
                emit_indent(gen);
                fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n", key_val_temp, key_var);

                // Get value using array_get
                char array_get_idx[32];
                snprintf(array_get_idx, sizeof(array_get_idx), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = insertvalue %%Value { i32 0, i64 0 }, i64 %s, 1\n", array_get_idx, idx_val_temp);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @index_get(%%Value %s, %%Value %s)\n",
                        value_val_temp, collection_temp, array_get_idx);

                // Store value_var
                emit_indent(gen);
                fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n", value_val_temp, value_var);

                // Execute body statements
                ASTNodeList *stmt = node->data.foreach_stmt.body;
                while (stmt != NULL) {
                    gen_statement(gen, stmt->node);
                    stmt = stmt->next;
                }

                emit_indent(gen);
                fprintf(gen->out, "br label %%%s\n", loop_incr);
                gen->indent_level--;

                // Loop increment
                fprintf(gen->out, "\n%s:\n", loop_incr);
                gen->indent_level++;
                char i_next[32], i_curr[32];
                snprintf(i_curr, sizeof(i_curr), "%%t%d", gen->temp_counter++);
                snprintf(i_next, sizeof(i_next), "%%t%d", gen->temp_counter++);

                emit_indent(gen);
                fprintf(gen->out, "%s = load i64, i64* %s\n", i_curr, i_ptr);
                emit_indent(gen);
                fprintf(gen->out, "%s = add i64 %s, 1\n", i_next, i_curr);
                emit_indent(gen);
                fprintf(gen->out, "store i64 %s, i64* %s\n", i_next, i_ptr);
                emit_indent(gen);
                fprintf(gen->out, "br label %%%s\n", loop_cond);
                gen->indent_level--;

                fprintf(gen->out, "\n%s:\n", loop_end);
                emit_indent(gen);
                fprintf(gen->out, "br label %%%s\n", end_label);
            }
            gen->indent_level--;

            // Dict foreach (simplified - just jump to end for now)
            fprintf(gen->out, "\n%s:\n", dict_label);
            gen->indent_level++;
            emit_indent(gen);
            fprintf(gen->out, "; TODO: dict foreach not yet implemented in codegen\n");
            emit_indent(gen);
            fprintf(gen->out, "br label %%%s\n", end_label);
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
    fprintf(gen->out, "define i32 @main(i32 %%argc, i8** %%argv) {\n");
    gen->indent_level = 1;

    // Call set_cmd_args to store command line arguments
    emit_indent(gen);
    fprintf(gen->out, "call void @set_cmd_args(i32 %%argc, i8** %%argv)\n\n");

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
