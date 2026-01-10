"""
Parser for the tiny language
Converts tokens into an Abstract Syntax Tree (AST)

做了什么: 对 lexer 解析出的 token list, 做语法 parse. 输出语法树(AST)
- 通过调用 Parser.parse(..)
- 所用算法: 从顶往下, 递归下降
- fun def, if else, while 这三者的处理, 可以看出递归下降法怎么做的
"""

from typing import List, Optional
from lexer_0 import Token, TokenType
from ast_nodes_2 import *


class Parser:
    def __init__(self, tokens: List[Token]):
        self.tokens = tokens
        self.pos = 0

    def error(self, msg: str):
        """ parse 出错时 """
        token = self.current_token()
        raise Exception(f"Parse error at line {token.line}, column {token.column}: {msg}")

    def current_token(self) -> Token:
        if self.pos < len(self.tokens):
            return self.tokens[self.pos]
        return self.tokens[-1]  # Return EOF

    def peek_token(self, offset: int = 1) -> Token:
        """
        没用到该函数
        """
        pos = self.pos + offset
        if pos < len(self.tokens):
            return self.tokens[pos]
        return self.tokens[-1]  # Return EOF

    def advance(self) -> Token:
        token = self.current_token()
        if self.pos < len(self.tokens) - 1:
            self.pos += 1
        return token

    def expect(self, token_type: TokenType) -> Token:
        token = self.current_token()
        if token.type != token_type:
            self.error(f"Expected {token_type}, got {token.type}")
        return self.advance()

    def parse(self) -> Program:
        """
        解析成一个一个 statements(语句): 本 class 暴露出的其实就是这个函数
        """
        statements = []
        while self.current_token().type != TokenType.EOF:
            stmt = self.parse_statement()
            if stmt:
                statements.append(stmt)
        return Program(statements)

    def parse_statement(self) -> Optional[ASTNode]:
        token = self.current_token()

        stmt = None
        if token.type == TokenType.VAR:
            stmt = self.parse_var_declaration()
        elif token.type == TokenType.FUNC:
            return self.parse_function_def()  # Functions don't have semicolons
        elif token.type == TokenType.RETURN:
            stmt = self.parse_return()
        elif token.type == TokenType.IF:
            return self.parse_if_statement()  # If statements don't have semicolons
        elif token.type == TokenType.WHILE:
            return self.parse_while_statement()  # While loops don't have semicolons
        elif token.type == TokenType.BREAK:
            self.advance()
            stmt = Break()
        elif token.type == TokenType.CONTINUE:
            self.advance()
            stmt = Continue()
        elif token.type == TokenType.IDENTIFIER:
            # Parse as postfix expression first, then check for assignment/compound assignment
            postfix = self.parse_postfix_expression()
            if self.current_token().type == TokenType.ASSIGN:
                self.advance()
                value = self.parse_expression()
                stmt = Assignment(postfix, value)
            elif self.current_token().type in (TokenType.PLUS_ASSIGN, TokenType.MINUS_ASSIGN,
                                               TokenType.MULTIPLY_ASSIGN, TokenType.DIVIDE_ASSIGN):
                # Compound assignment: postfix += expr
                op_token = self.current_token()
                self.advance()

                op_map = {
                    TokenType.PLUS_ASSIGN: '+',
                    TokenType.MINUS_ASSIGN: '-',
                    TokenType.MULTIPLY_ASSIGN: '*',
                    TokenType.DIVIDE_ASSIGN: '/'
                }
                operator = op_map[op_token.type]

                rhs = self.parse_expression()
                # Create binary operation: postfix op rhs
                import copy
                value = BinaryOp(copy.deepcopy(postfix), operator, rhs)
                stmt = Assignment(postfix, value)
            else:
                # Just an expression statement (like function call)
                stmt = postfix
        else:
            self.error(f"Unexpected token: {token.type}")

        # Skip optional semicolon after statement
        if stmt and self.current_token().type == TokenType.SEMICOLON:
            self.advance()

        return stmt

    def parse_var_declaration(self) -> VarDeclaration:
        self.expect(TokenType.VAR)
        name_token = self.expect(TokenType.IDENTIFIER)
        self.expect(TokenType.ASSIGN)
        value = self.parse_expression()
        return VarDeclaration(name_token.value, value)

    def parse_function_def(self) -> FunctionDef:
        """
        函数调用
        func $func_name($arg1, $arg2, ..) { ... }
        """
        self.expect(TokenType.FUNC)                          # func 关键词
        name_token = self.expect(TokenType.IDENTIFIER)       # 函数名
        self.expect(TokenType.LPAREN)                        # (

        params = []
        while self.current_token().type != TokenType.RPAREN:
            param_token = self.expect(TokenType.IDENTIFIER)  # 参数
            params.append(param_token.value)
            if self.current_token().type == TokenType.COMMA:
                self.advance()

        self.expect(TokenType.RPAREN)                        # )
        self.expect(TokenType.LBRACE)                        # {

        body = []
        while self.current_token().type != TokenType.RBRACE:
            stmt = self.parse_statement()                    # 函数体的各个语句
            if stmt:
                body.append(stmt)

        self.expect(TokenType.RBRACE)                        # }
        return FunctionDef(name_token.value, params, body)

    def parse_return(self) -> Return:
        self.expect(TokenType.RETURN)
        if self.current_token().type in [TokenType.RBRACE, TokenType.EOF]:
            return Return(None)
        value = self.parse_expression()
        return Return(value)

    def parse_if_statement(self) -> IfStatement:
        '''
        if (..) {..} else if {...}
        if (..) {..} else {...}
        '''
        self.expect(TokenType.IF)                             # if
        self.expect(TokenType.LPAREN)                         # (
        condition = self.parse_expression()                   #   ... 
        self.expect(TokenType.RPAREN)                         # )
        self.expect(TokenType.LBRACE)                         # {

        then_block = []
        while self.current_token().type != TokenType.RBRACE:
            stmt = self.parse_statement()                     #   ... if 分支的各个语句
            if stmt:
                then_block.append(stmt)

        self.expect(TokenType.RBRACE)                         # }

        else_block = None
        if self.current_token().type == TokenType.ELSE:       # else 
            self.advance()
            if self.current_token().type == TokenType.IF:     #    if   也就是如果组成了 else if
                # else if
                else_block = [self.parse_if_statement()]      #    else if, 则是重启了 if (..) {..}, 递归调用 parse_if_statement
            else:
                self.expect(TokenType.LBRACE)                 # {
                else_block = []
                while self.current_token().type != TokenType.RBRACE:
                    stmt = self.parse_statement()
                    if stmt:
                        else_block.append(stmt)
                self.expect(TokenType.RBRACE)                 # }

        return IfStatement(condition, then_block, else_block)

    def parse_while_statement(self) -> WhileStatement:
        """
        while (...) {...}
        """
        self.expect(TokenType.WHILE)               # while 
        self.expect(TokenType.LPAREN)              # (
        condition = self.parse_expression()
        self.expect(TokenType.RPAREN)              # )
        self.expect(TokenType.LBRACE)              # {

        body = []
        while self.current_token().type != TokenType.RBRACE:
            stmt = self.parse_statement()
            if stmt:
                body.append(stmt)

        self.expect(TokenType.RBRACE)              # }
        return WhileStatement(condition, body)

    def parse_expression(self) -> ASTNode:
        return self.parse_logical_or()

    def parse_logical_or(self) -> ASTNode:
        left = self.parse_logical_and()

        while self.current_token().type == TokenType.OR:
            op = self.advance()
            right = self.parse_logical_and()
            left = BinaryOp(left, 'or', right)

        return left

    def parse_logical_and(self) -> ASTNode:
        left = self.parse_equality()

        while self.current_token().type == TokenType.AND:
            op = self.advance()
            right = self.parse_equality()
            left = BinaryOp(left, 'and', right)

        return left

    def parse_equality(self) -> ASTNode:
        left = self.parse_comparison()

        while self.current_token().type in [TokenType.EQ, TokenType.NE]:
            op_token = self.advance()
            op = '==' if op_token.type == TokenType.EQ else '!='
            right = self.parse_comparison()
            left = BinaryOp(left, op, right)

        return left

    def parse_comparison(self) -> ASTNode:
        left = self.parse_additive()

        while self.current_token().type in [TokenType.LT, TokenType.LE, TokenType.GT, TokenType.GE]:
            op_token = self.advance()
            op_map = {
                TokenType.LT: '<',
                TokenType.LE: '<=',
                TokenType.GT: '>',
                TokenType.GE: '>='
            }
            op = op_map[op_token.type]
            right = self.parse_additive()
            left = BinaryOp(left, op, right)

        return left

    def parse_additive(self) -> ASTNode:
        left = self.parse_multiplicative()

        while self.current_token().type in [TokenType.PLUS, TokenType.MINUS]:
            op_token = self.advance()
            op = '+' if op_token.type == TokenType.PLUS else '-'
            right = self.parse_multiplicative()
            left = BinaryOp(left, op, right)

        return left

    def parse_multiplicative(self) -> ASTNode:
        left = self.parse_unary()

        while self.current_token().type in [TokenType.MULTIPLY, TokenType.DIVIDE, TokenType.MODULO]:
            op_token = self.advance()
            if op_token.type == TokenType.MULTIPLY:
                op = '*'
            elif op_token.type == TokenType.DIVIDE:
                op = '/'
            else:
                op = '%'
            right = self.parse_unary()
            left = BinaryOp(left, op, right)

        return left

    def parse_unary(self) -> ASTNode:
        if self.current_token().type == TokenType.MINUS:
            self.advance()
            operand = self.parse_unary()
            return UnaryOp('-', operand)
        elif self.current_token().type == TokenType.NOT:
            self.advance()
            operand = self.parse_unary()
            return UnaryOp('not', operand)

        return self.parse_postfix_expression()

    def parse_postfix_expression(self) -> ASTNode:
        expr = self.parse_primary()

        while True:
            if self.current_token().type == TokenType.LBRACKET:
                self.advance()

                # Parse first expression (start index or regular index)
                first_expr = self.parse_expression()

                # Check if this is a slice (has colon)
                if self.current_token().type == TokenType.COLON:
                    self.advance()
                    # Parse end expression (required for arr[a:b])
                    end_expr = self.parse_expression()
                    self.expect(TokenType.RBRACKET)
                    expr = SliceAccess(expr, first_expr, end_expr)
                else:
                    # Regular index access
                    self.expect(TokenType.RBRACKET)
                    expr = IndexAccess(expr, first_expr)
            elif self.current_token().type == TokenType.LPAREN:
                # Function call
                if not isinstance(expr, Identifier):
                    self.error("Only identifiers can be called as functions")
                self.advance()
                arguments = []
                while self.current_token().type != TokenType.RPAREN:
                    arguments.append(self.parse_expression())
                    if self.current_token().type == TokenType.COMMA:
                        self.advance()
                self.expect(TokenType.RPAREN)
                expr = FunctionCall(expr.name, arguments)
            else:
                break

        return expr

    def parse_primary(self) -> ASTNode:
        token = self.current_token()

        if token.type == TokenType.INT:
            self.advance()
            return IntLiteral(token.value)

        elif token.type == TokenType.FLOAT:
            self.advance()
            return FloatLiteral(token.value)

        elif token.type == TokenType.STRING:
            self.advance()
            return StringLiteral(token.value)

        elif token.type == TokenType.TRUE:
            self.advance()
            return BoolLiteral(True)

        elif token.type == TokenType.FALSE:
            self.advance()
            return BoolLiteral(False)

        elif token.type == TokenType.IDENTIFIER:
            self.advance()
            return Identifier(token.value)

        elif token.type == TokenType.LBRACKET:
            return self.parse_array_literal()

        elif token.type == TokenType.LBRACE:
            return self.parse_dict_literal()

        elif token.type == TokenType.LPAREN:
            self.advance()
            expr = self.parse_expression()
            self.expect(TokenType.RPAREN)
            return expr

        else:
            self.error(f"Unexpected token: {token.type}")

    def parse_array_literal(self) -> ArrayLiteral:
        self.expect(TokenType.LBRACKET)
        elements = []

        while self.current_token().type != TokenType.RBRACKET:
            elements.append(self.parse_expression())
            if self.current_token().type == TokenType.COMMA:
                self.advance()

        self.expect(TokenType.RBRACKET)
        return ArrayLiteral(elements)

    def parse_dict_literal(self) -> DictLiteral:
        self.expect(TokenType.LBRACE)
        pairs = []

        while self.current_token().type != TokenType.RBRACE:
            # Key must be a string
            if self.current_token().type != TokenType.STRING:
                self.error("Dictionary keys must be strings")
            key = self.parse_expression()
            self.expect(TokenType.COLON)
            value = self.parse_expression()
            pairs.append((key, value))

            if self.current_token().type == TokenType.COMMA:
                self.advance()

        self.expect(TokenType.RBRACE)
        return DictLiteral(pairs)
