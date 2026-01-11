#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <regex.h>
#include "interpreter.h"
#include "ast.h"

#define HASH_SIZE 128

static jmp_buf break_jmp;
static jmp_buf continue_jmp;
static int has_returned;
static Value *return_value;

static Environment *global_env;
static Environment *current_env;

/* Forward declarations */
static Value *eval_expression(ASTNode *node);
static void eval_statement(ASTNode *node);
static void setup_builtins();

/* Hash function */
static unsigned int hash(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % HASH_SIZE;
}

/* Value functions */
Value *create_int_value(int val) {
    Value *v = malloc(sizeof(Value));
    v->type = VAL_INT;
    v->data.int_val = val;
    return v;
}

Value *create_float_value(double val) {
    Value *v = malloc(sizeof(Value));
    v->type = VAL_FLOAT;
    v->data.float_val = val;
    return v;
}

Value *create_string_value(char *val) {
    Value *v = malloc(sizeof(Value));
    v->type = VAL_STRING;
    v->data.string_val = strdup(val);
    return v;
}

Value *create_bool_value(int val) {
    Value *v = malloc(sizeof(Value));
    v->type = VAL_BOOL;
    v->data.bool_val = val;
    return v;
}

Value *create_array_value() {
    Value *v = malloc(sizeof(Value));
    v->type = VAL_ARRAY;
    v->data.array_val = malloc(sizeof(Array));
    v->data.array_val->elements = NULL;
    v->data.array_val->size = 0;
    v->data.array_val->capacity = 0;
    return v;
}

Value *create_dict_value() {
    Value *v = malloc(sizeof(Value));
    v->type = VAL_DICT;
    v->data.dict_val = malloc(sizeof(Dict));
    v->data.dict_val->buckets = calloc(HASH_SIZE, sizeof(DictEntry*));
    v->data.dict_val->size = 0;
    return v;
}

Value *create_func_value(char *name, ASTNodeList *params, ASTNodeList *body, Environment *env) {
    Value *v = malloc(sizeof(Value));
    v->type = VAL_FUNC;
    v->data.func_val = malloc(sizeof(Function));
    v->data.func_val->name = strdup(name);
    v->data.func_val->params = params;
    v->data.func_val->body = body;
    v->data.func_val->env = env;
    return v;
}

Value *create_builtin_value(BuiltinFunc func) {
    Value *v = malloc(sizeof(Value));
    v->type = VAL_BUILTIN;
    v->data.builtin_val = func;
    return v;
}

Value *create_null_value() {
    Value *v = malloc(sizeof(Value));
    v->type = VAL_NULL;
    return v;
}

/* Array functions */
void array_append(Array *arr, Value *val) {
    if (arr->size >= arr->capacity) {
        arr->capacity = arr->capacity == 0 ? 8 : arr->capacity * 2;
        arr->elements = realloc(arr->elements, arr->capacity * sizeof(Value*));
    }
    arr->elements[arr->size++] = val;
}

Value *array_get(Array *arr, int index) {
    if (index < 0 || index >= arr->size) {
        fprintf(stderr, "Array index out of bounds: %d\n", index);
        exit(1);
    }
    return arr->elements[index];
}

void array_set(Array *arr, int index, Value *val) {
    if (index < 0 || index >= arr->size) {
        fprintf(stderr, "Array index out of bounds: %d\n", index);
        exit(1);
    }
    arr->elements[index] = val;
}

/* Dict functions */
void dict_set(Dict *dict, char *key, Value *val) {
    unsigned int idx = hash(key);
    DictEntry *entry = dict->buckets[idx];

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = val;
            return;
        }
        entry = entry->next;
    }

    entry = malloc(sizeof(DictEntry));
    entry->key = strdup(key);
    entry->value = val;
    entry->next = dict->buckets[idx];
    dict->buckets[idx] = entry;
    dict->size++;
}

Value *dict_get(Dict *dict, char *key) {
    unsigned int idx = hash(key);
    DictEntry *entry = dict->buckets[idx];

    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    fprintf(stderr, "Dictionary key not found: %s\n", key);
    exit(1);
}

char **dict_keys(Dict *dict, int *count) {
    char **keys = malloc(dict->size * sizeof(char*));
    int idx = 0;

    for (int i = 0; i < HASH_SIZE; i++) {
        DictEntry *entry = dict->buckets[i];
        while (entry != NULL) {
            keys[idx++] = entry->key;
            entry = entry->next;
        }
    }

    *count = dict->size;
    return keys;
}

/* Environment functions */
Environment *create_environment(Environment *parent) {
    Environment *env = malloc(sizeof(Environment));
    env->buckets = calloc(HASH_SIZE, sizeof(EnvEntry*));
    env->size = 0;
    env->parent = parent;
    return env;
}

void env_define(Environment *env, char *name, Value *val) {
    unsigned int idx = hash(name);
    EnvEntry *entry = malloc(sizeof(EnvEntry));
    entry->name = strdup(name);
    entry->value = val;
    entry->next = env->buckets[idx];
    env->buckets[idx] = entry;
    env->size++;
}

Value *env_get(Environment *env, char *name) {
    unsigned int idx = hash(name);
    EnvEntry *entry = env->buckets[idx];

    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }

    if (env->parent != NULL) {
        return env_get(env->parent, name);
    }

    fprintf(stderr, "Undefined variable: %s\n", name);
    exit(1);
}

