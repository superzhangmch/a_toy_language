CC = gcc
CFLAGS = -Wall -g -Icore
FLEX = flex
BISON = bison

# Interpreter version
INTERP_SRCS = interpreter_main.c core/ast.c interpreter.c core/tiny.tab.c core/lex.yy.c
INTERP_OBJS = $(INTERP_SRCS:.c=.o)
INTERP_TARGET = tinyc

# Compiler version (C code generator)
COMPILER_SRCS = tcc.c core/ast.c codegen_full.c core/tiny.tab.c core/lex.yy.c
COMPILER_OBJS = $(COMPILER_SRCS:.c=.o)
COMPILER_TARGET = tinycc

# LLVM backend compiler
LLVM_SRCS = tlc.c core/ast.c codegen_llvm.c core/tiny.tab.c core/lex.yy.c
LLVM_OBJS = $(LLVM_SRCS:.c=.o)
LLVM_TARGET = tlc

all: $(INTERP_TARGET) $(COMPILER_TARGET) $(LLVM_TARGET)

core/tiny.tab.c core/tiny.tab.h: core/tiny.y
	$(BISON) -d core/tiny.y -o core/tiny.tab.c

core/lex.yy.c: core/tiny.l core/tiny.tab.h
	$(FLEX) -o core/lex.yy.c core/tiny.l

$(INTERP_TARGET): $(INTERP_OBJS)
	$(CC) $(CFLAGS) -o $(INTERP_TARGET) $(INTERP_OBJS)

$(COMPILER_TARGET): $(COMPILER_OBJS)
	$(CC) $(CFLAGS) -o $(COMPILER_TARGET) $(COMPILER_OBJS)

$(LLVM_TARGET): $(LLVM_OBJS)
	$(CC) $(CFLAGS) -o $(LLVM_TARGET) $(LLVM_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(INTERP_OBJS) $(COMPILER_OBJS) $(LLVM_OBJS) $(INTERP_TARGET) $(COMPILER_TARGET) $(LLVM_TARGET) core/tiny.tab.c core/tiny.tab.h core/lex.yy.c runtime.o a.out

test: $(INTERP_TARGET)
	@echo "Testing interpreter with hello.tl:"
	./$(INTERP_TARGET) examples/hello.tl

test-compiler: $(COMPILER_TARGET)
	@echo "Testing compiler..."
	./$(COMPILER_TARGET) examples/test_simple.tl -o test_program
	@echo "\nRunning compiled program:"
	./test_program
	rm -f test_program

test-llvm: $(LLVM_TARGET)
	@echo "Testing LLVM backend..."
	./$(LLVM_TARGET) test_func_only.tl -o test_llvm
	@echo "\nRunning LLVM-compiled program:"
	./test_llvm
	rm -f test_llvm

.PHONY: all clean test test-compiler test-llvm
