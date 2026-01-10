#ifndef CODEGEN_C_H
#define CODEGEN_C_H

#include <stdio.h>
#include "ast.h"

typedef struct CCodeGen {
    FILE *out;
    int label_counter;
    int temp_counter;
    int indent_level;
} CCodeGen;

void ccodegen_init(CCodeGen *gen, FILE *out);
void ccodegen_program(CCodeGen *gen, ASTNode *root);

#endif /* CODEGEN_C_H */
