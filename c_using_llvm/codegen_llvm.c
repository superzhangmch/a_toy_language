#include "codegen_llvm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

void llvm_codegen_init(LLVMCodeGen *gen, FILE *out) {
    gen->out = out;
    gen->indent_level = 0;
    gen->temp_counter = 0;
    gen->label_counter = 0;
    gen->string_counter = 0;
    gen->scope_counter = 0;
    gen->scope_depth = 0;
    gen->strings = NULL;
    gen->var_mappings = NULL;
    gen->global_vars = NULL;
    gen->break_label = NULL;
    gen->continue_label = NULL;
    gen->functions = NULL;
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
static VarMapping* find_var_mapping_current_scope(LLVMCodeGen *gen, const char *original_name);
static VarMapping* push_scope(LLVMCodeGen *gen, int *saved_depth) {
    if (saved_depth) *saved_depth = gen->scope_depth;
    gen->scope_depth++;
    return gen->var_mappings;
}
static void pop_scope(LLVMCodeGen *gen, VarMapping *saved, int saved_depth) {
    gen->var_mappings = saved;
    gen->scope_depth = saved_depth;
}
static const char* create_unique_var_name(LLVMCodeGen *gen, const char *original_name, int is_global);
static void codegen_error(ASTNode *node, const char *fmt, ...) {
    va_list ap;
    if (node && node->file) {
        fprintf(stderr, "Error at %s:%d: ", node->file, node->line);
    } else {
        fprintf(stderr, "Error: ");
    }
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

// Helper to generate field initializer functions
static void gen_field_init_function(LLVMCodeGen *gen, const char *class_name, ASTNode *member_decl) {
    int saved_depth = 0;
    VarMapping *saved = push_scope(gen, &saved_depth);
    const char *field_name = member_decl->data.var_decl.name;
    fprintf(gen->out, "define %%Value @__field_init_%s_%s(%%Value %%this) {\n", class_name, field_name);
    gen->indent_level = 1;

    const char *this_unique = create_unique_var_name(gen, "this", 0);
    VarMapping *m_this = find_var_mapping_current_scope(gen, "this");
    if (m_this) m_this->declared = 1;
    emit_indent(gen);
    fprintf(gen->out, "%%%s = alloca %%Value\n", this_unique);
    emit_indent(gen);
    fprintf(gen->out, "store %%Value %%this, %%Value* %%%s\n", this_unique);

    const char *self_unique = create_unique_var_name(gen, "self", 0);
    VarMapping *m_self = find_var_mapping_current_scope(gen, "self");
    if (m_self) m_self->declared = 1;
    emit_indent(gen);
    fprintf(gen->out, "%%%s = alloca %%Value\n", self_unique);
    emit_indent(gen);
    fprintf(gen->out, "store %%Value %%this, %%Value* %%%s\n", self_unique);

    char val_temp[32];
    snprintf(val_temp, sizeof(val_temp), "%%t%d", gen->temp_counter++);
    gen_expr(gen, member_decl->data.var_decl.value, val_temp);

    emit_indent(gen);
    fprintf(gen->out, "ret %%Value %s\n", val_temp);
    fprintf(gen->out, "}\n\n");
    gen->indent_level = 0;
    pop_scope(gen, saved, saved_depth);
}

// Helper to generate method functions with (this, args, arg_count) signature
static void gen_method_function(LLVMCodeGen *gen, const char *class_name, ASTNode *func_def) {
    int saved_depth = 0;
    VarMapping *saved = push_scope(gen, &saved_depth);
    fprintf(gen->out, "define %%Value @%s__%s(%%Value %%this, %%Value* %%args, i32 %%arg_count) {\n",
            class_name, func_def->data.func_def.name);
    gen->indent_level = 1;

    const char *this_unique = create_unique_var_name(gen, "this", 0);
    VarMapping *m_this = find_var_mapping_current_scope(gen, "this");
    if (m_this) m_this->declared = 1;
    emit_indent(gen);
    fprintf(gen->out, "%%%s = alloca %%Value\n", this_unique);
    emit_indent(gen);
    fprintf(gen->out, "store %%Value %%this, %%Value* %%%s\n", this_unique);

    const char *self_unique = create_unique_var_name(gen, "self", 0);
    VarMapping *m_self = find_var_mapping_current_scope(gen, "self");
    if (m_self) m_self->declared = 1;
    emit_indent(gen);
    fprintf(gen->out, "%%%s = alloca %%Value\n", self_unique);
    emit_indent(gen);
    fprintf(gen->out, "store %%Value %%this, %%Value* %%%s\n", self_unique);

    ASTNodeList *param = func_def->data.func_def.params;
    int param_index = 0;
    while (param != NULL) {
        const char *pname = param->node->data.identifier.name;
        const char *unique = create_unique_var_name(gen, pname, 0);
        VarMapping *pm = find_var_mapping_current_scope(gen, pname);
        if (pm) pm->declared = 1;
        emit_indent(gen);
        fprintf(gen->out, "%%%s = alloca %%Value\n", unique);

        char arg_ptr[32];
        snprintf(arg_ptr, sizeof(arg_ptr), "%%t%d", gen->temp_counter++);
        emit_indent(gen);
        fprintf(gen->out, "%s = getelementptr %%Value, %%Value* %%args, i32 %d\n", arg_ptr, param_index);

        char arg_val[32];
        snprintf(arg_val, sizeof(arg_val), "%%t%d", gen->temp_counter++);
        emit_indent(gen);
        fprintf(gen->out, "%s = load %%Value, %%Value* %s\n", arg_val, arg_ptr);

        emit_indent(gen);
        fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n", arg_val, unique);

        param_index++;
        param = param->next;
    }

    ASTNodeList *body_stmt = func_def->data.func_def.body;
    while (body_stmt != NULL) {
        gen_statement(gen, body_stmt->node);
        body_stmt = body_stmt->next;
    }

    emit_indent(gen);
    fprintf(gen->out, "ret %%Value { i32 0, i64 0 }\n");
    fprintf(gen->out, "}\n\n");
    gen->indent_level = 0;
    pop_scope(gen, saved, saved_depth);
}

// Get unique name for an existing variable (lookup only)
static VarMapping* find_var_mapping(LLVMCodeGen *gen, const char *original_name) {
    VarMapping *m = gen->var_mappings;
    while (m != NULL) {
        if (strcmp(m->original_name, original_name) == 0) {
            return m;
        }
        m = m->next;
    }
    return NULL;
}

// Lookup only within the current lexical scope depth
static VarMapping* find_var_mapping_current_scope(LLVMCodeGen *gen, const char *original_name) {
    VarMapping *m = gen->var_mappings;
    while (m != NULL) {
        if (m->scope_depth == gen->scope_depth && strcmp(m->original_name, original_name) == 0) {
            return m;
        }
        m = m->next;
    }
    return NULL;
}

// Create a new unique name for a variable declaration (always creates new)
static const char* create_unique_var_name(LLVMCodeGen *gen, const char *original_name, int is_global) {
    // Always create new mapping with unique suffix
    VarMapping *new_mapping = malloc(sizeof(VarMapping));
    new_mapping->original_name = strdup(original_name);
    new_mapping->unique_name = malloc(256);
    if (is_global) {
        snprintf(new_mapping->unique_name, 256, "g_%s", original_name);
    } else {
        snprintf(new_mapping->unique_name, 256, "%s_%d", original_name, gen->scope_counter++);
    }
    new_mapping->is_global = is_global;
    new_mapping->scope_depth = gen->scope_depth;
    new_mapping->declared = 0;
    new_mapping->next_global = NULL;
    new_mapping->next = gen->var_mappings;
    gen->var_mappings = new_mapping;
    if (is_global) {
        new_mapping->next_global = gen->global_vars;
        gen->global_vars = new_mapping;
    }

    return new_mapping->unique_name;
}

static FuncInfo* find_function(LLVMCodeGen *gen, const char *name) {
    FuncInfo *f = gen->functions;
    while (f) {
        if (strcmp(f->name, name) == 0) return f;
        f = f->next;
    }
    return NULL;
}

static void register_function(LLVMCodeGen *gen, const char *name, int arity, const char *file, int line) {
    if (find_function(gen, name)) {
        ASTNode dummy = {.file = (char*)file, .line = line};
        codegen_error(&dummy, "Function '%s' redefined (codegen)", name);
    }
    FuncInfo *f = malloc(sizeof(FuncInfo));
    f->name = strdup(name);
    f->arity = arity;
    f->next = gen->functions;
    gen->functions = f;
}

// Pre-register globals that live in the top-level (including try/catch blocks which are not new scopes)
static void preregister_globals_in_list(LLVMCodeGen *gen, ASTNodeList *list, int is_global_scope) {
    while (list != NULL) {
        ASTNode *n = list->node;
        if (is_global_scope && n->type == NODE_VAR_DECL) {
            if (!find_var_mapping_current_scope(gen, n->data.var_decl.name)) {
                create_unique_var_name(gen, n->data.var_decl.name, 1);
            } else {
                codegen_error(n, "Redefinition of '%s' in the same scope (codegen)", n->data.var_decl.name);
            }
        } else if (is_global_scope && n->type == NODE_MULTI_VAR_DECL) {
            /* Register each variable in the multi-declaration */
            ASTNodeList *decl_list = n->data.multi_var_decl.declarations;
            while (decl_list) {
                ASTNode *decl = decl_list->node;
                if (!find_var_mapping_current_scope(gen, decl->data.var_decl.name)) {
                    create_unique_var_name(gen, decl->data.var_decl.name, 1);
                } else {
                    codegen_error(decl, "Redefinition of '%s' in the same scope (codegen)", decl->data.var_decl.name);
                }
                decl_list = decl_list->next;
            }
        } else if (is_global_scope && n->type == NODE_CLASS_DEF) {
            if (find_var_mapping_current_scope(gen, n->data.class_def.name)) {
                codegen_error(n, "Redefinition of class '%s' in the same scope (codegen)", n->data.class_def.name);
            }
            create_unique_var_name(gen, n->data.class_def.name, 1);
        } else if (n->type == NODE_TRY_CATCH) {
            preregister_globals_in_list(gen, n->data.try_catch.try_block, is_global_scope);
            preregister_globals_in_list(gen, n->data.try_catch.catch_block, is_global_scope);
        }
        list = list->next;
    }
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
static void register_functions_stmt(LLVMCodeGen *gen, ASTNode *node);
static void register_file_strings_expr(LLVMCodeGen *gen, ASTNode *node);
static void register_file_strings_stmt(LLVMCodeGen *gen, ASTNode *node);

static void collect_strings_expr(LLVMCodeGen *gen, ASTNode *node) {
    if (node == NULL) return;

    switch (node->type) {
        case NODE_STRING_LITERAL:
            register_string_literal(gen, node->data.string_literal.value);
            break;
        case NODE_BINARY_OP:
            if (node->file) register_string_literal(gen, node->file);
            collect_strings_expr(gen, node->data.binary_op.left);
            collect_strings_expr(gen, node->data.binary_op.right);
            break;
        case NODE_UNARY_OP:
            collect_strings_expr(gen, node->data.unary_op.operand);
            break;
        case NODE_FUNC_CALL: {
            ASTNodeList *arg = node->data.func_call.arguments;
            while (arg != NULL) {
                collect_strings_expr(gen, arg->node);
                arg = arg->next;
            }
            if (strcmp(node->data.func_call.name, "json_decode") == 0) {
                register_string_literal(gen, node->file ? node->file : "<input>");
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
        case NODE_MEMBER_ACCESS:
            register_string_literal(gen, node->data.member_access.member);
            collect_strings_expr(gen, node->data.member_access.object);
            break;
        case NODE_METHOD_CALL: {
            register_string_literal(gen, node->data.method_call.method);
            collect_strings_expr(gen, node->data.method_call.object);
            ASTNodeList *arg = node->data.method_call.arguments;
            while (arg != NULL) {
                collect_strings_expr(gen, arg->node);
                arg = arg->next;
            }
            break;
        }
        case NODE_NEW_EXPR: {
            // class name will be accessed as identifier; nothing extra
            ASTNodeList *arg = node->data.new_expr.arguments;
            while (arg != NULL) {
                collect_strings_expr(gen, arg->node);
                arg = arg->next;
            }
            break;
        }
        default:
            break;
    }
}

// Register file path strings for runtime error context
static void register_file_strings_expr(LLVMCodeGen *gen, ASTNode *node) {
    if (!node) return;
    if (node->file) register_string_literal(gen, node->file);
    switch (node->type) {
        case NODE_BINARY_OP:
            register_file_strings_expr(gen, node->data.binary_op.left);
            register_file_strings_expr(gen, node->data.binary_op.right);
            break;
        case NODE_UNARY_OP:
            register_file_strings_expr(gen, node->data.unary_op.operand);
            break;
        case NODE_INDEX_ACCESS:
            register_file_strings_expr(gen, node->data.index_access.object);
            register_file_strings_expr(gen, node->data.index_access.index);
            break;
        case NODE_SLICE_ACCESS:
            register_file_strings_expr(gen, node->data.slice_access.object);
            register_file_strings_expr(gen, node->data.slice_access.start);
            register_file_strings_expr(gen, node->data.slice_access.end);
            break;
        case NODE_ARRAY_LITERAL: {
            ASTNodeList *e = node->data.array_literal.elements;
            while (e) { register_file_strings_expr(gen, e->node); e = e->next; }
            break;
        }
        case NODE_DICT_LITERAL: {
            ASTNodeList *p = node->data.dict_literal.pairs;
            while (p) {
                register_file_strings_expr(gen, p->node->data.dict_pair.key);
                register_file_strings_expr(gen, p->node->data.dict_pair.value);
                p = p->next;
            }
            break;
        }
        case NODE_MEMBER_ACCESS:
            register_file_strings_expr(gen, node->data.member_access.object);
            break;
        case NODE_METHOD_CALL: {
            register_file_strings_expr(gen, node->data.method_call.object);
            ASTNodeList *a = node->data.method_call.arguments;
            while (a) { register_file_strings_expr(gen, a->node); a = a->next; }
            break;
        }
        case NODE_FUNC_CALL: {
            ASTNodeList *a = node->data.func_call.arguments;
            while (a) { register_file_strings_expr(gen, a->node); a = a->next; }
            break;
        }
        case NODE_NEW_EXPR: {
            ASTNodeList *a = node->data.new_expr.arguments;
            while (a) { register_file_strings_expr(gen, a->node); a = a->next; }
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
        case NODE_MULTI_VAR_DECL: {
            ASTNodeList *decl_list = node->data.multi_var_decl.declarations;
            while (decl_list) {
                collect_strings_stmt(gen, decl_list->node);
                decl_list = decl_list->next;
            }
            break;
        }
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

        case NODE_FOR_STMT:
            collect_strings_expr(gen, node->data.for_stmt.start);
            collect_strings_expr(gen, node->data.for_stmt.end);
            {
                ASTNodeList *stmt = node->data.for_stmt.body;
                while (stmt != NULL) {
                    collect_strings_stmt(gen, stmt->node);
                    stmt = stmt->next;
                }
            }
            break;

        case NODE_TRY_CATCH: {
            register_string_literal(gen, node->file ? node->file : "<input>");
            register_string_literal(gen, "[caught in ");
            char buf[64];
            snprintf(buf, sizeof(buf), ":%d] ", node->line);
            register_string_literal(gen, buf);
            ASTNodeList *stmt = node->data.try_catch.try_block;
            while (stmt != NULL) {
                collect_strings_stmt(gen, stmt->node);
                stmt = stmt->next;
            }
            stmt = node->data.try_catch.catch_block;
            while (stmt != NULL) {
                collect_strings_stmt(gen, stmt->node);
                stmt = stmt->next;
            }
            break;
        }

        case NODE_RAISE:
            register_string_literal(gen, node->file ? node->file : "<input>");
            collect_strings_expr(gen, node->data.raise_stmt.expr);
            break;

        case NODE_ASSERT:
            register_string_literal(gen, node->file ? node->file : "<input>");
            collect_strings_expr(gen, node->data.assert_stmt.expr);
            if (node->data.assert_stmt.msg) {
                collect_strings_expr(gen, node->data.assert_stmt.msg);
            } else {
                register_string_literal(gen, "Assertion failed");
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
        case NODE_CLASS_DEF: {
            register_string_literal(gen, node->data.class_def.name);
            ASTNodeList *m = node->data.class_def.members;
            while (m != NULL) {
                register_string_literal(gen, m->node->data.var_decl.name);
                collect_strings_expr(gen, m->node->data.var_decl.value);
                m = m->next;
            }
            ASTNodeList *meth = node->data.class_def.methods;
            while (meth != NULL) {
                register_string_literal(gen, meth->node->data.func_def.name);
                ASTNodeList *stmt2 = meth->node->data.func_def.body;
                while (stmt2 != NULL) {
                    collect_strings_stmt(gen, stmt2->node);
                    stmt2 = stmt2->next;
                }
                meth = meth->next;
            }
            break;
        }
        default:
            // For expression statements
            collect_strings_expr(gen, node);
            break;
    }
}

static void register_file_strings_stmt(LLVMCodeGen *gen, ASTNode *node) {
    if (!node) return;
    if (node->file) register_string_literal(gen, node->file);
    switch (node->type) {
        case NODE_VAR_DECL:
            register_file_strings_expr(gen, node->data.var_decl.value);
            break;
        case NODE_MULTI_VAR_DECL: {
            ASTNodeList *decl_list = node->data.multi_var_decl.declarations;
            while (decl_list) {
                register_file_strings_stmt(gen, decl_list->node);
                decl_list = decl_list->next;
            }
            break;
        }
        case NODE_ASSIGNMENT:
            register_file_strings_expr(gen, node->data.assignment.target);
            register_file_strings_expr(gen, node->data.assignment.value);
            break;
        case NODE_IF_STMT: {
            register_file_strings_expr(gen, node->data.if_stmt.condition);
            ASTNodeList *s = node->data.if_stmt.then_block;
            while (s) { register_file_strings_stmt(gen, s->node); s = s->next; }
            s = node->data.if_stmt.else_block;
            while (s) { register_file_strings_stmt(gen, s->node); s = s->next; }
            break;
        }
        case NODE_WHILE_STMT: {
            register_file_strings_expr(gen, node->data.while_stmt.condition);
            ASTNodeList *s = node->data.while_stmt.body;
            while (s) { register_file_strings_stmt(gen, s->node); s = s->next; }
            break;
        }
        case NODE_FOREACH_STMT: {
            register_file_strings_expr(gen, node->data.foreach_stmt.collection);
            ASTNodeList *s = node->data.foreach_stmt.body;
            while (s) { register_file_strings_stmt(gen, s->node); s = s->next; }
            break;
        }
        case NODE_FOR_STMT: {
            register_file_strings_expr(gen, node->data.for_stmt.start);
            register_file_strings_expr(gen, node->data.for_stmt.end);
            ASTNodeList *s = node->data.for_stmt.body;
            while (s) { register_file_strings_stmt(gen, s->node); s = s->next; }
            break;
        }
        case NODE_TRY_CATCH: {
            ASTNodeList *s = node->data.try_catch.try_block;
            while (s) { register_file_strings_stmt(gen, s->node); s = s->next; }
            s = node->data.try_catch.catch_block;
            while (s) { register_file_strings_stmt(gen, s->node); s = s->next; }
            break;
        }
        case NODE_FUNC_DEF: {
            ASTNodeList *s = node->data.func_def.body;
            while (s) { register_file_strings_stmt(gen, s->node); s = s->next; }
            break;
        }
        case NODE_CLASS_DEF: {
            ASTNodeList *m = node->data.class_def.members;
            while (m) { register_file_strings_expr(gen, m->node->data.var_decl.value); m = m->next; }
            ASTNodeList *meth = node->data.class_def.methods;
            while (meth) {
                ASTNodeList *s = meth->node->data.func_def.body;
                while (s) { register_file_strings_stmt(gen, s->node); s = s->next; }
                meth = meth->next;
            }
            break;
        }
        default:
            register_file_strings_expr(gen, node);
            break;
    }
}

// Pre-pass to register function arities and detect redefinitions
static void register_functions_stmt(LLVMCodeGen *gen, ASTNode *node) {
    if (node == NULL) return;
    switch (node->type) {
        case NODE_FUNC_DEF: {
            int arity = 0;
            ASTNodeList *p = node->data.func_def.params;
            while (p) { arity++; p = p->next; }
            register_function(gen, node->data.func_def.name, arity, node->file, node->line);
            break;
        }
        case NODE_TRY_CATCH: {
            ASTNodeList *stmt = node->data.try_catch.try_block;
            while (stmt) { register_functions_stmt(gen, stmt->node); stmt = stmt->next; }
            stmt = node->data.try_catch.catch_block;
            while (stmt) { register_functions_stmt(gen, stmt->node); stmt = stmt->next; }
            break;
        }
        default:
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
        "@TYPE_DICT = constant i32 4\n"
        "@TYPE_CLASS = constant i32 5\n"
        "@TYPE_INSTANCE = constant i32 6\n"
        "@TYPE_NULL = constant i32 7\n"
        "@TYPE_BOOL = constant i32 8\n\n"

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
        "@.str_trim_ws = private unnamed_addr constant [4 x i8] c\" \\09\\0A\\00\", align 1\n\n"

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
        "declare %%Value @make_null()\n"
        "declare %%Value @slice_access(%%Value, %%Value, %%Value)\n"
        "declare %%Value @input(%%Value)\n"
        "declare %%Value @file_read(%%Value)\n"
        "declare %%Value @file_write(%%Value, %%Value)\n"
        "declare %%Value @file_append(%%Value, %%Value)\n"
        "declare %%Value @file_size(%%Value)\n"
        "declare %%Value @file_exist(%%Value)\n"
        "declare %%Value @make_dict()\n"
        "declare %%Value @dict_set(%%Value, %%Value, %%Value)\n"
        "declare %%Value @dict_get(%%Value, %%Value)\n"
        "declare %%Value @dict_has(%%Value, %%Value)\n"
        "declare %%Value @dict_keys(%%Value)\n"
        "declare %%Value @keys(%%Value)\n"
        "declare %%Value @in_operator(%%Value, %%Value, i32, i8*)\n"
        "declare %%Value @not_in_operator(%%Value, %%Value, i32, i8*)\n"
        "declare %%Value @binary_op(%%Value, i32, %%Value, i32, i8*)\n"
        "declare %%Value @regexp_match(%%Value, %%Value)\n"
        "declare %%Value @regexp_find(%%Value, %%Value)\n"
        "declare %%Value @regexp_replace(%%Value, %%Value, %%Value)\n"
        "declare %%Value @str_split(%%Value, %%Value)\n"
        "declare %%Value @str_join(%%Value, %%Value)\n"
        "declare %%Value @str_trim(%%Value, %%Value)\n"
        "declare %%Value @str_format(%%Value, %%Value*, i32)\n"
        "declare %%Value @json_encode(%%Value)\n"
        "declare %%Value @json_decode_ctx(%%Value, i32, i8*)\n"
        "declare %%Value @math_random_val(%%Value, %%Value, i32)\n"
        "declare void @set_source_ctx(i32, i8*)\n"
        "declare double @sin(double)\n"
        "declare double @cos(double)\n"
        "declare double @asin(double)\n"
        "declare double @acos(double)\n"
        "declare double @log(double)\n"
        "declare double @exp(double)\n"
        "declare double @ceil(double)\n"
        "declare double @floor(double)\n"
        "declare double @round(double)\n"
        "declare double @sqrt(double)\n"
        "declare double @pow(double, double)\n"
        "declare i8* @__try_push_buf()\n"
        "declare void @__try_pop()\n"
        "declare void @__raise(%%Value, i32, i8*)\n"
        "declare %%Value @__get_exception()\n"
        "declare i32 @setjmp(i8*)\n"
        "declare %%Value @remove_entry(%%Value, %%Value)\n"
        "declare %%Value @cmd_args()\n"
        "declare %%Value @gc_stat()\n"
        "declare %%Value @gc_run()\n"
        "declare %%Value @make_class(i8*)\n"
        "declare void @class_add_field(%%Value, i8*, %%Value (%%Value)*, i32)\n"
        "declare void @class_add_method(%%Value, i8*, %%Value (%%Value, %%Value*, i32)*, i32, i32)\n"
        "declare %%Value @instantiate_class(%%Value, %%Value*, i32)\n"
        "declare %%Value @member_get(%%Value, i8*)\n"
        "declare %%Value @member_set(%%Value, i8*, %%Value)\n"
        "declare %%Value @method_call(%%Value, i8*, %%Value*, i32)\n\n"
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

        "define %%Value @make_bool(i1 %%val) {\n"
        "  %%ext = zext i1 %%val to i64\n"
        "  %%result = insertvalue %%Value { i32 8, i64 0 }, i32 8, 0\n"
        "  %%result2 = insertvalue %%Value %%result, i64 %%ext, 1\n"
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

        "define internal i32 @__is_truthy_ir(%%Value %%v) {\n"
        "entry:\n"
        "  %%type = extractvalue %%Value %%v, 0\n"
        "  %%data = extractvalue %%Value %%v, 1\n"
        "  %%is_null = icmp eq i32 %%type, 7\n"
        "  br i1 %%is_null, label %%ret_false, label %%check_str\n"
        "check_str:\n"
        "  %%is_str = icmp eq i32 %%type, 2\n"
        "  br i1 %%is_str, label %%len_str, label %%check_arr\n"
        "len_str:\n"
        "  %%lstr = call %%Value @len(%%Value %%v)\n"
        "  %%lsz = extractvalue %%Value %%lstr, 1\n"
        "  %%lsz_zero = icmp eq i64 %%lsz, 0\n"
        "  br i1 %%lsz_zero, label %%ret_false, label %%ret_true\n"
        "check_arr:\n"
        "  %%is_arr = icmp eq i32 %%type, 3\n"
        "  %%is_dict = icmp eq i32 %%type, 4\n"
        "  %%arr_or_dict = or i1 %%is_arr, %%is_dict\n"
        "  br i1 %%arr_or_dict, label %%len_arr, label %%default_check\n"
        "len_arr:\n"
        "  %%larr = call %%Value @len(%%Value %%v)\n"
        "  %%asz = extractvalue %%Value %%larr, 1\n"
        "  %%asz_zero = icmp eq i64 %%asz, 0\n"
        "  br i1 %%asz_zero, label %%ret_false, label %%ret_true\n"
        "default_check:\n"
        "  %%is_nonzero = icmp ne i64 %%data, 0\n"
        "  br i1 %%is_nonzero, label %%ret_true, label %%ret_false\n"
        "ret_true:\n"
        "  ret i32 1\n"
        "ret_false:\n"
        "  ret i32 0\n"
        "}\n\n"

        "declare i32 @printf(i8*, ...)\n"
        "declare i8* @malloc(i64)\n"
        "declare void @free(i8*)\n"
        "declare i64 @strlen(i8*)\n"
        "declare i8* @strcpy(i8*, i8*)\n"
        "declare i8* @strcat(i8*, i8*)\n"
        "declare void @print_value(%%Value)\n"
        "declare void @set_cmd_args(i32, i8**)\n"
        "declare void @gc_init()\n"
        "declare void @gc_set_stack_bottom(i8*)\n"
        "declare void @gc_push_root(%%Value*)\n\n"

        "@.str_newline = private unnamed_addr constant [2 x i8] c\"\\0A\\00\", align 1\n"
        "@.str_space = private unnamed_addr constant [2 x i8] c\" \\00\", align 1\n\n"
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

        case NODE_FLOAT_LITERAL: {
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @make_float(double %f)\n",
                    result_var, node->data.float_literal.value);
            break;
        }

        case NODE_BOOL_LITERAL: {
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @make_bool(i1 %d)\n",
                    result_var, node->data.bool_literal.value ? 1 : 0);
            break;
        }

        case NODE_NULL_LITERAL: {
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @make_null()\n", result_var);
            break;
        }

        case NODE_STRING_LITERAL: {
            const char *global_name = register_string_literal(gen, node->data.string_literal.value);
            int len = strlen(node->data.string_literal.value) + 1;
            char str_ptr[32];
            snprintf(str_ptr, sizeof(str_ptr), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                    str_ptr, len, len, global_name);
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @make_string(i8* %s)\n",
                    result_var, str_ptr);
            break;
        }

        case NODE_IDENTIFIER: {
            VarMapping *m = find_var_mapping(gen, node->data.identifier.name);
            if (m == NULL) {
                codegen_error(node, "Variable '%s' not declared in this scope (codegen)", node->data.identifier.name);
            }
            emit_indent(gen);
            if (m->is_global) {
                fprintf(gen->out, "%s = load %%Value, %%Value* @%s\n", result_var, m->unique_name);
            } else {
                fprintf(gen->out, "%s = load %%Value, %%Value* %%%s\n", result_var, m->unique_name);
            }
            break;
        }

        case NODE_MEMBER_ACCESS: {
            char obj_temp[32];
            snprintf(obj_temp, sizeof(obj_temp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.member_access.object, obj_temp);

            const char *global_name = register_string_literal(gen, node->data.member_access.member);
            int len = strlen(node->data.member_access.member) + 1;
            char str_ptr[32];
            snprintf(str_ptr, sizeof(str_ptr), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                    str_ptr, len, len, global_name);

            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @member_get(%%Value %s, i8* %s)\n",
                    result_var, obj_temp, str_ptr);
            break;
        }

        case NODE_BINARY_OP: {
            char left_temp[32];
            char right_temp[32];
            snprintf(left_temp, sizeof(left_temp), "%%t%d", gen->temp_counter++);
            snprintf(right_temp, sizeof(right_temp), "%%t%d", gen->temp_counter++);

            gen_expr(gen, node->data.binary_op.left, left_temp);
            gen_expr(gen, node->data.binary_op.right, right_temp);

            const char *file_global = register_string_literal(gen, node->file ? node->file : "<input>");
            int flen = strlen(node->file ? node->file : "<input>") + 1;
            char file_ptr[32];
            snprintf(file_ptr, sizeof(file_ptr), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                    file_ptr, flen, flen, file_global);

            // Special handling for IN / NOT_IN operators
            if (node->data.binary_op.op == OP_IN) {
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @in_operator(%%Value %s, %%Value %s, i32 %d, i8* %s)\n",
                        result_var, left_temp, right_temp, node->line, file_ptr);
                break;
            } else if (node->data.binary_op.op == OP_NOT_IN) {
                char in_tmp[32], truthy[32], cmp[32], bool_int[32], base_val[32];
                snprintf(in_tmp, sizeof(in_tmp), "%%t%d", gen->temp_counter++);
                snprintf(truthy, sizeof(truthy), "%%t%d", gen->temp_counter++);
                snprintf(cmp, sizeof(cmp), "%%t%d", gen->temp_counter++);
                snprintf(bool_int, sizeof(bool_int), "%%t%d", gen->temp_counter++);
                snprintf(base_val, sizeof(base_val), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @in_operator(%%Value %s, %%Value %s, i32 %d, i8* %s)\n",
                        in_tmp, left_temp, right_temp, node->line, file_ptr);
                emit_indent(gen);
                fprintf(gen->out, "%s = call i32 @__is_truthy_ir(%%Value %s)\n", truthy, in_tmp);
                emit_indent(gen);
                fprintf(gen->out, "%s = icmp eq i32 %s, 0\n", cmp, truthy);
                emit_indent(gen);
                fprintf(gen->out, "%s = zext i1 %s to i64\n", bool_int, cmp);
                emit_indent(gen);
                fprintf(gen->out, "%s = insertvalue %%Value { i32 0, i64 0 }, i32 0, 0\n", base_val);
                emit_indent(gen);
                fprintf(gen->out, "%s = insertvalue %%Value %s, i64 %s, 1\n", result_var, base_val, bool_int);
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
            fprintf(gen->out, "%s = call %%Value @binary_op(%%Value %s, i32 %d, %%Value %s, i32 %d, i8* %s)\n",
                    result_var, left_temp, op_code, right_temp, node->line, file_ptr);
            break;
        }

        case NODE_UNARY_OP: {
            char operand_temp[32];
            snprintf(operand_temp, sizeof(operand_temp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.unary_op.operand, operand_temp);

            if (node->data.unary_op.op == OP_NOT) {
                char truthy[32];
                snprintf(truthy, sizeof(truthy), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call i32 @__is_truthy_ir(%%Value %s)\n", truthy, operand_temp);
                char cmp[32];
                snprintf(cmp, sizeof(cmp), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = icmp eq i32 %s, 0\n", cmp, truthy);
                char bool_int[32];
                snprintf(bool_int, sizeof(bool_int), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = zext i1 %s to i64\n", bool_int, cmp);
                char base_val[32];
                snprintf(base_val, sizeof(base_val), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = insertvalue %%Value { i32 0, i64 0 }, i32 0, 0\n", base_val);
                emit_indent(gen);
                fprintf(gen->out, "%s = insertvalue %%Value %s, i64 %s, 1\n", result_var, base_val, bool_int);
            } else if (node->data.unary_op.op == OP_NEG) {
                char zero[32];
                snprintf(zero, sizeof(zero), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @make_int(i64 0)\n", zero);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @binary_op(%%Value %s, i32 1, %%Value %s)\n",
                        result_var, zero, operand_temp); // OP_SUB
            }
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
                    // Print space between arguments (but not after the last one)
                    if (i < arg_count - 1) {
                        char space_temp[32];
                        snprintf(space_temp, sizeof(space_temp), "%%t%d", gen->temp_counter++);
                        emit_indent(gen);
                        fprintf(gen->out, "%s = getelementptr [2 x i8], [2 x i8]* @.str_space, i64 0, i64 0\n", space_temp);
                        emit_indent(gen);
                        fprintf(gen->out, "call i32 (i8*, ...) @printf(i8* %s)\n", space_temp);
                    }
                }
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @make_int(i64 0)\n", result_var);
            } else if (strcmp(node->data.func_call.name, "println") == 0) {
                for (int i = 0; i < arg_count; i++) {
                    emit_indent(gen);
                    fprintf(gen->out, "call void @print_value(%%Value %s)\n", arg_temps[i]);
                    // Print space between arguments (but not after the last one)
                    if (i < arg_count - 1) {
                        char space_temp[32];
                        snprintf(space_temp, sizeof(space_temp), "%%t%d", gen->temp_counter++);
                        emit_indent(gen);
                        fprintf(gen->out, "%s = getelementptr [2 x i8], [2 x i8]* @.str_space, i64 0, i64 0\n", space_temp);
                        emit_indent(gen);
                        fprintf(gen->out, "call i32 (i8*, ...) @printf(i8* %s)\n", space_temp);
                    }
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
            } else if (strcmp(node->data.func_call.name, "remove") == 0) {
                if (arg_count != 2) {
                    codegen_error(node, "remove() requires 2 arguments");
                }
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @remove_entry(%%Value %s, %%Value %s)\n",
                        result_var, arg_temps[0], arg_temps[1]);
            } else if (strcmp(node->data.func_call.name, "json_encode") == 0) {
                if (arg_count != 1) {
                    codegen_error(node, "json_encode() requires 1 argument");
                }
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @json_encode(%%Value %s)\n",
                        result_var, arg_temps[0]);
            } else if (strcmp(node->data.func_call.name, "json_decode") == 0) {
                if (arg_count != 1) {
                    codegen_error(node, "json_decode() requires 1 argument");
                }
                const char *file_global = register_string_literal(gen, node->file ? node->file : "<input>");
                int flen = strlen(node->file ? node->file : "<input>") + 1;
                char file_ptr[32];
                snprintf(file_ptr, sizeof(file_ptr), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                        file_ptr, flen, flen, file_global);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @json_decode_ctx(%%Value %s, i32 %d, i8* %s)\n",
                        result_var, arg_temps[0], node->line, file_ptr);
            } else if (strcmp(node->data.func_call.name, "str_format") == 0) {
                if (arg_count < 1) { codegen_error(node, "str_format requires at least format"); }
                int fmt_args = arg_count - 1;
                char args_alloca[32];
                snprintf(args_alloca, sizeof(args_alloca), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = alloca [%d x %%Value]\n", args_alloca, fmt_args > 0 ? fmt_args : 1);
                for (int i = 0; i < fmt_args; i++) {
                    char arg_ptr[32];
                    snprintf(arg_ptr, sizeof(arg_ptr), "%%t%d", gen->temp_counter++);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = getelementptr [%d x %%Value], [%d x %%Value]* %s, i32 0, i32 %d\n",
                            arg_ptr, fmt_args > 0 ? fmt_args : 1, fmt_args > 0 ? fmt_args : 1, args_alloca, i);
                    emit_indent(gen);
                    fprintf(gen->out, "store %%Value %s, %%Value* %s\n", arg_temps[i+1], arg_ptr);
                }
                char args_base[32];
                snprintf(args_base, sizeof(args_base), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = getelementptr [%d x %%Value], [%d x %%Value]* %s, i32 0, i32 0\n",
                        args_base, fmt_args > 0 ? fmt_args : 1, fmt_args > 0 ? fmt_args : 1, args_alloca);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @str_format(%%Value %s, %%Value* %s, i32 %d)\n",
                        result_var, arg_temps[0], args_base, fmt_args);
            } else {
                int handled_math = 0;
                const char *fname = node->data.func_call.name;
                if (strcmp(fname, "round") == 0 && arg_count == 2) {
                    char v_f[32], v_bits[32], v_d[32];
                    char d_f[32], d_bits[32], d_d[32];
                    char scale[32], mul[32], rnd[32], res[32];
                    snprintf(v_f, sizeof(v_f), "%%t%d", gen->temp_counter++);
                    snprintf(v_bits, sizeof(v_bits), "%%t%d", gen->temp_counter++);
                    snprintf(v_d, sizeof(v_d), "%%t%d", gen->temp_counter++);
                    snprintf(d_f, sizeof(d_f), "%%t%d", gen->temp_counter++);
                    snprintf(d_bits, sizeof(d_bits), "%%t%d", gen->temp_counter++);
                    snprintf(d_d, sizeof(d_d), "%%t%d", gen->temp_counter++);
                    snprintf(scale, sizeof(scale), "%%t%d", gen->temp_counter++);
                    snprintf(mul, sizeof(mul), "%%t%d", gen->temp_counter++);
                    snprintf(rnd, sizeof(rnd), "%%t%d", gen->temp_counter++);
                    snprintf(res, sizeof(res), "%%t%d", gen->temp_counter++);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call %%Value @to_float(%%Value %s)\n", v_f, arg_temps[0]);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = extractvalue %%Value %s, 1\n", v_bits, v_f);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = bitcast i64 %s to double\n", v_d, v_bits);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call %%Value @to_float(%%Value %s)\n", d_f, arg_temps[1]);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = extractvalue %%Value %s, 1\n", d_bits, d_f);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = bitcast i64 %s to double\n", d_d, d_bits);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call double @pow(double 1.000000e+01, double %s)\n", scale, d_d);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = fmul double %s, %s\n", mul, v_d, scale);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call double @round(double %s)\n", rnd, mul);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = fdiv double %s, %s\n", res, rnd, scale);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call %%Value @make_float(double %s)\n", result_var, res);
                    handled_math = 1;
                } else if (
                    strcmp(fname, "sin") == 0 || strcmp(fname, "cos") == 0 ||
                    strcmp(fname, "asin") == 0 || strcmp(fname, "acos") == 0 ||
                    strcmp(fname, "log") == 0 || strcmp(fname, "exp") == 0 ||
                    strcmp(fname, "ceil") == 0 || strcmp(fname, "floor") == 0 ||
                    strcmp(fname, "sqrt") == 0 ||
                    strcmp(fname, "round") == 0
                ) {
                    if (arg_count != 1) {
                        codegen_error(node, "%s() requires 1 argument", fname);
                    }
                    char fval[32], bits[32], dval[32], mres[32];
                    snprintf(fval, sizeof(fval), "%%t%d", gen->temp_counter++);
                    snprintf(bits, sizeof(bits), "%%t%d", gen->temp_counter++);
                    snprintf(dval, sizeof(dval), "%%t%d", gen->temp_counter++);
                    snprintf(mres, sizeof(mres), "%%t%d", gen->temp_counter++);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call %%Value @to_float(%%Value %s)\n", fval, arg_temps[0]);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = extractvalue %%Value %s, 1\n", bits, fval);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = bitcast i64 %s to double\n", dval, bits);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call double @%s(double %s)\n", mres, fname, dval);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call %%Value @make_float(double %s)\n", result_var, mres);
                    handled_math = 1;
                } else if (strcmp(fname, "pow") == 0) {
                    if (arg_count != 2) {
                        codegen_error(node, "pow() requires 2 arguments");
                    }
                    char a_val[32], a_bits[32], a_d[32];
                    char b_val[32], b_bits[32], b_d[32];
                    char mres[32];
                    snprintf(a_val, sizeof(a_val), "%%t%d", gen->temp_counter++);
                    snprintf(a_bits, sizeof(a_bits), "%%t%d", gen->temp_counter++);
                    snprintf(a_d, sizeof(a_d), "%%t%d", gen->temp_counter++);
                    snprintf(b_val, sizeof(b_val), "%%t%d", gen->temp_counter++);
                    snprintf(b_bits, sizeof(b_bits), "%%t%d", gen->temp_counter++);
                    snprintf(b_d, sizeof(b_d), "%%t%d", gen->temp_counter++);
                    snprintf(mres, sizeof(mres), "%%t%d", gen->temp_counter++);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call %%Value @to_float(%%Value %s)\n", a_val, arg_temps[0]);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = extractvalue %%Value %s, 1\n", a_bits, a_val);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = bitcast i64 %s to double\n", a_d, a_bits);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call %%Value @to_float(%%Value %s)\n", b_val, arg_temps[1]);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = extractvalue %%Value %s, 1\n", b_bits, b_val);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = bitcast i64 %s to double\n", b_d, b_bits);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call double @pow(double %s, double %s)\n", mres, a_d, b_d);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call %%Value @make_float(double %s)\n", result_var, mres);
                    handled_math = 1;
                }

                if (handled_math) {
                    for (int i = 0; i < arg_count; i++) free(arg_temps[i]);
                    free(arg_temps);
                    break;
                }

                FuncInfo *fi = find_function(gen, fname);
                if (fi && arg_count != fi->arity) {
                    codegen_error(node, "Function '%s' expects %d args but got %d (codegen)", fname, fi->arity, arg_count);
                }

                // Map builtin function names to runtime function names
                const char *runtime_name = node->data.func_call.name;
                if (strcmp(node->data.func_call.name, "int") == 0) {
                    runtime_name = "to_int";
                } else if (strcmp(node->data.func_call.name, "float") == 0) {
                    runtime_name = "to_float";
                }
                else if (strcmp(runtime_name, "read") == 0) runtime_name = "file_read";
                else if (strcmp(runtime_name, "write") == 0) runtime_name = "file_write";
                else if (strcmp(runtime_name, "file_read") == 0) runtime_name = "file_read";
                else if (strcmp(runtime_name, "file_write") == 0) runtime_name = "file_write";
                else if (strcmp(runtime_name, "file_append") == 0) runtime_name = "file_append";
                else if (strcmp(runtime_name, "file_size") == 0) runtime_name = "file_size";
                else if (strcmp(runtime_name, "file_exist") == 0) runtime_name = "file_exist";
                else if (strcmp(runtime_name, "str_trim") == 0) runtime_name = "str_trim";
                else if (strcmp(runtime_name, "random") == 0) runtime_name = "math_random_val";

                // User function call
                char rnd_zero1[32] = {0}, rnd_zero2[32] = {0};
                if (strcmp(runtime_name, "math_random_val") == 0 && arg_count == 0) {
                    snprintf(rnd_zero1, sizeof(rnd_zero1), "%%t%d", gen->temp_counter++);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call %%Value @make_int(i64 0)\n", rnd_zero1);
                    snprintf(rnd_zero2, sizeof(rnd_zero2), "%%t%d", gen->temp_counter++);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call %%Value @make_int(i64 0)\n", rnd_zero2);
                }

                if (strcmp(runtime_name, "str_trim") == 0 && arg_count == 1) {
                    char defptr[32], defval[32];
                    snprintf(defptr, sizeof(defptr), "%%t%d", gen->temp_counter++);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = getelementptr inbounds [4 x i8], [4 x i8]* @.str_trim_ws, i64 0, i64 0\n", defptr);
                    snprintf(defval, sizeof(defval), "%%t%d", gen->temp_counter++);
                    emit_indent(gen);
                    fprintf(gen->out, "%s = call %%Value @make_string(i8* %s)\n", defval, defptr);
                    arg_temps = realloc(arg_temps, 2 * sizeof(char*));
                    arg_temps[1] = strdup(defval);
                    arg_count = 2;
                }

                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @%s(", result_var, runtime_name);
                if (strcmp(runtime_name, "math_random_val") == 0 && arg_count == 0) {
                    fprintf(gen->out, "%%Value %s, %%Value %s, i32 0", rnd_zero1, rnd_zero2);
                } else {
                    for (int i = 0; i < arg_count; i++) {
                        if (i > 0) fprintf(gen->out, ", ");
                        fprintf(gen->out, "%%Value %s", arg_temps[i]);
                    }
                    if (strcmp(runtime_name, "math_random_val") == 0) {
                        fprintf(gen->out, ", i32 %d", arg_count);
                    }
                }
                fprintf(gen->out, ")\n");
            }

            for (int i = 0; i < arg_count; i++) {
                free(arg_temps[i]);
            }
            free(arg_temps);
            break;
        }

        case NODE_METHOD_CALL: {
            char obj_temp[32];
            snprintf(obj_temp, sizeof(obj_temp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.method_call.object, obj_temp);

            int arg_count = 0;
            ASTNodeList *arg = node->data.method_call.arguments;
            while (arg != NULL) {
                arg_count++;
                arg = arg->next;
            }

            char args_alloca[32];
            snprintf(args_alloca, sizeof(args_alloca), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = alloca [%d x %%Value]\n", args_alloca, arg_count > 0 ? arg_count : 1);

            arg = node->data.method_call.arguments;
            for (int i = 0; i < arg_count; i++) {
                char arg_temp[32];
                snprintf(arg_temp, sizeof(arg_temp), "%%t%d", gen->temp_counter++);
                gen_expr(gen, arg->node, arg_temp);

                char arg_ptr[32];
                snprintf(arg_ptr, sizeof(arg_ptr), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = getelementptr [%d x %%Value], [%d x %%Value]* %s, i32 0, i32 %d\n",
                        arg_ptr, arg_count > 0 ? arg_count : 1, arg_count > 0 ? arg_count : 1, args_alloca, i);
                emit_indent(gen);
                fprintf(gen->out, "store %%Value %s, %%Value* %s\n", arg_temp, arg_ptr);

                arg = arg->next;
            }

            char args_base[32];
            snprintf(args_base, sizeof(args_base), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = getelementptr [%d x %%Value], [%d x %%Value]* %s, i32 0, i32 0\n",
                    args_base, arg_count > 0 ? arg_count : 1, arg_count > 0 ? arg_count : 1, args_alloca);

            const char *global_name = register_string_literal(gen, node->data.method_call.method);
            int len = strlen(node->data.method_call.method) + 1;
            char str_ptr[32];
            snprintf(str_ptr, sizeof(str_ptr), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                    str_ptr, len, len, global_name);

            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @method_call(%%Value %s, i8* %s, %%Value* %s, i32 %d)\n",
                    result_var, obj_temp, str_ptr, args_base, arg_count);
            break;
        }

        case NODE_NEW_EXPR: {
            VarMapping *m = find_var_mapping(gen, node->data.new_expr.class_name);
            char cls_temp[32];
            snprintf(cls_temp, sizeof(cls_temp), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            if (m && m->is_global) {
                fprintf(gen->out, "%s = load %%Value, %%Value* @%s\n", cls_temp, m->unique_name);
            } else if (m) {
                fprintf(gen->out, "%s = load %%Value, %%Value* %%%s\n", cls_temp, m->unique_name);
            }

            int arg_count = 0;
            ASTNodeList *arg = node->data.new_expr.arguments;
            while (arg != NULL) {
                arg_count++;
                arg = arg->next;
            }

            char args_alloca[32];
            snprintf(args_alloca, sizeof(args_alloca), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = alloca [%d x %%Value]\n", args_alloca, arg_count > 0 ? arg_count : 1);

            arg = node->data.new_expr.arguments;
            for (int i = 0; i < arg_count; i++) {
                char arg_temp[32];
                snprintf(arg_temp, sizeof(arg_temp), "%%t%d", gen->temp_counter++);
                gen_expr(gen, arg->node, arg_temp);

                char arg_ptr[32];
                snprintf(arg_ptr, sizeof(arg_ptr), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = getelementptr [%d x %%Value], [%d x %%Value]* %s, i32 0, i32 %d\n",
                        arg_ptr, arg_count > 0 ? arg_count : 1, arg_count > 0 ? arg_count : 1, args_alloca, i);
                emit_indent(gen);
                fprintf(gen->out, "store %%Value %s, %%Value* %s\n", arg_temp, arg_ptr);

                arg = arg->next;
            }

            char args_base[32];
            snprintf(args_base, sizeof(args_base), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = getelementptr [%d x %%Value], [%d x %%Value]* %s, i32 0, i32 0\n",
                    args_base, arg_count > 0 ? arg_count : 1, arg_count > 0 ? arg_count : 1, args_alloca);

            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @instantiate_class(%%Value %s, %%Value* %s, i32 %d)\n",
                    result_var, cls_temp, args_base, arg_count);
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
            VarMapping *m_current = find_var_mapping_current_scope(gen, node->data.var_decl.name);
            VarMapping *m = m_current ? m_current : find_var_mapping(gen, node->data.var_decl.name);
            if (m_current && m_current->declared) {
                codegen_error(node, "Redefinition of '%s' in the same scope (codegen)", node->data.var_decl.name);
            }
            // Evaluate initial value
            char val_temp[32];
            snprintf(val_temp, sizeof(val_temp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.var_decl.value, val_temp);

            if (!m_current) {
                int is_global_decl = (gen->scope_depth == 0);
                create_unique_var_name(gen, node->data.var_decl.name, is_global_decl);
                m = find_var_mapping(gen, node->data.var_decl.name);
                if (!m->is_global) {
                    emit_indent(gen);
                    fprintf(gen->out, "%%%s = alloca %%Value\n", m->unique_name);
                }
            }
            if (m) m->declared = 1;
            emit_indent(gen);
            if (m && m->is_global) {
                fprintf(gen->out, "store %%Value %s, %%Value* @%s\n", val_temp, m->unique_name);
            } else {
                fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n",
                        val_temp, m->unique_name);
            }
            break;
        }

        case NODE_MULTI_VAR_DECL: {
            /* Generate code for each declaration in the list */
            ASTNodeList *decl_list = node->data.multi_var_decl.declarations;
            while (decl_list != NULL) {
                gen_statement(gen, decl_list->node);
                decl_list = decl_list->next;
            }
            break;
        }

        case NODE_ASSIGNMENT: {
            // Evaluate value
            char val_temp[32];
            snprintf(val_temp, sizeof(val_temp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.assignment.value, val_temp);

            // Store to variable or array element
            if (node->data.assignment.target->type == NODE_IDENTIFIER) {
                VarMapping *m = find_var_mapping(gen, node->data.assignment.target->data.identifier.name);
                if (m == NULL) {
                    codegen_error(node, "Variable '%s' not declared in this scope (codegen)",
                                  node->data.assignment.target->data.identifier.name);
                }
                emit_indent(gen);
                if (m && m->is_global) {
                    fprintf(gen->out, "store %%Value %s, %%Value* @%s\n", val_temp, m->unique_name);
                } else if (m) {
                    fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n", val_temp, m->unique_name);
                }
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
            } else if (node->data.assignment.target->type == NODE_MEMBER_ACCESS) {
                char obj_temp[32];
                snprintf(obj_temp, sizeof(obj_temp), "%%t%d", gen->temp_counter++);
                gen_expr(gen, node->data.assignment.target->data.member_access.object, obj_temp);

                const char *global_name = register_string_literal(gen, node->data.assignment.target->data.member_access.member);
                int len = strlen(node->data.assignment.target->data.member_access.member) + 1;
                char str_ptr[32];
                snprintf(str_ptr, sizeof(str_ptr), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                        str_ptr, len, len, global_name);

                char result_temp[32];
                snprintf(result_temp, sizeof(result_temp), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @member_set(%%Value %s, i8* %s, %%Value %s)\n",
                        result_temp, obj_temp, str_ptr, val_temp);
            }
            break;
        }

        case NODE_CLASS_DEF: {
            VarMapping *m_current = find_var_mapping_current_scope(gen, node->data.class_def.name);
            if (m_current && m_current->declared) {
                codegen_error(node, "Redefinition of class '%s' in the same scope (codegen)", node->data.class_def.name);
            }
            if (!m_current) {
                int is_global_decl = (gen->scope_depth == 0);
                create_unique_var_name(gen, node->data.class_def.name, is_global_decl);
                m_current = find_var_mapping_current_scope(gen, node->data.class_def.name);
            }
            if (m_current) m_current->declared = 1;
            const char *unique_name = m_current ? m_current->unique_name : node->data.class_def.name;
            int is_global = (m_current && m_current->is_global);

            // Allocate storage for class value if needed
            if (!is_global) {
                emit_indent(gen);
                fprintf(gen->out, "%%%s = alloca %%Value\n", unique_name);
            }

            // Class name string
            const char *class_global = register_string_literal(gen, node->data.class_def.name);
            int name_len = strlen(node->data.class_def.name) + 1;
            char name_ptr[32];
            snprintf(name_ptr, sizeof(name_ptr), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                    name_ptr, name_len, name_len, class_global);

            char class_val[32];
            snprintf(class_val, sizeof(class_val), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @make_class(i8* %s)\n", class_val, name_ptr);
            emit_indent(gen);
            fprintf(gen->out, "store %%Value %s, %%Value* %s%s\n", class_val, is_global ? "@" : "%", unique_name);

            // Register fields
            ASTNodeList *member = node->data.class_def.members;
            while (member != NULL) {
                const char *field_name = member->node->data.var_decl.name;
                const char *field_global = register_string_literal(gen, field_name);
                int field_len = strlen(field_name) + 1;
                char field_ptr[32];
                snprintf(field_ptr, sizeof(field_ptr), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                        field_ptr, field_len, field_len, field_global);

                char cls_load[32];
                snprintf(cls_load, sizeof(cls_load), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = load %%Value, %%Value* %s%s\n", cls_load, is_global ? "@" : "%", unique_name);

                int is_private = field_name[0] == '_' ? 1 : 0;
                emit_indent(gen);
                fprintf(gen->out, "call void @class_add_field(%%Value %s, i8* %s, %%Value (%%Value)* @__field_init_%s_%s, i32 %d)\n",
                        cls_load, field_ptr, node->data.class_def.name, field_name, is_private);

                member = member->next;
            }

            // Register methods
            ASTNodeList *meth = node->data.class_def.methods;
            while (meth != NULL) {
                const char *method_name = meth->node->data.func_def.name;
                const char *method_global = register_string_literal(gen, method_name);
                int method_len = strlen(method_name) + 1;
                char method_ptr[32];
                snprintf(method_ptr, sizeof(method_ptr), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                        method_ptr, method_len, method_len, method_global);

                int arity = 0;
                ASTNodeList *p = meth->node->data.func_def.params;
                while (p != NULL) {
                    arity++;
                    p = p->next;
                }
                int is_private = method_name[0] == '_' ? 1 : 0;

                char cls_load2[32];
                snprintf(cls_load2, sizeof(cls_load2), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = load %%Value, %%Value* %s%s\n", cls_load2, is_global ? "@" : "%", unique_name);

                emit_indent(gen);
                fprintf(gen->out, "call void @class_add_method(%%Value %s, i8* %s, %%Value (%%Value, %%Value*, i32)* @%s__%s, i32 %d, i32 %d)\n",
                        cls_load2, method_ptr, node->data.class_def.name, method_name, arity, is_private);

                meth = meth->next;
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
            fprintf(gen->out, "%s = call i32 @__is_truthy_ir(%%Value %s)\n", truthy_temp, cond_temp);

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
            int saved_then_depth = 0;
            VarMapping *then_scope = push_scope(gen, &saved_then_depth);
            ASTNodeList *stmt = node->data.if_stmt.then_block;
            while (stmt != NULL) {
                gen_statement(gen, stmt->node);
                stmt = stmt->next;
            }
            pop_scope(gen, then_scope, saved_then_depth);
            emit_indent(gen);
            fprintf(gen->out, "br label %%%s\n", end_label);
            gen->indent_level--;

            // Else block
            if (node->data.if_stmt.else_block) {
                fprintf(gen->out, "\n%s:\n", else_label);
                gen->indent_level++;
                int saved_else_depth = 0;
                VarMapping *else_scope = push_scope(gen, &saved_else_depth);
                stmt = node->data.if_stmt.else_block;
                while (stmt != NULL) {
                    gen_statement(gen, stmt->node);
                    stmt = stmt->next;
                }
                pop_scope(gen, else_scope, saved_else_depth);
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

            char *prev_break = gen->break_label;
            char *prev_continue = gen->continue_label;
            gen->break_label = strdup(end_label);
            gen->continue_label = strdup(cond_label);

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
            fprintf(gen->out, "%s = call i32 @__is_truthy_ir(%%Value %s)\n", truthy_temp, cond_temp);

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
            int saved_body_depth = 0;
            VarMapping *body_scope = push_scope(gen, &saved_body_depth);
            ASTNodeList *stmt = node->data.while_stmt.body;
            while (stmt != NULL) {
                gen_statement(gen, stmt->node);
                stmt = stmt->next;
            }
            pop_scope(gen, body_scope, saved_body_depth);
            emit_indent(gen);
            fprintf(gen->out, "br label %%%s\n", cond_label);
            gen->indent_level--;

            fprintf(gen->out, "\n%s:\n", end_label);
            gen->break_label = prev_break;
            gen->continue_label = prev_continue;
            break;
        }

        case NODE_BREAK: {
            if (!gen->break_label) {
                codegen_error(node, "break used outside of loop (codegen)");
            }
            emit_indent(gen);
            fprintf(gen->out, "br label %%%s\n", gen->break_label);
            break;
        }

        case NODE_CONTINUE: {
            if (!gen->continue_label) {
                codegen_error(node, "continue used outside of loop (codegen)");
            }
            emit_indent(gen);
            fprintf(gen->out, "br label %%%s\n", gen->continue_label);
            break;
        }

        case NODE_FOR_STMT: {
            int saved_for_depth = 0;
            VarMapping *for_scope = push_scope(gen, &saved_for_depth);

            char start_val[32], end_val[32];
            char start_int[32], end_int[32];
            char start_i64[32], end_i64[32];
            snprintf(start_val, sizeof(start_val), "%%t%d", gen->temp_counter++);
            snprintf(end_val, sizeof(end_val), "%%t%d", gen->temp_counter++);
            snprintf(start_int, sizeof(start_int), "%%t%d", gen->temp_counter++);
            snprintf(end_int, sizeof(end_int), "%%t%d", gen->temp_counter++);
            snprintf(start_i64, sizeof(start_i64), "%%t%d", gen->temp_counter++);
            snprintf(end_i64, sizeof(end_i64), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.for_stmt.start, start_val);
            gen_expr(gen, node->data.for_stmt.end, end_val);
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @to_int(%%Value %s)\n", start_int, start_val);
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @to_int(%%Value %s)\n", end_int, end_val);
            emit_indent(gen);
            fprintf(gen->out, "%s = extractvalue %%Value %s, 1\n", start_i64, start_int);
            emit_indent(gen);
            fprintf(gen->out, "%s = extractvalue %%Value %s, 1\n", end_i64, end_int);

            char step_pos[32], step_val[32];
            snprintf(step_pos, sizeof(step_pos), "%%t%d", gen->temp_counter++);
            snprintf(step_val, sizeof(step_val), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = icmp sle i64 %s, %s\n", step_pos, start_i64, end_i64);
            emit_indent(gen);
            fprintf(gen->out, "%s = select i1 %s, i64 1, i64 -1\n", step_val, step_pos);

            const char *idx_unique = create_unique_var_name(gen, node->data.for_stmt.index_var, 0);
            VarMapping *idx_map = find_var_mapping_current_scope(gen, node->data.for_stmt.index_var);
            if (idx_map) idx_map->declared = 1;
            emit_indent(gen);
            fprintf(gen->out, "%%%s = alloca %%Value\n", idx_unique);
            char init_val[32];
            snprintf(init_val, sizeof(init_val), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @make_int(i64 %s)\n", init_val, start_i64);
            emit_indent(gen);
            fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n", init_val, idx_unique);

            char cond_label[32], body_label[32], incr_label[32], end_label[32];
            snprintf(cond_label, sizeof(cond_label), "label%d", gen->label_counter++);
            snprintf(body_label, sizeof(body_label), "label%d", gen->label_counter++);
            snprintf(incr_label, sizeof(incr_label), "label%d", gen->label_counter++);
            snprintf(end_label, sizeof(end_label), "label%d", gen->label_counter++);

            char *prev_break = gen->break_label;
            char *prev_continue = gen->continue_label;
            gen->break_label = strdup(end_label);
            gen->continue_label = strdup(incr_label);

            emit_indent(gen);
            fprintf(gen->out, "br label %%%s\n", cond_label);

            // Condition
            fprintf(gen->out, "\n%s:\n", cond_label);
            gen->indent_level++;
            char idx_load[32], idx_i64[32], cmp_le[32], cmp_ge[32], cmp_sel[32];
            snprintf(idx_load, sizeof(idx_load), "%%t%d", gen->temp_counter++);
            snprintf(idx_i64, sizeof(idx_i64), "%%t%d", gen->temp_counter++);
            snprintf(cmp_le, sizeof(cmp_le), "%%t%d", gen->temp_counter++);
            snprintf(cmp_ge, sizeof(cmp_ge), "%%t%d", gen->temp_counter++);
            snprintf(cmp_sel, sizeof(cmp_sel), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = load %%Value, %%Value* %%%s\n", idx_load, idx_unique);
            emit_indent(gen);
            fprintf(gen->out, "%s = extractvalue %%Value %s, 1\n", idx_i64, idx_load);
            emit_indent(gen);
            fprintf(gen->out, "%s = icmp sle i64 %s, %s\n", cmp_le, idx_i64, end_i64);
            emit_indent(gen);
            fprintf(gen->out, "%s = icmp sge i64 %s, %s\n", cmp_ge, idx_i64, end_i64);
            emit_indent(gen);
            fprintf(gen->out, "%s = select i1 %s, i1 %s, i1 %s\n", cmp_sel, step_pos, cmp_le, cmp_ge);
            emit_indent(gen);
            fprintf(gen->out, "br i1 %s, label %%%s, label %%%s\n", cmp_sel, body_label, end_label);
            gen->indent_level--;

            // Body
            fprintf(gen->out, "\n%s:\n", body_label);
            gen->indent_level++;
            {
                int saved_body_depth = 0;
                VarMapping *body_scope = push_scope(gen, &saved_body_depth);
                ASTNodeList *stmt = node->data.for_stmt.body;
                while (stmt != NULL) {
                    gen_statement(gen, stmt->node);
                    stmt = stmt->next;
                }
                pop_scope(gen, body_scope, saved_body_depth);
            }
            emit_indent(gen);
            fprintf(gen->out, "br label %%%s\n", incr_label);
            gen->indent_level--;

            // Increment
            fprintf(gen->out, "\n%s:\n", incr_label);
            gen->indent_level++;
            char idx_load2[32], idx_i64_2[32], next_i64[32], next_val[32];
            snprintf(idx_load2, sizeof(idx_load2), "%%t%d", gen->temp_counter++);
            snprintf(idx_i64_2, sizeof(idx_i64_2), "%%t%d", gen->temp_counter++);
            snprintf(next_i64, sizeof(next_i64), "%%t%d", gen->temp_counter++);
            snprintf(next_val, sizeof(next_val), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = load %%Value, %%Value* %%%s\n", idx_load2, idx_unique);
            emit_indent(gen);
            fprintf(gen->out, "%s = extractvalue %%Value %s, 1\n", idx_i64_2, idx_load2);
            emit_indent(gen);
            fprintf(gen->out, "%s = add i64 %s, %s\n", next_i64, idx_i64_2, step_val);
            emit_indent(gen);
            fprintf(gen->out, "%s = call %%Value @make_int(i64 %s)\n", next_val, next_i64);
            emit_indent(gen);
            fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n", next_val, idx_unique);
            emit_indent(gen);
            fprintf(gen->out, "br label %%%s\n", cond_label);
            gen->indent_level--;

            fprintf(gen->out, "\n%s:\n", end_label);
            pop_scope(gen, for_scope, saved_for_depth);
            gen->break_label = prev_break;
            gen->continue_label = prev_continue;
            break;
        }

        case NODE_FOREACH_STMT: {
            int saved_foreach_depth = 0;
            VarMapping *saved_foreach_scope = push_scope(gen, &saved_foreach_depth);
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
                char *prev_break = gen->break_label;
                char *prev_continue = gen->continue_label;
                char len_temp[32], i_ptr[32];
                snprintf(len_temp, sizeof(len_temp), "%%t%d", gen->temp_counter++);
                snprintf(i_ptr, sizeof(i_ptr), "%%t%d", gen->temp_counter++);

                // Get array length
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @len(%%Value %s)\n", len_temp, collection_temp);

                // Allocate loop variables with unique names (these are new declarations)
                const char *key_var = create_unique_var_name(gen, node->data.foreach_stmt.key_var, 0);
                const char *value_var = create_unique_var_name(gen, node->data.foreach_stmt.value_var, 0);
                VarMapping *kvm = find_var_mapping_current_scope(gen, node->data.foreach_stmt.key_var);
                VarMapping *vvm = find_var_mapping_current_scope(gen, node->data.foreach_stmt.value_var);
                if (kvm) kvm->declared = 1;
                if (vvm) vvm->declared = 1;

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

                gen->break_label = strdup(end_label);
                gen->continue_label = strdup(loop_incr);

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
                gen->break_label = prev_break;
                gen->continue_label = prev_continue;
            }
            gen->indent_level--;

            // Dict foreach
            fprintf(gen->out, "\n%s:\n", dict_label);
            gen->indent_level++;
            {
                char *prev_break = gen->break_label;
                char *prev_continue = gen->continue_label;
                // keys_arr = keys(collection)
                char keys_val[32];
                snprintf(keys_val, sizeof(keys_val), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @keys(%%Value %s)\n", keys_val, collection_temp);

                // len(keys_arr)
                char len_temp[32];
                snprintf(len_temp, sizeof(len_temp), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @len(%%Value %s)\n", len_temp, keys_val);

                const char *key_var = create_unique_var_name(gen, node->data.foreach_stmt.key_var, 0);
                const char *value_var = create_unique_var_name(gen, node->data.foreach_stmt.value_var, 0);
                VarMapping *kvm = find_var_mapping_current_scope(gen, node->data.foreach_stmt.key_var);
                VarMapping *vvm = find_var_mapping_current_scope(gen, node->data.foreach_stmt.value_var);
                if (kvm) kvm->declared = 1;
                if (vvm) vvm->declared = 1;
                emit_indent(gen);
                fprintf(gen->out, "%%%s = alloca %%Value\n", key_var);
                emit_indent(gen);
                fprintf(gen->out, "%%%s = alloca %%Value\n", value_var);

                char i_ptr[32];
                snprintf(i_ptr, sizeof(i_ptr), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = alloca i64\n", i_ptr);
                emit_indent(gen);
                fprintf(gen->out, "store i64 0, i64* %s\n", i_ptr);

                char loop_cond[32], loop_body[32], loop_incr[32], loop_end[32];
                snprintf(loop_cond, sizeof(loop_cond), "label%d", gen->label_counter++);
                snprintf(loop_body, sizeof(loop_body), "label%d", gen->label_counter++);
                snprintf(loop_incr, sizeof(loop_incr), "label%d", gen->label_counter++);
                snprintf(loop_end, sizeof(loop_end), "label%d", gen->label_counter++);

                gen->break_label = strdup(end_label);
                gen->continue_label = strdup(loop_incr);

                emit_indent(gen);
                fprintf(gen->out, "br label %%%s\n", loop_cond);

                // condition
                fprintf(gen->out, "\n%s:\n", loop_cond);
                gen->indent_level++;
                char i_val[32], len_val[32], cmp_res[32];
                snprintf(i_val, sizeof(i_val), "%%t%d", gen->temp_counter++);
                snprintf(len_val, sizeof(len_val), "%%t%d", gen->temp_counter++);
                snprintf(cmp_res, sizeof(cmp_res), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = load i64, i64* %s\n", i_val, i_ptr);
                emit_indent(gen);
                fprintf(gen->out, "%s = extractvalue %%Value %s, 1\n", len_val, len_temp);
                emit_indent(gen);
                fprintf(gen->out, "%s = icmp slt i64 %s, %s\n", cmp_res, i_val, len_val);
                emit_indent(gen);
                fprintf(gen->out, "br i1 %s, label %%%s, label %%%s\n", cmp_res, loop_body, loop_end);
                gen->indent_level--;

                // body
                fprintf(gen->out, "\n%s:\n", loop_body);
                gen->indent_level++;
                char key_idx_val[32], key_elem[32], key_idx_value[32], dict_val_temp[32];
                snprintf(key_idx_val, sizeof(key_idx_val), "%%t%d", gen->temp_counter++);
                snprintf(key_idx_value, sizeof(key_idx_value), "%%t%d", gen->temp_counter++);
                snprintf(key_elem, sizeof(key_elem), "%%t%d", gen->temp_counter++);
                snprintf(dict_val_temp, sizeof(dict_val_temp), "%%t%d", gen->temp_counter++);

                emit_indent(gen);
                fprintf(gen->out, "%s = load i64, i64* %s\n", key_idx_val, i_ptr);
                emit_indent(gen);
                fprintf(gen->out, "%s = insertvalue %%Value { i32 0, i64 0 }, i64 %s, 1\n", key_idx_value, key_idx_val);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @index_get(%%Value %s, %%Value %s)\n", key_elem, keys_val, key_idx_value);
                emit_indent(gen);
                fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n", key_elem, key_var);

                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @dict_get(%%Value %s, %%Value %s)\n", dict_val_temp, collection_temp, key_elem);
                emit_indent(gen);
                fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n", dict_val_temp, value_var);

                ASTNodeList *stmt = node->data.foreach_stmt.body;
                while (stmt != NULL) {
                    gen_statement(gen, stmt->node);
                    stmt = stmt->next;
                }
                emit_indent(gen);
                fprintf(gen->out, "br label %%%s\n", loop_incr);
                gen->indent_level--;

                // incr
                fprintf(gen->out, "\n%s:\n", loop_incr);
                gen->indent_level++;
                char i_curr[32], i_next[32];
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
                gen->break_label = prev_break;
                gen->continue_label = prev_continue;
            }
            gen->indent_level--;

            fprintf(gen->out, "\n%s:\n", end_label);
            pop_scope(gen, saved_foreach_scope, saved_foreach_depth);
            break;
        }

        case NODE_TRY_CATCH: {
            char try_buf[32], try_res[32];
            snprintf(try_buf, sizeof(try_buf), "%%t%d", gen->temp_counter++);
            snprintf(try_res, sizeof(try_res), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = call i8* @__try_push_buf()\n", try_buf);
            emit_indent(gen);
            fprintf(gen->out, "%s = call i32 @setjmp(i8* %s)\n", try_res, try_buf);

            char try_label[32], catch_label[32], end_label[32];
            snprintf(try_label, sizeof(try_label), "label%d", gen->label_counter++);
            snprintf(catch_label, sizeof(catch_label), "label%d", gen->label_counter++);
            snprintf(end_label, sizeof(end_label), "label%d", gen->label_counter++);

            char cmp_res[32];
            snprintf(cmp_res, sizeof(cmp_res), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = icmp eq i32 %s, 0\n", cmp_res, try_res);
            emit_indent(gen);
            fprintf(gen->out, "br i1 %s, label %%%s, label %%%s\n", cmp_res, try_label, catch_label);

            // try block
            fprintf(gen->out, "\n%s:\n", try_label);
            gen->indent_level++;
            {
                ASTNodeList *stmt = node->data.try_catch.try_block;
                while (stmt != NULL) {
                    gen_statement(gen, stmt->node);
                    stmt = stmt->next;
                }
                emit_indent(gen);
                fprintf(gen->out, "call void @__try_pop()\n");
                emit_indent(gen);
                fprintf(gen->out, "br label %%%s\n", end_label);
            }
            gen->indent_level--;

            // catch block
            fprintf(gen->out, "\n%s:\n", catch_label);
            gen->indent_level++;
            {
                const char *catch_var = create_unique_var_name(gen, node->data.try_catch.catch_var, 0);
                VarMapping *cm = find_var_mapping_current_scope(gen, node->data.try_catch.catch_var);
                if (cm) cm->declared = 1;
                emit_indent(gen);
                fprintf(gen->out, "%%%s = alloca %%Value\n", catch_var);
                char exc_tmp[32];
                snprintf(exc_tmp, sizeof(exc_tmp), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @__get_exception()\n", exc_tmp);
                // Build prefixed message: "[caught in file:line]" + exception
                const char *pref_str = register_string_literal(gen, "[caught in ");
                int pref_len = strlen("[caught in ") + 1;
                char pref_ptr[32];
                snprintf(pref_ptr, sizeof(pref_ptr), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                        pref_ptr, pref_len, pref_len, pref_str);
                char pref_val[32];
                snprintf(pref_val, sizeof(pref_val), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @make_string(i8* %s)\n", pref_val, pref_ptr);

                const char *file_str = register_string_literal(gen, node->file ? node->file : "<input>");
                int file_len = strlen(node->file ? node->file : "<input>") + 1;
                char file_ptr[32], file_val[32];
                snprintf(file_ptr, sizeof(file_ptr), "%%t%d", gen->temp_counter++);
                snprintf(file_val, sizeof(file_val), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                        file_ptr, file_len, file_len, file_str);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @make_string(i8* %s)\n", file_val, file_ptr);

                // prefix + file
                char pref_file[32];
                snprintf(pref_file, sizeof(pref_file), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @binary_op(%%Value %s, i32 0, %%Value %s)\n", pref_file, pref_val, file_val);

                // add colon/line and closing bracket
                char line_buf[64];
                snprintf(line_buf, sizeof(line_buf), ":%d] ", node->line);
                const char *line_str = register_string_literal(gen, line_buf);
                int line_len = strlen(line_buf) + 1;
                char line_ptr[32], line_val[32];
                snprintf(line_ptr, sizeof(line_ptr), "%%t%d", gen->temp_counter++);
                snprintf(line_val, sizeof(line_val), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                        line_ptr, line_len, line_len, line_str);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @make_string(i8* %s)\n", line_val, line_ptr);

                char prefix_full[32];
                snprintf(prefix_full, sizeof(prefix_full), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @binary_op(%%Value %s, i32 0, %%Value %s)\n", prefix_full, pref_file, line_val);

                char combined[32];
                snprintf(combined, sizeof(combined), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @binary_op(%%Value %s, i32 0, %%Value %s)\n", combined, prefix_full, exc_tmp);

                emit_indent(gen);
                fprintf(gen->out, "store %%Value %s, %%Value* %%%s\n", combined, catch_var);
                emit_indent(gen);
                fprintf(gen->out, "call void @__try_pop()\n");

                ASTNodeList *stmt = node->data.try_catch.catch_block;
                while (stmt != NULL) {
                    gen_statement(gen, stmt->node);
                    stmt = stmt->next;
                }
                emit_indent(gen);
                fprintf(gen->out, "br label %%%s\n", end_label);
            }
            gen->indent_level--;

            fprintf(gen->out, "\n%s:\n", end_label);
            break;
        }

        case NODE_RAISE: {
            char msg_tmp[32];
            snprintf(msg_tmp, sizeof(msg_tmp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.raise_stmt.expr, msg_tmp);
            const char *file_global = register_string_literal(gen, node->file ? node->file : "<input>");
            int flen = strlen(node->file ? node->file : "<input>") + 1;
            char file_ptr[32];
            snprintf(file_ptr, sizeof(file_ptr), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                    file_ptr, flen, flen, file_global);
            emit_indent(gen);
            fprintf(gen->out, "call void @__raise(%%Value %s, i32 %d, i8* %s)\n", msg_tmp, node->line, file_ptr);
            break;
        }

        case NODE_ASSERT: {
            char cond_tmp[32];
            snprintf(cond_tmp, sizeof(cond_tmp), "%%t%d", gen->temp_counter++);
            gen_expr(gen, node->data.assert_stmt.expr, cond_tmp);
            char truthy[32], cmp[32];
            snprintf(truthy, sizeof(truthy), "%%t%d", gen->temp_counter++);
            snprintf(cmp, sizeof(cmp), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = call i32 @__is_truthy_ir(%%Value %s)\n", truthy, cond_tmp);
            emit_indent(gen);
            fprintf(gen->out, "%s = icmp eq i32 %s, 0\n", cmp, truthy);

            char ok_label[32], fail_label[32], end_label[32];
            snprintf(ok_label, sizeof(ok_label), "label%d", gen->label_counter++);
            snprintf(fail_label, sizeof(fail_label), "label%d", gen->label_counter++);
            snprintf(end_label, sizeof(end_label), "label%d", gen->label_counter++);

            emit_indent(gen);
            fprintf(gen->out, "br i1 %s, label %%%s, label %%%s\n", cmp, fail_label, ok_label);

            // fail
            fprintf(gen->out, "\n%s:\n", fail_label);
            gen->indent_level++;
            char msg_tmp[32];
            snprintf(msg_tmp, sizeof(msg_tmp), "%%t%d", gen->temp_counter++);
            if (node->data.assert_stmt.msg) {
                gen_expr(gen, node->data.assert_stmt.msg, msg_tmp);
            } else {
                const char *def_str = register_string_literal(gen, "Assertion failed");
                int dlen = strlen("Assertion failed") + 1;
                char def_ptr[32];
                snprintf(def_ptr, sizeof(def_ptr), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                        def_ptr, dlen, dlen, def_str);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @make_string(i8* %s)\n", msg_tmp, def_ptr);
            }
            const char *file_global = register_string_literal(gen, node->file ? node->file : "<input>");
            int flen = strlen(node->file ? node->file : "<input>") + 1;
            char file_ptr[32];
            snprintf(file_ptr, sizeof(file_ptr), "%%t%d", gen->temp_counter++);
            emit_indent(gen);
            fprintf(gen->out, "%s = getelementptr inbounds [%d x i8], [%d x i8]* %s, i64 0, i64 0\n",
                    file_ptr, flen, flen, file_global);
            emit_indent(gen);
            fprintf(gen->out, "call void @__raise(%%Value %s, i32 %d, i8* %s)\n", msg_tmp, node->line, file_ptr);
            emit_indent(gen);
            fprintf(gen->out, "br label %%%s\n", end_label);
            gen->indent_level--;

            // ok
            fprintf(gen->out, "\n%s:\n", ok_label);
            gen->indent_level++;
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
                char null_temp[32];
                snprintf(null_temp, sizeof(null_temp), "%%t%d", gen->temp_counter++);
                emit_indent(gen);
                fprintf(gen->out, "%s = call %%Value @make_null()\n", null_temp);
                emit_indent(gen);
                fprintf(gen->out, "ret %%Value %s\n", null_temp);
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
    // Register file path strings for runtime context
    s = root->data.program.statements;
    while (s != NULL) {
        register_file_strings_stmt(gen, s->node);
        s = s->next;
    }

    // Pre-pass: register function signatures for arity checks
    s = root->data.program.statements;
    while (s != NULL) {
        register_functions_stmt(gen, s->node);
        s = s->next;
    }

    // Emit string literals
    fprintf(gen->out, "; String literals\n");
    emit_string_literals(gen);
    fprintf(gen->out, "\n");

    // Emit declarations
    emit_runtime_decls(gen);

    // Pre-register global variable mappings so functions can reference them
    ASTNodeList *stmt = root->data.program.statements;
    preregister_globals_in_list(gen, stmt, 1);

    // Emit runtime implementation
    emit_runtime_impl(gen);

    // Emit global variable storage for top-level vars
    fprintf(gen->out, "; Global variable storage\n");
    VarMapping *gm = gen->var_mappings;
    while (gm != NULL) {
        if (gm->is_global) {
            fprintf(gen->out, "@%s = global %%Value { i32 0, i64 0 }\n", gm->unique_name);
        }
        gm = gm->next;
    }
    fprintf(gen->out, "\n");

    // Generate user function implementations
    fprintf(gen->out, "; ===== User Function Implementations =====\n\n");
    stmt = root->data.program.statements;
    while (stmt != NULL) {
        if (stmt->node->type == NODE_FUNC_DEF) {
            int saved_depth = 0;
            VarMapping *saved_scope = push_scope(gen, &saved_depth);
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

            // Register parameters in current scope
            param = stmt->node->data.func_def.params;
            while (param != NULL) {
                const char *pname = param->node->data.identifier.name;
                create_unique_var_name(gen, pname, 0);
                VarMapping *pm = find_var_mapping_current_scope(gen, pname);
                if (pm) pm->declared = 1;
                param = param->next;
            }

            // Allocate and initialize parameters
            param = stmt->node->data.func_def.params;
            while (param != NULL) {
                VarMapping *pm = find_var_mapping(gen, param->node->data.identifier.name);
                const char *uname = pm ? pm->unique_name : param->node->data.identifier.name;
                emit_indent(gen);
                fprintf(gen->out, "%%%s = alloca %%Value\n", uname);
                emit_indent(gen);
                fprintf(gen->out, "store %%Value %%param_%s, %%Value* %%%s\n",
                        param->node->data.identifier.name, uname);
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
            pop_scope(gen, saved_scope, saved_depth);
        } else if (stmt->node->type == NODE_CLASS_DEF) {
            // Field init functions
            ASTNodeList *member = stmt->node->data.class_def.members;
            while (member != NULL) {
                gen_field_init_function(gen, stmt->node->data.class_def.name, member->node);
                member = member->next;
            }
            // Method functions
            ASTNodeList *meth = stmt->node->data.class_def.methods;
            while (meth != NULL) {
                gen_method_function(gen, stmt->node->data.class_def.name, meth->node);
                meth = meth->next;
            }
        }
        stmt = stmt->next;
    }

    // Generate main function
    fprintf(gen->out, "; ===== Main Function =====\n\n");
    fprintf(gen->out, "define i32 @main(i32 %%argc, i8** %%argv) {\n");
    gen->indent_level = 1;

    // Initialize GC
    emit_indent(gen);
    fprintf(gen->out, "call void @gc_init()\n");

    // Set stack bottom for conservative scanning
    emit_indent(gen);
    fprintf(gen->out, "%%stack_anchor = alloca i8\n");
    emit_indent(gen);
    fprintf(gen->out, "%%stack_bottom_ptr = bitcast i8* %%stack_anchor to i8*\n");
    emit_indent(gen);
    fprintf(gen->out, "call void @gc_set_stack_bottom(i8* %%stack_bottom_ptr)\n\n");

    // Call set_cmd_args to store command line arguments
    emit_indent(gen);
    fprintf(gen->out, "call void @set_cmd_args(i32 %%argc, i8** %%argv)\n\n");

    // Register global variables as GC roots
    VarMapping *global_var = gen->var_mappings;
    while (global_var != NULL) {
        if (global_var->is_global) {
            emit_indent(gen);
            fprintf(gen->out, "call void @gc_push_root(%%Value* @%s)\n", global_var->unique_name);
        }
        global_var = global_var->next;
    }
    if (gen->var_mappings) {
        fprintf(gen->out, "\n");
    }

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
