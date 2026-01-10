"""
Lexer for the tiny language
Converts source code into tokens

做了啥: 对于输入的源代码, 处理成一个数组, 元素是 [TokenType, str_value|int_value|None, line_pos:start_line, line_pos:start_column]
"""

from enum import Enum, auto
from dataclasses import dataclass
from typing import List, Optional


class TokenType(Enum):
    # Literals
    INT = auto()
    FLOAT = auto()
    STRING = auto()
    TRUE = auto()
    FALSE = auto()

    # Identifiers and keywords
    IDENTIFIER = auto()
    VAR = auto()
    FUNC = auto()
    RETURN = auto()
    IF = auto()
    THEN = auto()
    ELSE = auto()
    WHILE = auto()
    BREAK = auto()
    CONTINUE = auto()

    # Operators
    PLUS = auto()
    MINUS = auto()
    MULTIPLY = auto()
    DIVIDE = auto()
    MODULO = auto()
    ASSIGN = auto()
    PLUS_ASSIGN = auto()      # +=
    MINUS_ASSIGN = auto()     # -=
    MULTIPLY_ASSIGN = auto()  # *=
    DIVIDE_ASSIGN = auto()    # /=

    # Comparison operators
    EQ = auto()      # ==
    NE = auto()      # !=
    LT = auto()      # <
    LE = auto()      # <=
    GT = auto()      # >
    GE = auto()      # >=

    # Logical operators
    AND = auto()
    OR = auto()
    NOT = auto()

    # Delimiters
    LPAREN = auto()
    RPAREN = auto()
    LBRACE = auto()
    RBRACE = auto()
    LBRACKET = auto()
    RBRACKET = auto()
    COMMA = auto()
    COLON = auto()
    SEMICOLON = auto()

    # Special
    EOF = auto()
    NEWLINE = auto()


@dataclass
class Token:
    type: TokenType
    value: any
    line: int
    column: int