void env_set(Environment *env, char *name, Value *val) {
    unsigned int idx = hash(name);
    EnvEntry *entry = env->buckets[idx];

    while (entry != NULL) {
        if (strcmp(entry->name, name) == 0) {
            entry->value = val;
            return;
        }
        entry = entry->next;
    }

    if (env->parent != NULL) {
        env_set(env->parent, name, val);
        return;
    }

    fprintf(stderr, "Undefined variable: %s\n", name);
    exit(1);
}

/* Built-in functions */
static void print_value_recursive(Value *v) {
    switch (v->type) {
        case VAL_INT:
            printf("%d", v->data.int_val);
            break;
        case VAL_FLOAT:
            printf("%g", v->data.float_val);
            break;
        case VAL_STRING:
            printf("\"%s\"", v->data.string_val);
            break;
        case VAL_BOOL:
            printf("%s", v->data.bool_val ? "True" : "False");
            break;
        case VAL_NULL:
            printf("None");
            break;
        case VAL_ARRAY: {
            printf("[");
            Array *arr = v->data.array_val;
            for (int j = 0; j < arr->size; j++) {
                print_value_recursive(arr->elements[j]);
                if (j < arr->size - 1) printf(", ");
            }
            printf("]");
            break;
        }
        case VAL_DICT: {
            printf("{");
            Dict *dict = v->data.dict_val;
            int count = 0;
            for (int j = 0; j < HASH_SIZE; j++) {
                DictEntry *entry = dict->buckets[j];
                while (entry != NULL) {
                    if (count > 0) printf(", ");
                    printf("\"%s\": ", entry->key);
                    print_value_recursive(entry->value);
                    entry = entry->next;
                    count++;
                }
            }
            printf("}");
            break;
        }
        default:
            printf("<object>");
    }
}

static Value *builtin_print(Value **args, int arg_count) {
    for (int i = 0; i < arg_count; i++) {
        Value *v = args[i];
        // For non-string types, print without quotes
        if (v->type == VAL_STRING) {
            printf("%s", v->data.string_val);
        } else if (v->type == VAL_ARRAY || v->type == VAL_DICT) {
            print_value_recursive(v);
        } else {
            switch (v->type) {
                case VAL_INT:
                    printf("%d", v->data.int_val);
                    break;
                case VAL_FLOAT:
                    printf("%g", v->data.float_val);
                    break;
                case VAL_BOOL:
                    printf("%s", v->data.bool_val ? "True" : "False");
                    break;
                case VAL_NULL:
                    printf("None");
                    break;
                default:
                    printf("<object>");
            }
        }
        if (i < arg_count - 1) printf(" ");
    }
    return create_null_value();
}

static Value *builtin_println(Value **args, int arg_count) {
    builtin_print(args, arg_count);
    printf("\n");
    return create_null_value();
}

static Value *builtin_len(Value **args, int arg_count) {
    if (arg_count != 1) {
        fprintf(stderr, "len() takes exactly 1 argument\n");
        exit(1);
    }
    Value *v = args[0];
    if (v->type == VAL_STRING) {
        return create_int_value(strlen(v->data.string_val));
    } else if (v->type == VAL_ARRAY) {
        return create_int_value(v->data.array_val->size);
    } else if (v->type == VAL_DICT) {
        return create_int_value(v->data.dict_val->size);
    }
    fprintf(stderr, "len() not supported for this type\n");
    exit(1);
}

static Value *builtin_int(Value **args, int arg_count) {
    if (arg_count != 1) {
        fprintf(stderr, "int() takes exactly 1 argument\n");
        exit(1);
    }
    Value *v = args[0];
    if (v->type == VAL_INT) return v;
    if (v->type == VAL_FLOAT) return create_int_value((int)v->data.float_val);
    if (v->type == VAL_STRING) return create_int_value(atoi(v->data.string_val));
    if (v->type == VAL_BOOL) return create_int_value(v->data.bool_val);
    return create_int_value(0);
}

static Value *builtin_float(Value **args, int arg_count) {
    if (arg_count != 1) {
        fprintf(stderr, "float() takes exactly 1 argument\n");
        exit(1);
    }
    Value *v = args[0];
    if (v->type == VAL_FLOAT) return v;
    if (v->type == VAL_INT) return create_float_value((double)v->data.int_val);
    if (v->type == VAL_STRING) return create_float_value(atof(v->data.string_val));
    return create_float_value(0.0);
}

static Value *builtin_str(Value **args, int arg_count) {
    if (arg_count != 1) {
        fprintf(stderr, "str() takes exactly 1 argument\n");
        exit(1);
    }
    Value *v = args[0];
    char buf[256];
    if (v->type == VAL_STRING) return v;
    if (v->type == VAL_INT) {
        sprintf(buf, "%d", v->data.int_val);
        return create_string_value(buf);
    }
    if (v->type == VAL_FLOAT) {
        sprintf(buf, "%g", v->data.float_val);
        return create_string_value(buf);
    }
    if (v->type == VAL_BOOL) {
        return create_string_value(v->data.bool_val ? "True" : "False");
    }
    return create_string_value("");
}

static Value *builtin_append(Value **args, int arg_count) {
    if (arg_count != 2) {
        fprintf(stderr, "append() takes exactly 2 arguments\n");
        exit(1);
    }
    if (args[0]->type != VAL_ARRAY) {
        fprintf(stderr, "append() requires an array\n");
        exit(1);
    }
    array_append(args[0]->data.array_val, args[1]);
    return create_null_value();
}

