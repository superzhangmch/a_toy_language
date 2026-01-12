#include <stdio.h>
#include <stdlib.h>
#include "ast.h"
#include "interpreter.h"
#include "core/preprocess.h"

extern int yyparse();
extern FILE *yyin;
extern ASTNode *root;
extern PreprocessResult g_pp_result;
extern int yylineno;
typedef void* YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char *yy_str);
extern void yy_delete_buffer(YY_BUFFER_STATE b);

int main(int argc, char **argv) {
    // Set command line arguments for cmd_args() builtin
    set_cmd_args(argc, argv);

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <source.tl>\n", argv[0]);
        return 1;
    }

    PreprocessResult res;
    if (preprocess_file(argv[1], &res) != 0) {
        return 1;
    }
    g_pp_result = res;

    yylineno = 1;
    YY_BUFFER_STATE buf = yy_scan_string(res.combined_source);
    if (yyparse() == 0 && root != NULL) {
        interpret(root);
    }
    yy_delete_buffer(buf);
    free_preprocess_result(&res);

    return 0;
}