class Lexer:
    def __init__(self, source: str):
        self.source = source
        self.pos = 0
        self.line = 1
        self.column = 1
        self.tokens: List[Token] = []

        self.keywords = {
            'var': TokenType.VAR,
            'func': TokenType.FUNC,
            'return': TokenType.RETURN,
            'if': TokenType.IF,
            'then': TokenType.THEN,
            'else': TokenType.ELSE,
            'while': TokenType.WHILE,
            'break': TokenType.BREAK,
            'continue': TokenType.CONTINUE,
            'true': TokenType.TRUE,
            'false': TokenType.FALSE,
            'and': TokenType.AND,
            'or': TokenType.OR,
            'not': TokenType.NOT,
        }

    def error(self, msg: str):
        raise Exception(f"Lexer error at line {self.line}, column {self.column}: {msg}")

    def peek(self, offset: int = 0) -> Optional[str]:
        pos = self.pos + offset
        if pos < len(self.source):
            return self.source[pos]
        return None

    def advance(self) -> Optional[str]:
        if self.pos < len(self.source):
            char = self.source[self.pos]
            self.pos += 1
            if char == '\n':
                self.line += 1
                self.column = 1
            else:
                self.column += 1
            return char
        return None

    def skip_whitespace(self):
        while self.peek() and self.peek() in ' \t\r\n':
            self.advance()

    def skip_comment(self):
        if self.peek() == '#':
            while self.peek() and self.peek() != '\n':
                self.advance()

    def read_number(self) -> Token:
        start_line = self.line
        start_column = self.column
        num_str = ''
        has_dot = False

        while self.peek() and (self.peek().isdigit() or self.peek() == '.'):
            if self.peek() == '.':
                if has_dot:
                    break
                has_dot = True
            num_str += self.advance()

        if has_dot:
            return Token(TokenType.FLOAT, float(num_str), start_line, start_column)
        else:
            return Token(TokenType.INT, int(num_str), start_line, start_column)

    def read_string(self) -> Token:
        start_line = self.line
        start_column = self.column
        quote = self.advance()  # Skip opening quote
        string_val = ''

        while self.peek() and self.peek() != quote:
            if self.peek() == '\\':
                self.advance()
                next_char = self.advance()
                if next_char == 'n':
                    string_val += '\n'
                elif next_char == 't':
                    string_val += '\t'
                elif next_char == '\\':
                    string_val += '\\'
                elif next_char == quote:
                    string_val += quote
                else:
                    string_val += next_char
            else:
                string_val += self.advance()

        if not self.peek():
            self.error("Unterminated string")

        self.advance()  # Skip closing quote
        return Token(TokenType.STRING, string_val, start_line, start_column)

    def read_identifier(self) -> Token:
        start_line = self.line
        start_column = self.column
        ident = ''

        while self.peek() and (self.peek().isalnum() or self.peek() == '_'):
            ident += self.advance()

        token_type = self.keywords.get(ident, TokenType.IDENTIFIER)
        value = ident if token_type == TokenType.IDENTIFIER else None

        return Token(token_type, value, start_line, start_column)

    def tokenize(self) -> List[Token]:
        while self.pos < len(self.source):
            self.skip_whitespace()

            if self.pos >= len(self.source):
                break

            # Skip comments
            if self.peek() == '#':
                self.skip_comment()
                continue

            start_line = self.line
            start_column = self.column
            char = self.peek()

            # Numbers
            if char.isdigit():
                self.tokens.append(self.read_number())

            # Strings
            elif char in '"\'':
                self.tokens.append(self.read_string())

            # Identifiers and keywords
            elif char.isalpha() or char == '_':
                self.tokens.append(self.read_identifier())

            # Operators and delimiters
            elif char == '+':
                self.advance()
                if self.peek() == '=':
                    self.advance()
                    self.tokens.append(Token(TokenType.PLUS_ASSIGN, None, start_line, start_column))
                else:
                    self.tokens.append(Token(TokenType.PLUS, None, start_line, start_column))

            elif char == '-':
                self.advance()
                if self.peek() == '=':
                    self.advance()
                    self.tokens.append(Token(TokenType.MINUS_ASSIGN, None, start_line, start_column))
                else:
                    self.tokens.append(Token(TokenType.MINUS, None, start_line, start_column))

            elif char == '*':
                self.advance()
                if self.peek() == '=':
                    self.advance()
                    self.tokens.append(Token(TokenType.MULTIPLY_ASSIGN, None, start_line, start_column))
                else:
                    self.tokens.append(Token(TokenType.MULTIPLY, None, start_line, start_column))

            elif char == '/':
                self.advance()
                if self.peek() == '=':
                    self.advance()
                    self.tokens.append(Token(TokenType.DIVIDE_ASSIGN, None, start_line, start_column))
                else:
                    self.tokens.append(Token(TokenType.DIVIDE, None, start_line, start_column))

            elif char == '%':
                self.advance()
                self.tokens.append(Token(TokenType.MODULO, None, start_line, start_column))

            elif char == '=':
                self.advance()
                if self.peek() == '=':
                    self.advance()
                    self.tokens.append(Token(TokenType.EQ, None, start_line, start_column))
                else:
                    self.tokens.append(Token(TokenType.ASSIGN, None, start_line, start_column))

            elif char == '!':
                self.advance()
                if self.peek() == '=':
                    self.advance()
                    self.tokens.append(Token(TokenType.NE, None, start_line, start_column))
                else:
                    self.error(f"Unexpected character: {char}")

            elif char == '<':
                self.advance()
                if self.peek() == '=':
                    self.advance()
                    self.tokens.append(Token(TokenType.LE, None, start_line, start_column))
                else:
                    self.tokens.append(Token(TokenType.LT, None, start_line, start_column))

            elif char == '>':
                self.advance()
                if self.peek() == '=':
                    self.advance()
                    self.tokens.append(Token(TokenType.GE, None, start_line, start_column))
                else:
                    self.tokens.append(Token(TokenType.GT, None, start_line, start_column))

            elif char == '(':
                self.advance()
                self.tokens.append(Token(TokenType.LPAREN, None, start_line, start_column))

            elif char == ')':
                self.advance()
                self.tokens.append(Token(TokenType.RPAREN, None, start_line, start_column))

            elif char == '{':
                self.advance()
                self.tokens.append(Token(TokenType.LBRACE, None, start_line, start_column))

            elif char == '}':
                self.advance()
                self.tokens.append(Token(TokenType.RBRACE, None, start_line, start_column))

            elif char == '[':
                self.advance()
                self.tokens.append(Token(TokenType.LBRACKET, None, start_line, start_column))

            elif char == ']':
                self.advance()
                self.tokens.append(Token(TokenType.RBRACKET, None, start_line, start_column))

            elif char == ',':
                self.advance()
                self.tokens.append(Token(TokenType.COMMA, None, start_line, start_column))

            elif char == ':':
                self.advance()
                self.tokens.append(Token(TokenType.COLON, None, start_line, start_column))

            elif char == ';':
                self.advance()
                self.tokens.append(Token(TokenType.SEMICOLON, None, start_line, start_column))

            else:
                self.error(f"Unexpected character: {char}")

        self.tokens.append(Token(TokenType.EOF, None, self.line, self.column))
        return self.tokens