static Value *builtin_type(Value **args, int arg_count) {
    if (arg_count != 1) {
        fprintf(stderr, "type() takes exactly 1 argument\n");
        exit(1);
    }
    Value *v = args[0];
    switch (v->type) {
        case VAL_INT: return create_string_value("int");
        case VAL_FLOAT: return create_string_value("float");
        case VAL_STRING: return create_string_value("string");
        case VAL_BOOL: return create_string_value("bool");
        case VAL_ARRAY: return create_string_value("array");
        case VAL_DICT: return create_string_value("dict");
        case VAL_FUNC: return create_string_value("function");
        default: return create_string_value("unknown");
    }
}

static Value *builtin_keys(Value **args, int arg_count) {
    if (arg_count != 1 || args[0]->type != VAL_DICT) {
        fprintf(stderr, "keys() requires a dict\n");
        exit(1);
    }
    int count;
    char **keys = dict_keys(args[0]->data.dict_val, &count);
    Value *arr = create_array_value();
    for (int i = 0; i < count; i++) {
        array_append(arr->data.array_val, create_string_value(keys[i]));
    }
    free(keys);
    return arr;
}

static Value *builtin_values(Value **args, int arg_count) {
    if (arg_count != 1 || args[0]->type != VAL_DICT) {
        fprintf(stderr, "values() requires a dict\n");
        exit(1);
    }
    Value *arr = create_array_value();
    Dict *dict = args[0]->data.dict_val;
    for (int i = 0; i < HASH_SIZE; i++) {
        DictEntry *entry = dict->buckets[i];
        while (entry != NULL) {
            array_append(arr->data.array_val, entry->value);
            entry = entry->next;
        }
    }
    return arr;
}

static Value *builtin_input(Value **args, int arg_count) {
    char buffer[1024];
    if (arg_count > 0 && args[0]->type == VAL_STRING) {
        printf("%s", args[0]->data.string_val);
    }
    if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        // Remove newline if present
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        return create_string_value(buffer);
    }
    return create_string_value("");
}

static Value *builtin_read(Value **args, int arg_count) {
    if (arg_count != 1 || args[0]->type != VAL_STRING) {
        fprintf(stderr, "read() requires a filename string\n");
        exit(1);
    }

    FILE *fp = fopen(args[0]->data.string_val, "r");
    if (fp == NULL) {
        fprintf(stderr, "Error opening file: %s\n", args[0]->data.string_val);
        exit(1);
    }

    // Read entire file
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *content = malloc(fsize + 1);
    fread(content, 1, fsize, fp);
    content[fsize] = '\0';
    fclose(fp);

    return create_string_value(content);
}

static Value *builtin_write(Value **args, int arg_count) {
    if (arg_count != 2) {
        fprintf(stderr, "write() requires content and filename\n");
        exit(1);
    }

    Value *content = args[0];
    if (args[1]->type != VAL_STRING) {
        fprintf(stderr, "write() filename must be a string\n");
        exit(1);
    }

    FILE *fp = fopen(args[1]->data.string_val, "w");
    if (fp == NULL) {
        fprintf(stderr, "Error writing to file: %s\n", args[1]->data.string_val);
        exit(1);
    }

    // Convert content to string if needed
    if (content->type == VAL_STRING) {
        fprintf(fp, "%s", content->data.string_val);
    } else if (content->type == VAL_INT) {
        fprintf(fp, "%d", content->data.int_val);
    } else if (content->type == VAL_FLOAT) {
        fprintf(fp, "%g", content->data.float_val);
    } else {
        fprintf(fp, "%s", content->data.bool_val ? "True" : "False");
    }

    fclose(fp);
    return create_null_value();
}


static Value *builtin_regexp_match(Value **args, int arg_count) {
    if (arg_count != 2) {
        fprintf(stderr, "regexp_match() requires pattern and string\n");
        exit(1);
    }
    if (args[0]->type != VAL_STRING || args[1]->type != VAL_STRING) {
        fprintf(stderr, "regexp_match() requires two string arguments\n");
        exit(1);
    }

    char *pattern = args[0]->data.string_val;
    char *str = args[1]->data.string_val;

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        fprintf(stderr, "Failed to compile regex: %s\n", pattern);
        regfree(&regex);
        return create_int_value(0);
    }

    ret = regexec(&regex, str, 0, NULL, 0);
    regfree(&regex);

    return create_int_value((ret == 0) ? 1 : 0);
}

