#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ast.h"
#include "c_codegen.h"
#include "core/preprocess.h"

extern int yyparse();
extern FILE *yyin;
extern ASTNode *root;
extern PreprocessResult g_pp_result;
extern int yylineno;
typedef void* YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *yy_str);
extern void yy_delete_buffer(YY_BUFFER_STATE b);

static void compile_to_c(const char *output_file) {
    FILE *out = fopen(output_file, "w");
    if (!out) {
        fprintf(stderr, "Error: Cannot create C file %s\n", output_file);
        exit(1);
    }

    CCodeGen gen;
    ccodegen_init(&gen, out);
    ccodegen_program(&gen, root);

    fclose(out);
}

static void run_command(const char *cmd) {
    printf("Running: %s\n", cmd);
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "Error: Command failed with code %d\n", ret);
        exit(1);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source.tl> [-o output]\n", argv[0]);
        return 1;
    }

    const char *input_file = argv[1];
    char *output_file = "a.out";

    // Parse arguments
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[i + 1];
            i++;
        }
    }

    PreprocessResult res;
    if (preprocess_file(input_file, &res) != 0) {
        return 1;
    }
    g_pp_result = res;

    printf("Parsing %s...\n", input_file);
    yylineno = 1;
    YY_BUFFER_STATE buf = yy_scan_string(res.combined_source);
    if (yyparse() != 0 || root == NULL) {
        fprintf(stderr, "Parse error\n");
        yy_delete_buffer(buf);
        free_preprocess_result(&res);
        return 1;
    }
    yy_delete_buffer(buf);

    // Generate C code
    char c_file[256];
    snprintf(c_file, sizeof(c_file), "/tmp/tiny_%d.c", getpid());
    printf("Generating C code: %s...\n", c_file);
    compile_to_c(c_file);

    // Compile C to executable
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -O2 %s -o %s", c_file, output_file);
    run_command(cmd);

    // Cleanup
    unlink(c_file);
    free_preprocess_result(&res);

    printf("Successfully compiled to: %s\n", output_file);
    printf("\nRun with: ./%s\n", output_file);

    return 0;
}
