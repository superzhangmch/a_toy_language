#!/usr/bin/env python3
"""
Main entry point for the tiny language compiler/interpreter
"""

import sys
from lexer import Lexer
from parser import Parser
from interpreter import Interpreter


def run_file(filename: str):
    try:
        with open(filename, 'r') as f:
            source = f.read()

        # Tokenize
        lexer = Lexer(source)
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