static Value *builtin_regexp_find(Value **args, int arg_count) {
    if (arg_count != 2) {
        fprintf(stderr, "regexp_find() requires pattern and string\n");
        exit(1);
    }
    if (args[0]->type != VAL_STRING || args[1]->type != VAL_STRING) {
        fprintf(stderr, "regexp_find() requires two string arguments\n");
        exit(1);
    }

    char *pattern = args[0]->data.string_val;
    char *str = args[1]->data.string_val;

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        fprintf(stderr, "Failed to compile regex: %s\n", pattern);
        regfree(&regex);
        return create_array_value();
    }

    // Get number of capture groups
    size_t num_groups = regex.re_nsub + 1;  // +1 for the whole match
    regmatch_t *matches = (regmatch_t*)malloc(num_groups * sizeof(regmatch_t));

    // Find all matches
    Value *result = create_array_value();
    char *search_str = str;

    while (regexec(&regex, search_str, num_groups, matches, 0) == 0) {
        // If there are capture groups, return only the captured parts
        // Otherwise return the whole match
        int start_idx = (num_groups > 1) ? 1 : 0;  // Skip whole match if we have groups

        for (size_t i = start_idx; i < num_groups; i++) {
            if (matches[i].rm_so == -1) continue;  // This group didn't match

            int match_len = matches[i].rm_eo - matches[i].rm_so;
            char *matched = (char*)malloc(match_len + 1);
            strncpy(matched, search_str + matches[i].rm_so, match_len);
            matched[match_len] = '\0';

            // Add to result array
            Value *matched_val = create_string_value(matched);
            array_append(result->data.array_val, matched_val);
        }

        // Move to next position after the whole match
        int advance = matches[0].rm_eo;
        if (advance == 0) {
            if (*search_str == '\0') break;
            search_str++;
        } else {
            search_str += advance;
        }
    }

    free(matches);
    regfree(&regex);
    return result;
}

static Value *builtin_regexp_replace(Value **args, int arg_count) {
    if (arg_count != 3) {
        fprintf(stderr, "regexp_replace() requires pattern, string, and replacement\n");
        exit(1);
    }
    if (args[0]->type != VAL_STRING || args[1]->type != VAL_STRING || args[2]->type != VAL_STRING) {
        fprintf(stderr, "regexp_replace() requires three string arguments\n");
        exit(1);
    }

    char *pattern = args[0]->data.string_val;
    char *str = args[1]->data.string_val;
    char *replacement = args[2]->data.string_val;

    regex_t regex;
    int ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (ret != 0) {
        fprintf(stderr, "Failed to compile regex: %s\n", pattern);
        regfree(&regex);
        return create_string_value(strdup(str));
    }

    // Build result string
    char *result_str = (char*)malloc(4096);
    result_str[0] = '\0';
    int result_pos = 0;

    regmatch_t match;
    char *search_str = str;

    while (regexec(&regex, search_str, 1, &match, 0) == 0) {
        // Copy text before match
        int pre_len = match.rm_so;
        if (result_pos + pre_len < 4096) {
            strncat(result_str + result_pos, search_str, pre_len);
            result_pos += pre_len;
        }

        // Copy replacement
        int repl_len = strlen(replacement);
        if (result_pos + repl_len < 4096) {
            strcat(result_str + result_pos, replacement);
            result_pos += repl_len;
        }

        // Move to next position
        search_str += match.rm_eo;

        // Prevent infinite loop on zero-length matches
        if (match.rm_eo == 0) {
            if (*search_str == '\0') break;
            if (result_pos < 4095) {
                result_str[result_pos++] = *search_str;
                result_str[result_pos] = '\0';
            }
            search_str++;
        }
    }

    // Copy remaining text
    if (result_pos < 4096) {
        strcat(result_str + result_pos, search_str);
    }

    regfree(&regex);
    return create_string_value(result_str);
}

static Value *builtin_str_split(Value **args, int arg_count) {
    if (arg_count != 2) {
        fprintf(stderr, "str_split() requires string and separator\n");
        exit(1);
    }
    if (args[0]->type != VAL_STRING || args[1]->type != VAL_STRING) {
        fprintf(stderr, "str_split() requires two string arguments\n");
        exit(1);
    }

    char *str = args[0]->data.string_val;
    char *separator = args[1]->data.string_val;
    int sep_len = strlen(separator);

    if (sep_len == 0) {
        fprintf(stderr, "str_split separator cannot be empty\n");
        exit(1);
    }

    Value *result = create_array_value();
    char *current = str;
    char *next;

    while ((next = strstr(current, separator)) != NULL) {
        // Extract substring before separator
        int len = next - current;
        char *part = (char*)malloc(len + 1);
        strncpy(part, current, len);
        part[len] = '\0';

        // Add to result array
        Value *part_val = create_string_value(part);
        array_append(result->data.array_val, part_val);

        // Move to position after separator
        current = next + sep_len;
    }

    // Add remaining part after last separator
    Value *last_val = create_string_value(strdup(current));
    array_append(result->data.array_val, last_val);

    return result;
}

