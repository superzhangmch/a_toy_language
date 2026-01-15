#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "ast.h"
#include "interpreter.h"
#include "core/preprocess.h"
#include "gc.h"

extern int yyparse();
extern FILE *yyin;
extern ASTNode *root;
extern PreprocessResult g_pp_result;
extern int yylineno;
extern int yylex(void);
typedef void* YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *yy_str);
extern void yy_delete_buffer(YY_BUFFER_STATE b);

// Helper function to trim whitespace from both ends of a string
char* trim(char *str) {
    char *end;
    // Trim leading space
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void run_batch_mode(const char *filename) {
    PreprocessResult res;
    if (preprocess_file(filename, &res) != 0) {
        return;
    }
    g_pp_result = res;

    yylineno = 1;
    YY_BUFFER_STATE buf = yy_scan_string(res.combined_source);
    if (yyparse() == 0 && root != NULL) {
        interpret(root);
    }
    yy_delete_buffer(buf);
    free_preprocess_result(&res);
}

void run_interactive_mode() {
    printf("Toy Language Interactive Mode\n");
    printf("Type 'exit' to quit. Use '\\' at the end of a line for multi-line input.\n");
    printf("Use up/down arrows to navigate through command history.\n\n");

    // Initialize the interpreter environment once
    interpret_init();

    // Set history limit to 5
    stifle_history(5);

    char *accumulated_input = NULL;
    size_t accumulated_size = 0;

    while (1) {
        // Choose prompt based on whether we're in multi-line mode
        const char *prompt = accumulated_input ? "... " : "> ";
        char *line = readline(prompt);

        if (line == NULL) {
            // EOF reached (Ctrl+D)
            printf("\n");
            break;
        }

        // Trim the line
        char *trimmed = trim(line);

        // Check for exit command (only if not in multi-line mode)
        if (accumulated_input == NULL && strcmp(trimmed, "exit") == 0) {
            free(line);
            break;
        }

        // Skip empty lines when not in multi-line mode
        if (accumulated_input == NULL && strlen(trimmed) == 0) {
            free(line);
            continue;
        }

        // Check if line ends with backslash for continuation
        int continues = 0;
        size_t line_len = strlen(line);
        if (line_len > 0 && line[line_len - 1] == '\\') {
            continues = 1;
            line[line_len - 1] = '\n';  // Replace \ with newline
        } else {
            // Add newline if not continuing
            char *temp = malloc(line_len + 2);
            strcpy(temp, line);
            strcat(temp, "\n");
            free(line);
            line = temp;
            line_len++;
        }

        // Accumulate input
        if (accumulated_input == NULL) {
            accumulated_input = line;
            accumulated_size = line_len;
        } else {
            size_t new_size = accumulated_size + line_len + 1;
            accumulated_input = realloc(accumulated_input, new_size);
            strcat(accumulated_input, line);
            accumulated_size = new_size - 1;
            free(line);
        }

        // If line continues, keep reading
        if (continues) {
            continue;
        }

        // We have complete input, now parse and execute
        // Add to history (without the newline for display)
        char *hist_copy = strdup(accumulated_input);
        char *newline_pos = strchr(hist_copy, '\n');
        if (newline_pos) *newline_pos = '\0';
        // Only add non-empty commands to history
        if (strlen(trim(hist_copy)) > 0) {
            add_history(hist_copy);
        }
        free(hist_copy);

        // Create a minimal preprocessor result
        PreprocessResult res;
        res.combined_source = strdup(accumulated_input);
        res.mapping_count = 0;
        res.mappings = NULL;
        g_pp_result = res;

        // Parse and execute
        yylineno = 1;
        root = NULL;
        YY_BUFFER_STATE buf = yy_scan_string(accumulated_input);

        int parse_result = yyparse();
        if (parse_result == 0 && root != NULL) {
            // Execute the code - catch any runtime errors with setjmp
            jmp_buf *error_jmp = (jmp_buf*)get_interactive_error_jmpbuf();
            if (setjmp(*error_jmp) == 0) {
                // Normal execution
                interpret_interactive(root);
            } else {
                // Error occurred, longjmp was called
                // Error message already printed by runtime_error
                // Just continue to next iteration
            }
        } else if (parse_result != 0) {
            printf("Parse error - please try again\n");
        }

        yy_delete_buffer(buf);
        free(res.combined_source);
        free(accumulated_input);
        accumulated_input = NULL;
        accumulated_size = 0;
    }

    if (accumulated_input) {
        free(accumulated_input);
    }
}

int main(int argc, char **argv) {
    // Initialize GC
    gc_init();

    // Set stack bottom for conservative stack scanning
    int stack_anchor;
    gc_set_stack_bottom(&stack_anchor);

    if (argc < 2) {
        // No file provided - run in interactive mode
        // In interactive mode, pass all args (though there are none)
        set_cmd_args(argc - 1, argv + 1);
        run_interactive_mode();
    } else {
        // File provided - run in batch mode
        // Skip executable (argv[0]) and script file (argv[1])
        // Only pass the script arguments (argv[2] onwards)
        set_cmd_args(argc - 2, argv + 2);
        run_batch_mode(argv[1]);
    }

    return 0;
}
