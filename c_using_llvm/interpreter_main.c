#include <stdio.h>
#include <stdlib.h>
#include "ast.h"
#include "interpreter.h"

extern int yyparse();
extern FILE *yyin;
extern ASTNode *root;

int main(int argc, char **argv) {
    // Set command line arguments for cmd_args() builtin
    set_cmd_args(argc, argv);

    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            fprintf(stderr, "Error: Cannot open file %s\n", argv[1]);
            return 1;
        }
    } else {
        yyin = stdin;
    }

    if (yyparse() == 0 && root != NULL) {
        interpret(root);
    }

    if (argc > 1) {
        fclose(yyin);
    }

    return 0;
}