static Value *builtin_str_join(Value **args, int arg_count) {
    if (arg_count != 2) {
        fprintf(stderr, "str_join() requires array and separator\n");
        exit(1);
    }
    if (args[0]->type != VAL_ARRAY || args[1]->type != VAL_STRING) {
        fprintf(stderr, "str_join() requires array and string separator\n");
        exit(1);
    }

    Array *arr = args[0]->data.array_val;
    char *separator = args[1]->data.string_val;

    // Calculate total length needed
    int total_len = 0;
    for (int i = 0; i < arr->size; i++) {
        Value *elem = arr->elements[i];
        if (elem->type == VAL_STRING) {
            total_len += strlen(elem->data.string_val);
        } else if (elem->type == VAL_INT) {
            total_len += 20;
        } else {
            total_len += 30;
        }
        
        if (i < arr->size - 1) {
            total_len += strlen(separator);
        }
    }

    // Build result string
    char *result_str = (char*)malloc(total_len + 1);
    result_str[0] = '\0';

    for (int i = 0; i < arr->size; i++) {
        Value *elem = arr->elements[i];
        char temp[128];
        
        if (elem->type == VAL_STRING) {
            strcat(result_str, elem->data.string_val);
        } else if (elem->type == VAL_INT) {
            sprintf(temp, "%d", elem->data.int_val);
            strcat(result_str, temp);
        } else if (elem->type == VAL_FLOAT) {
            sprintf(temp, "%g", elem->data.float_val);
            strcat(result_str, temp);
        } else {
            strcat(result_str, "<object>");
        }

        if (i < arr->size - 1) {
            strcat(result_str, separator);
        }
    }

    return create_string_value(result_str);
}
static void setup_builtins() {
    env_define(global_env, "print", create_builtin_value(builtin_print));
    env_define(global_env, "println", create_builtin_value(builtin_println));
    env_define(global_env, "len", create_builtin_value(builtin_len));
    env_define(global_env, "int", create_builtin_value(builtin_int));
    env_define(global_env, "float", create_builtin_value(builtin_float));
    env_define(global_env, "str", create_builtin_value(builtin_str));
    env_define(global_env, "type", create_builtin_value(builtin_type));
    env_define(global_env, "append", create_builtin_value(builtin_append));
    env_define(global_env, "keys", create_builtin_value(builtin_keys));
    env_define(global_env, "values", create_builtin_value(builtin_values));
    env_define(global_env, "input", create_builtin_value(builtin_input));
    env_define(global_env, "read", create_builtin_value(builtin_read));
    env_define(global_env, "write", create_builtin_value(builtin_write));
    env_define(global_env, "regexp_match", create_builtin_value(builtin_regexp_match));
    env_define(global_env, "regexp_find", create_builtin_value(builtin_regexp_find));
    env_define(global_env, "regexp_replace", create_builtin_value(builtin_regexp_replace));
    env_define(global_env, "str_split", create_builtin_value(builtin_str_split));
    env_define(global_env, "str_join", create_builtin_value(builtin_str_join));
}

/* Evaluation functions */
static int is_truthy(Value *v) {
    if (v->type == VAL_BOOL) return v->data.bool_val;
    if (v->type == VAL_NULL) return 0;
    if (v->type == VAL_INT) return v->data.int_val != 0;
    if (v->type == VAL_FLOAT) return v->data.float_val != 0.0;
    if (v->type == VAL_STRING) return strlen(v->data.string_val) > 0;
    if (v->type == VAL_ARRAY) return v->data.array_val->size > 0;
    if (v->type == VAL_DICT) return v->data.dict_val->size > 0;
    return 1;
}

