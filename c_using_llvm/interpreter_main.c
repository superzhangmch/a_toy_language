#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    printf("Type your code line by line. Press Ctrl+D (Unix) or Ctrl+Z (Windows) to exit.\n\n");

    // Initialize the interpreter environment once
    interpret_init();

    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    while (1) {
        printf("> ");
        fflush(stdout);

        read = getline(&line, &len, stdin);
        if (read == -1) {
            // EOF reached
            printf("\n");
            break;
        }

        // Skip empty lines
        if (read <= 1) {
            continue;
        }

        // Create a minimal preprocessor result for the line
        PreprocessResult res;
        res.combined_source = strdup(line);
        res.mapping_count = 0;
        res.mappings = NULL;
        g_pp_result = res;

        // Parse and execute the line
        yylineno = 1;
        root = NULL;
        YY_BUFFER_STATE buf = yy_scan_string(line);

        int parse_result = yyparse();
        if (parse_result == 0 && root != NULL) {
            interpret_interactive(root);
        } else if (parse_result != 0) {
            printf("Parse error\n");
        }

        yy_delete_buffer(buf);
        free(res.combined_source);
    }

    if (line) {
        free(line);
    }
}

int main(int argc, char **argv) {
    // Initialize GC
    gc_init();

    // Set stack bottom for conservative stack scanning
    int stack_anchor;
    gc_set_stack_bottom(&stack_anchor);

    // Set command line arguments for cmd_args() builtin
    set_cmd_args(argc, argv);

    if (argc < 2) {
        // No file provided - run in interactive mode
        run_interactive_mode();
    } else {
        // File provided - run in batch mode
        run_batch_mode(argv[1]);
    }

    return 0;
}
