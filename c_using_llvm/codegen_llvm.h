#ifndef CODEGEN_LLVM_H
#define CODEGEN_LLVM_H

#include <stdio.h>
#include "ast.h"

// String literal storage
typedef struct StringLiteral {
    char *value;
    char *global_name;
    struct StringLiteral *next;
} StringLiteral;

typedef struct {
    FILE *out;
    int indent_level;
    int temp_counter;      // For generating temporary variable names
    int label_counter;     // For generating label names
    int string_counter;    // For string literals
    StringLiteral *strings; // Linked list of string literals
} LLVMCodeGen;

void llvm_codegen_init(LLVMCodeGen *gen, FILE *out);
void llvm_codegen_program(LLVMCodeGen *gen, ASTNode *root);

#endif /* CODEGEN_LLVM_H */