static Value *eval_binary_op(Value *left, Operator op, Value *right) {
    switch (op) {
        case OP_ADD:
            if (left->type == VAL_STRING || right->type == VAL_STRING) {
                char buf[1024];
                char left_str[512], right_str[512];
                if (left->type == VAL_STRING) strcpy(left_str, left->data.string_val);
                else if (left->type == VAL_INT) sprintf(left_str, "%d", left->data.int_val);
                else sprintf(left_str, "%g", left->data.float_val);
                if (right->type == VAL_STRING) strcpy(right_str, right->data.string_val);
                else if (right->type == VAL_INT) sprintf(right_str, "%d", right->data.int_val);
                else sprintf(right_str, "%g", right->data.float_val);
                sprintf(buf, "%s%s", left_str, right_str);
                return create_string_value(buf);
            }
            if (left->type == VAL_FLOAT || right->type == VAL_FLOAT) {
                double l = left->type == VAL_FLOAT ? left->data.float_val : left->data.int_val;
                double r = right->type == VAL_FLOAT ? right->data.float_val : right->data.int_val;
                return create_float_value(l + r);
            }
            return create_int_value(left->data.int_val + right->data.int_val);

        case OP_SUB:
            if (left->type == VAL_FLOAT || right->type == VAL_FLOAT) {
                double l = left->type == VAL_FLOAT ? left->data.float_val : left->data.int_val;
                double r = right->type == VAL_FLOAT ? right->data.float_val : right->data.int_val;
                return create_float_value(l - r);
            }
            return create_int_value(left->data.int_val - right->data.int_val);

        case OP_MUL:
            if (left->type == VAL_FLOAT || right->type == VAL_FLOAT) {
                double l = left->type == VAL_FLOAT ? left->data.float_val : left->data.int_val;
                double r = right->type == VAL_FLOAT ? right->data.float_val : right->data.int_val;
                return create_float_value(l * r);
            }
            return create_int_value(left->data.int_val * right->data.int_val);

        case OP_DIV:
            if (left->type == VAL_INT && right->type == VAL_INT) {
                return create_int_value(left->data.int_val / right->data.int_val);
            }
            double l = left->type == VAL_FLOAT ? left->data.float_val : left->data.int_val;
            double r = right->type == VAL_FLOAT ? right->data.float_val : right->data.int_val;
            return create_float_value(l / r);

        case OP_MOD:
            return create_int_value(left->data.int_val % right->data.int_val);

        case OP_EQ:
            if (left->type != right->type) return create_bool_value(0);
            if (left->type == VAL_INT) return create_bool_value(left->data.int_val == right->data.int_val);
            if (left->type == VAL_FLOAT) return create_bool_value(left->data.float_val == right->data.float_val);
            if (left->type == VAL_STRING) return create_bool_value(strcmp(left->data.string_val, right->data.string_val) == 0);
            if (left->type == VAL_BOOL) return create_bool_value(left->data.bool_val == right->data.bool_val);
            return create_bool_value(0);

        case OP_NE:
            if (left->type != right->type) return create_bool_value(1);
            if (left->type == VAL_INT) return create_bool_value(left->data.int_val != right->data.int_val);
            if (left->type == VAL_FLOAT) return create_bool_value(left->data.float_val != right->data.float_val);
            if (left->type == VAL_STRING) return create_bool_value(strcmp(left->data.string_val, right->data.string_val) != 0);
            if (left->type == VAL_BOOL) return create_bool_value(left->data.bool_val != right->data.bool_val);
            return create_bool_value(1);

        case OP_LT:
            if (left->type == VAL_STRING && right->type == VAL_STRING) {
                return create_bool_value(strcmp(left->data.string_val, right->data.string_val) < 0);
            }
            if (left->type == VAL_FLOAT || right->type == VAL_FLOAT) {
                double l = left->type == VAL_FLOAT ? left->data.float_val : left->data.int_val;
                double r = right->type == VAL_FLOAT ? right->data.float_val : right->data.int_val;
                return create_bool_value(l < r);
            }
            return create_bool_value(left->data.int_val < right->data.int_val);

        case OP_LE:
            if (left->type == VAL_STRING && right->type == VAL_STRING) {
                return create_bool_value(strcmp(left->data.string_val, right->data.string_val) <= 0);
            }
            if (left->type == VAL_FLOAT || right->type == VAL_FLOAT) {
                double l = left->type == VAL_FLOAT ? left->data.float_val : left->data.int_val;
                double r = right->type == VAL_FLOAT ? right->data.float_val : right->data.int_val;
                return create_bool_value(l <= r);
            }
            return create_bool_value(left->data.int_val <= right->data.int_val);

        case OP_GT:
            if (left->type == VAL_STRING && right->type == VAL_STRING) {
                return create_bool_value(strcmp(left->data.string_val, right->data.string_val) > 0);
            }
            if (left->type == VAL_FLOAT || right->type == VAL_FLOAT) {
                double l = left->type == VAL_FLOAT ? left->data.float_val : left->data.int_val;
                double r = right->type == VAL_FLOAT ? right->data.float_val : right->data.int_val;
                return create_bool_value(l > r);
            }
            return create_bool_value(left->data.int_val > right->data.int_val);

        case OP_GE:
            if (left->type == VAL_STRING && right->type == VAL_STRING) {
                return create_bool_value(strcmp(left->data.string_val, right->data.string_val) >= 0);
            }
            if (left->type == VAL_FLOAT || right->type == VAL_FLOAT) {
                double l = left->type == VAL_FLOAT ? left->data.float_val : left->data.int_val;
                double r = right->type == VAL_FLOAT ? right->data.float_val : right->data.int_val;
                return create_bool_value(l >= r);
            }
            return create_bool_value(left->data.int_val >= right->data.int_val);

        case OP_AND:
            return create_bool_value(is_truthy(left) && is_truthy(right));

        case OP_OR:
            return create_bool_value(is_truthy(left) || is_truthy(right));

        case OP_IN:
            // Check if left is in right (element in array, key in dict, substring in string)
            if (right->type == VAL_ARRAY) {
                // element in array
                Array *arr = right->data.array_val;
                for (int i = 0; i < arr->size; i++) {
                    Value *elem = arr->elements[i];
                    // Compare elements
                    if (left->type == elem->type) {
                        if (left->type == VAL_INT && left->data.int_val == elem->data.int_val) {
                            return create_bool_value(1);
                        }
                        if (left->type == VAL_FLOAT && left->data.float_val == elem->data.float_val) {
                            return create_bool_value(1);
                        }
                        if (left->type == VAL_STRING && strcmp(left->data.string_val, elem->data.string_val) == 0) {
                            return create_bool_value(1);
                        }
                        if (left->type == VAL_BOOL && left->data.bool_val == elem->data.bool_val) {
                            return create_bool_value(1);
                        }
                    }
                }
                return create_bool_value(0);
            } else if (right->type == VAL_DICT) {
                // key in dict
                if (left->type != VAL_STRING) {
                    fprintf(stderr, "Dictionary keys must be strings\n");
                    exit(1);
                }
                Dict *dict = right->data.dict_val;
                for (int i = 0; i < HASH_SIZE; i++) {
                    DictEntry *entry = dict->buckets[i];
                    while (entry != NULL) {
                        if (strcmp(entry->key, left->data.string_val) == 0) {
                            return create_bool_value(1);
                        }
                        entry = entry->next;
                    }
                }
                return create_bool_value(0);
            } else if (right->type == VAL_STRING) {
                // substring in string
                if (left->type != VAL_STRING) {
                    fprintf(stderr, "Can only check if string is in string\n");
                    exit(1);
                }
                if (strstr(right->data.string_val, left->data.string_val) != NULL) {
                    return create_bool_value(1);
                } else {
                    return create_bool_value(0);
                }
            } else {
                fprintf(stderr, "IN operator requires array, dict, or string on the right side\n");
                exit(1);
            }

        default:
            fprintf(stderr, "Unknown binary operator\n");
            exit(1);
    }
}

