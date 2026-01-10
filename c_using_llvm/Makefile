CC = gcc
CFLAGS = -Wall -g
FLEX = flex
BISON = bison

# Interpreter version
INTERP_SRCS = interpreter_main.c ast.c interpreter.c tiny.tab.c lex.yy.c
INTERP_OBJS = $(INTERP_SRCS:.c=.o)
INTERP_TARGET = tinyc

# Compiler version (C code generator)
COMPILER_SRCS = tcc.c ast.c codegen_full.c tiny.tab.c lex.yy.c
COMPILER_OBJS = $(COMPILER_SRCS:.c=.o)
COMPILER_TARGET = tinycc

# LLVM backend compiler
LLVM_SRCS = tlc.c ast.c codegen_llvm.c tiny.tab.c lex.yy.c
LLVM_OBJS = $(LLVM_SRCS:.c=.o)
LLVM_TARGET = tlc

all: $(INTERP_TARGET) $(COMPILER_TARGET) $(LLVM_TARGET)

tiny.tab.c tiny.tab.h: tiny.y
	$(BISON) -d tiny.y

lex.yy.c: tiny.l tiny.tab.h
	$(FLEX) tiny.l

$(INTERP_TARGET): $(INTERP_OBJS)
	$(CC) $(CFLAGS) -o $(INTERP_TARGET) $(INTERP_OBJS)

$(COMPILER_TARGET): $(COMPILER_OBJS)
	$(CC) $(CFLAGS) -o $(COMPILER_TARGET) $(COMPILER_OBJS)

$(LLVM_TARGET): $(LLVM_OBJS)
	$(CC) $(CFLAGS) -o $(LLVM_TARGET) $(LLVM_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(INTERP_OBJS) $(COMPILER_OBJS) $(LLVM_OBJS) $(INTERP_TARGET) $(COMPILER_TARGET) $(LLVM_TARGET) tiny.tab.c tiny.tab.h lex.yy.c runtime.o a.out

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
