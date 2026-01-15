CC = gcc
CFLAGS = -Wall -g -Icore
LIBS = -lm
READLINE_LIBS = -lreadline
FLEX = flex
BISON = bison

# Interpreter version
INTERP_SRCS = interpreter_main.c core/ast.c interpreter.c core/tiny.tab.c core/lex.yy.c core/preprocess.c
INTERP_OBJS = $(INTERP_SRCS:.c=.o)
INTERP_TARGET = interpreter

# Compiler version (C code generator)
COMPILER_SRCS = c_codegen_main.c core/ast.c c_codegen.c core/tiny.tab.c core/lex.yy.c core/preprocess.c
COMPILER_OBJS = $(COMPILER_SRCS:.c=.o)
COMPILER_TARGET = c_codegen

# LLVM backend compiler
LLVM_SRCS = codegen_llvm_main.c core/ast.c codegen_llvm.c core/tiny.tab.c core/lex.yy.c core/preprocess.c
LLVM_OBJS = $(LLVM_SRCS:.c=.o)
LLVM_TARGET = codegen_llvm

# Runtime library
RUNTIME = runtime.o gc.o

all: $(INTERP_TARGET) $(COMPILER_TARGET) $(LLVM_TARGET)

core/tiny.tab.c core/tiny.tab.h: core/tiny.y
	$(BISON) -d core/tiny.y -o core/tiny.tab.c

core/lex.yy.c: core/tiny.l core/tiny.tab.h
	$(FLEX) -o core/lex.yy.c core/tiny.l

# Ensure headers are generated before compiling sources that include them
core/ast.o: core/tiny.tab.h

$(INTERP_TARGET): $(INTERP_OBJS) $(RUNTIME)
	$(CC) $(CFLAGS) -o $(INTERP_TARGET) $(INTERP_OBJS) $(RUNTIME) $(LIBS) $(READLINE_LIBS)

$(COMPILER_TARGET): $(COMPILER_OBJS)
	$(CC) $(CFLAGS) -o $(COMPILER_TARGET) $(COMPILER_OBJS) $(LIBS)

runtime.o: runtime.c runtime.h gc.h
	$(CC) $(CFLAGS) -c runtime.c -o runtime.o

gc.o: gc.c gc.h runtime.h
	$(CC) $(CFLAGS) -c gc.c -o gc.o

$(LLVM_TARGET): $(LLVM_OBJS) $(RUNTIME)
	$(CC) $(CFLAGS) -o $(LLVM_TARGET) $(LLVM_OBJS) $(RUNTIME) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(INTERP_OBJS) $(COMPILER_OBJS) $(LLVM_OBJS) $(INTERP_TARGET) $(COMPILER_TARGET) $(LLVM_TARGET) core/tiny.tab.c core/tiny.tab.h core/lex.yy.c runtime.o gc.o a.out

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