static Value *eval_expression(ASTNode *node) {
    switch (node->type) {
        case NODE_INT_LITERAL:
            return create_int_value(node->data.int_literal.value);

        case NODE_FLOAT_LITERAL:
            return create_float_value(node->data.float_literal.value);

        case NODE_STRING_LITERAL:
            return create_string_value(node->data.string_literal.value);

        case NODE_BOOL_LITERAL:
            return create_bool_value(node->data.bool_literal.value);

        case NODE_IDENTIFIER:
            return env_get(current_env, node->data.identifier.name);

        case NODE_BINARY_OP: {
            Value *left = eval_expression(node->data.binary_op.left);
            Value *right = eval_expression(node->data.binary_op.right);
            return eval_binary_op(left, node->data.binary_op.op, right);
        }

        case NODE_UNARY_OP: {
            Value *operand = eval_expression(node->data.unary_op.operand);
            if (node->data.unary_op.op == OP_NEG) {
                if (operand->type == VAL_INT) return create_int_value(-operand->data.int_val);
                if (operand->type == VAL_FLOAT) return create_float_value(-operand->data.float_val);
            } else if (node->data.unary_op.op == OP_NOT) {
                return create_bool_value(!is_truthy(operand));
            }
            return operand;
        }

        case NODE_ARRAY_LITERAL: {
            Value *arr = create_array_value();
            ASTNodeList *elem = node->data.array_literal.elements;
            while (elem != NULL) {
                array_append(arr->data.array_val, eval_expression(elem->node));
                elem = elem->next;
            }
            return arr;
        }

        case NODE_DICT_LITERAL: {
            Value *dict = create_dict_value();
            ASTNodeList *pair = node->data.dict_literal.pairs;
            while (pair != NULL) {
                ASTNode *pair_node = pair->node;
                Value *key = eval_expression(pair_node->data.dict_pair.key);
                Value *value = eval_expression(pair_node->data.dict_pair.value);
                dict_set(dict->data.dict_val, key->data.string_val, value);
                pair = pair->next;
            }
            return dict;
        }

        case NODE_INDEX_ACCESS: {
            Value *obj = eval_expression(node->data.index_access.object);
            Value *idx = eval_expression(node->data.index_access.index);

            if (obj->type == VAL_ARRAY) {
                return array_get(obj->data.array_val, idx->data.int_val);
            } else if (obj->type == VAL_DICT) {
                return dict_get(obj->data.dict_val, idx->data.string_val);
            } else if (obj->type == VAL_STRING) {
                int i = idx->data.int_val;
                char c[2] = {obj->data.string_val[i], '\0'};
                return create_string_value(c);
            }
            fprintf(stderr, "Cannot index this type\n");
            exit(1);
        }

        case NODE_SLICE_ACCESS: {
            Value *obj = eval_expression(node->data.slice_access.object);
            Value *start_val = eval_expression(node->data.slice_access.start);
            Value *end_val = eval_expression(node->data.slice_access.end);

            if (start_val->type != VAL_INT || end_val->type != VAL_INT) {
                fprintf(stderr, "Slice indices must be integers\n");
                exit(1);
            }

            int start = start_val->data.int_val;
            int end = end_val->data.int_val;

            if (obj->type == VAL_ARRAY) {
                Array *arr = obj->data.array_val;
                if (start < 0) start = 0;
                if (end > arr->size) end = arr->size;
                if (start > end) start = end;

                Value *result = create_array_value();
                for (int i = start; i < end; i++) {
                    array_append(result->data.array_val, arr->elements[i]);
                }
                return result;
            } else if (obj->type == VAL_STRING) {
                char *str = obj->data.string_val;
                int len = strlen(str);
                if (start < 0) start = 0;
                if (end > len) end = len;
                if (start > end) start = end;

                int slice_len = end - start;
                char *result_str = malloc(slice_len + 1);
                strncpy(result_str, str + start, slice_len);
                result_str[slice_len] = '\0';
                return create_string_value(result_str);
            }
            fprintf(stderr, "Cannot slice this type\n");
            exit(1);
        }

        case NODE_FUNC_CALL: {
            Value *func = env_get(current_env, node->data.func_call.name);

            // Evaluate arguments
            int arg_count = 0;
            ASTNodeList *arg_node = node->data.func_call.arguments;
            while (arg_node != NULL) {
                arg_count++;
                arg_node = arg_node->next;
            }

            Value **args = malloc(arg_count * sizeof(Value*));
            arg_node = node->data.func_call.arguments;
            for (int i = 0; i < arg_count; i++) {
                args[i] = eval_expression(arg_node->node);
                arg_node = arg_node->next;
            }

            if (func->type == VAL_BUILTIN) {
                Value *result = func->data.builtin_val(args, arg_count);
                free(args);
                return result;
            }

            if (func->type == VAL_FUNC) {
                Function *f = func->data.func_val;
                Environment *func_env = create_environment(f->env);

                // Bind parameters
                ASTNodeList *param = f->params;
                for (int i = 0; i < arg_count; i++) {
                    if (param == NULL) {
                        fprintf(stderr, "Too many arguments\n");
                        exit(1);
                    }
                    env_define(func_env, param->node->data.identifier.name, args[i]);
                    param = param->next;
                }

                // Execute function body
                Environment *prev_env = current_env;
                current_env = func_env;

                has_returned = 0;
                ASTNodeList *stmt = f->body;
                while (stmt != NULL && !has_returned) {
                    eval_statement(stmt->node);
                    stmt = stmt->next;
                }

                current_env = prev_env;
                free(args);

                if (has_returned) {
                    Value *result = return_value;
                    has_returned = 0;  // Reset flag for next call
                    return result;
                } else {
                    return create_null_value();
                }
            }

            fprintf(stderr, "Not a function\n");
            exit(1);
        }

        default:
            fprintf(stderr, "Unknown expression type\n");
            exit(1);
    }
}

