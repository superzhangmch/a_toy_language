#!/usr/bin/env python3
"""
Main entry point for the tiny language compiler/interpreter
"""

import sys
from lexer_0 import Lexer
from parser_1 import Parser
from interpreter_3 import Interpreter
import os

def preprocess_file(path: str, include_once_set=None, stack=None, mapping=None, combined=None, combined_line=1):
    if include_once_set is None:
        include_once_set = set()
    if stack is None:
        stack = []
    if mapping is None:
        mapping = []
    if combined is None:
        combined = []

    abs_path = os.path.abspath(path)
    if abs_path in stack:
        raise Exception(f"Include cycle detected at {abs_path}")

    stack.append(abs_path)
    mapping.append((combined_line, abs_path, 1))

    with open(abs_path, 'r') as f:
        for line in f:
            stripped = line.lstrip()
            if stripped.startswith("include_once") or stripped.startswith("include"):
                parts = stripped.split(None, 1)
                if len(parts) == 2:
                    fname = parts[1].strip()
                    if fname[0] in "\"'":
                        fname = fname.strip("\"'")
                    target = fname if os.path.isabs(fname) else os.path.join(os.path.dirname(abs_path), fname)
                    target_abs = os.path.abspath(target)
                    if stripped.startswith("include_once") and target_abs in include_once_set:
                        continue
                    include_once_set.add(target_abs)
                    combined_line = preprocess_file(target_abs, include_once_set, stack, mapping, combined, combined_line)
                    continue
            combined.append(line.rstrip('\n'))
            combined_line += 1

    stack.pop()
    return combined_line

def preprocess_entry(path: str):
    mapping = []
    combined_lines = []
    preprocess_file(path, set(), [], mapping, combined_lines, 1)
    source = "\n".join(combined_lines) + "\n"
    return source, mapping

def run_file(filename: str):
    try:
        source, mapping = preprocess_entry(filename)

        # Tokenize
        lexer = Lexer(source, mapping)
        tokens = lexer.tokenize()

        # Parse
        parser = Parser(tokens)
        ast = parser.parse()

        # Interpret
        interpreter = Interpreter()
        interpreter.interpret(ast)

    except FileNotFoundError:
        print(f"Error: File '{filename}' not found")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)


def run_repl():
    print("Tiny Language REPL")
    print("Type 'exit' to quit")
    print()

    interpreter = Interpreter()

    while True:
        try:
            line = input(">>> ")
            if line.strip() == 'exit':
                break

            if not line.strip():
                continue

            # Tokenize
            lexer = Lexer(line)
            tokens = lexer.tokenize()

            # Parse
            parser = Parser(tokens)
            ast = parser.parse()

            # Interpret
            interpreter.interpret(ast)

        except EOFError:
            break
        except Exception as e:
            print(f"Error: {e}")


def main():
    if len(sys.argv) > 1:
        run_file(sys.argv[1])
    else:
        run_repl()


if __name__ == '__main__':
    main()