static void eval_statement(ASTNode *node) {
    switch (node->type) {
        case NODE_VAR_DECL: {
            Value *val = eval_expression(node->data.var_decl.value);
            env_define(current_env, node->data.var_decl.name, val);
            break;
        }

        case NODE_ASSIGNMENT: {
            Value *val = eval_expression(node->data.assignment.value);
            ASTNode *target = node->data.assignment.target;

            if (target->type == NODE_IDENTIFIER) {
                env_set(current_env, target->data.identifier.name, val);
            } else if (target->type == NODE_INDEX_ACCESS) {
                Value *obj = eval_expression(target->data.index_access.object);
                Value *idx = eval_expression(target->data.index_access.index);

                if (obj->type == VAL_ARRAY) {
                    array_set(obj->data.array_val, idx->data.int_val, val);
                } else if (obj->type == VAL_DICT) {
                    dict_set(obj->data.dict_val, idx->data.string_val, val);
                }
            }
            break;
        }

        case NODE_FUNC_DEF: { // 这是函数定义, 不是函数调用. 函数调用在 eval_expression NODE_FUNC_CALL
            Value *func = create_func_value(
                node->data.func_def.name,
                node->data.func_def.params,
                node->data.func_def.body,
                current_env
            );
            env_define(current_env, node->data.func_def.name, func);
            break;
        }

        case NODE_RETURN: {
            return_value = node->data.return_stmt.value ?
                eval_expression(node->data.return_stmt.value) :
                create_null_value();
            has_returned = 1;
            break;
        }

        case NODE_IF_STMT: {
            Value *cond = eval_expression(node->data.if_stmt.condition);
            if (is_truthy(cond)) {
                ASTNodeList *stmt = node->data.if_stmt.then_block;
                while (stmt != NULL && !has_returned) {
                    eval_statement(stmt->node);
                    stmt = stmt->next;
                }
            } else if (node->data.if_stmt.else_block) {
                ASTNodeList *stmt = node->data.if_stmt.else_block;
                while (stmt != NULL && !has_returned) {
                    eval_statement(stmt->node);
                    stmt = stmt->next;
                }
            }
            break;
        }

        case NODE_WHILE_STMT: {
            if (setjmp(break_jmp) == 0) {
                while (1) {
                    if (has_returned) break;

                    Value *cond = eval_expression(node->data.while_stmt.condition);
                    if (!is_truthy(cond)) break;

                    if (setjmp(continue_jmp) == 0) {
                        ASTNodeList *stmt = node->data.while_stmt.body;
                        while (stmt != NULL && !has_returned) {
                            eval_statement(stmt->node);
                            stmt = stmt->next;
                        }
                    }
                }
            }
            break;
        }

        case NODE_FOREACH_STMT: {
            Value *collection = eval_expression(node->data.foreach_stmt.collection);

            if (setjmp(break_jmp) == 0) {
                if (collection->type == VAL_ARRAY) {
                    Array *arr = collection->data.array_val;
                    for (int i = 0; i < arr->size && !has_returned; i++) {
                        env_define(current_env, node->data.foreach_stmt.key_var, create_int_value(i));
                        env_define(current_env, node->data.foreach_stmt.value_var, arr->elements[i]);

                        if (setjmp(continue_jmp) == 0) {
                            ASTNodeList *stmt = node->data.foreach_stmt.body;
                            while (stmt != NULL && !has_returned) {
                                eval_statement(stmt->node);
                                stmt = stmt->next;
                            }
                        }
                    }
                } else if (collection->type == VAL_DICT) {
                    Dict *dict = collection->data.dict_val;
                    int count;
                    char **keys = dict_keys(dict, &count);

                    for (int i = 0; i < count && !has_returned; i++) {
                        env_define(current_env, node->data.foreach_stmt.key_var, create_string_value(keys[i]));
                        env_define(current_env, node->data.foreach_stmt.value_var, dict_get(dict, keys[i]));

                        if (setjmp(continue_jmp) == 0) {
                            ASTNodeList *stmt = node->data.foreach_stmt.body;
                            while (stmt != NULL && !has_returned) {
                                eval_statement(stmt->node);
                                stmt = stmt->next;
                            }
                        }
                    }

                    free(keys);
                } else {
                    fprintf(stderr, "Can only iterate over arrays and dicts\n");
                    exit(1);
                }
            }
            break;
        }

        case NODE_BREAK:
            longjmp(break_jmp, 1);
            break;

        case NODE_CONTINUE:
            longjmp(continue_jmp, 1);
            break;

        case NODE_FUNC_CALL:
            eval_expression(node);
            break;

        default:
            eval_expression(node);
            break;
    }
}

void interpret(ASTNode *root) {
    global_env = create_environment(NULL);
    current_env = global_env;

    setup_builtins();

    if (root->type == NODE_PROGRAM) {
        ASTNodeList *stmt = root->data.program.statements;
        while (stmt != NULL) {
            eval_statement(stmt->node);
            stmt = stmt->next;
        }
    }
}
