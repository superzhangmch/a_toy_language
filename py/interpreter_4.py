"""
Interpreter for the tiny language
Executes the AST

通过执行 Interpreter.interpret(): 内部是循环执行 AST 中的各个 statement(语句)
"""

from typing import Any, Dict, List, Optional
from ast_nodes_2 import *


class BreakException(Exception):
    pass


class ContinueException(Exception):
    pass


class ReturnException(Exception):
    def __init__(self, value):
        self.value = value


class Environment:
    '''
    不同作用域的变量怎么管理
    '''
    def __init__(self, parent: Optional['Environment'] = None):
        self.parent = parent                 # 更高一层(更偏全局一些的)变量
        self.variables: Dict[str, Any] = {}  # 本地临时变量

    def define(self, name: str, value: Any):
        # 新建(define)变量
        self.variables[name] = value

    def get(self, name: str) -> Any:
        # 获得变量值
        if name in self.variables:
            return self.variables[name]
        if self.parent:
            return self.parent.get(name)
        raise Exception(f"Undefined variable: {name}")

    def set(self, name: str, value: Any):
        # 修改变量值
        if name in self.variables:
            self.variables[name] = value
        elif self.parent:
            self.parent.set(name, value)
        else:
            raise Exception(f"Undefined variable: {name}")


class Function:
    def __init__(self, name: str, params: List[str], body: List[ASTNode], env: Environment):
        self.name = name
        self.params = params
        self.body = body
        self.env = env       # 维持函数内变量的作用域


class Interpreter:
    def __init__(self):
        self.global_env = Environment()
        self.current_env = self.global_env
        self.setup_builtins()

    def setup_builtins(self):
        '''
        内置函数
        '''
        # Built-in functions
        def builtin_print(*args):
            print(*args)
            return None

        def builtin_len(obj):
            if isinstance(obj, (list, dict, str)):
                return len(obj)
            raise Exception(f"len() not supported for type {type(obj)}")

        def builtin_int(val):
            return int(val)

        def builtin_float(val):
            return float(val)

        def builtin_str(val):
            return str(val)

        def builtin_bool(val):
            return bool(val)

        def builtin_type(val):
            if isinstance(val, bool):
                return "bool"
            elif isinstance(val, int):
                return "int"
            elif isinstance(val, float):
                return "float"
            elif isinstance(val, str):
                return "string"
            elif isinstance(val, list):
                return "array"
            elif isinstance(val, dict):
                return "dict"
            elif isinstance(val, Function):
                return "function"
            return "unknown"

        def builtin_input(prompt=""):
            return input(prompt)

        def builtin_range(*args):
            return list(range(*args))

        def builtin_append(arr, val):
            if not isinstance(arr, list):
                raise Exception("append() requires an array")
            arr.append(val)
            return None

        def builtin_pop(arr, idx=-1):
            if not isinstance(arr, list):
                raise Exception("pop() requires an array")
            return arr.pop(idx)

        def builtin_keys(d):
            if not isinstance(d, dict):
                raise Exception("keys() requires a dict")
            return list(d.keys())

        def builtin_values(d):
            if not isinstance(d, dict):
                raise Exception("values() requires a dict")
            return list(d.values())

        def builtin_read(filename):
            """Read file contents and return as string"""
            try:
                with open(filename, 'r') as f:
                    return f.read()
            except Exception as e:
                raise Exception(f"Error reading file '{filename}': {e}")

        def builtin_write(content, filename):
            """Write string content to file"""
            try:
                with open(filename, 'w') as f:
                    f.write(str(content))
                return None
            except Exception as e:
                raise Exception(f"Error writing to file '{filename}': {e}")

        self.global_env.define('print', builtin_print)
        self.global_env.define('len', builtin_len)
        self.global_env.define('int', builtin_int)
        self.global_env.define('float', builtin_float)
        self.global_env.define('str', builtin_str)
        self.global_env.define('bool', builtin_bool)
        self.global_env.define('type', builtin_type)
        self.global_env.define('input', builtin_input)
        self.global_env.define('range', builtin_range)
        self.global_env.define('append', builtin_append)
        self.global_env.define('pop', builtin_pop)
        self.global_env.define('keys', builtin_keys)
        self.global_env.define('values', builtin_values)
        self.global_env.define('read', builtin_read)
        self.global_env.define('write', builtin_write)

    def interpret(self, program: Program):
        '''
        遍历执行每个 statement, 从而完成程序执行. note: 一个循环是作为一个 statement 出现的.
        每个 statement 会根据解析出的 AST, 而内部包含子 statement, 形成了层层嵌套的树结构
        '''
        for statement in program.statements:
            self.eval_statement(statement)

    def eval_statement(self, node: ASTNode) -> Any:
        if isinstance(node, VarDeclaration):
            # 变量定义
            value = self.eval_expression(node.value)
            self.current_env.define(node.name, value)

        elif isinstance(node, Assignment):
            # 变量赋值
            value = self.eval_expression(node.value)
            if isinstance(node.target, Identifier):
                self.current_env.set(node.target.name, value)
            elif isinstance(node.target, IndexAccess):
                obj = self.eval_expression(node.target.object)
                index = self.eval_expression(node.target.index)
                if isinstance(obj, list):
                    if not isinstance(index, int):
                        raise Exception("Array index must be an integer")
                    obj[index] = value
                elif isinstance(obj, dict):
                    if not isinstance(index, str):
                        raise Exception("Dictionary key must be a string")
                    obj[index] = value
                elif isinstance(obj, str):
                    raise Exception("Strings are immutable")
                else:
                    raise Exception(f"Cannot index type {type(obj)}")

        elif isinstance(node, FunctionDef):
            # 函数定义
            func = Function(node.name, node.params, node.body, self.current_env)
            self.current_env.define(node.name, func)

        elif isinstance(node, Return):
            value = None if node.value is None else self.eval_expression(node.value)
            raise ReturnException(value)

        elif isinstance(node, IfStatement):
            # if-else-then: 调用宿主语言的 if/else/then
            condition = self.eval_expression(node.condition)
            if self.is_truthy(condition):
                for stmt in node.then_block:
                    self.eval_statement(stmt)
            elif node.else_block:
                for stmt in node.else_block:
                    self.eval_statement(stmt)

        elif isinstance(node, WhileStatement):
            # while 循环: 调用宿主语言的 while 完成
            while True:
                condition = self.eval_expression(node.condition)
                if not self.is_truthy(condition):
                    break
                try:
                    for stmt in node.body:
                        self.eval_statement(stmt)
                except BreakException:
                    break
                except ContinueException:
                    continue

        elif isinstance(node, ForeachStatement):
            # foreach 循环: 遍历数组或字典
            collection = self.eval_expression(node.collection)

            if isinstance(collection, list):
                # Array iteration
                try:
                    for idx, value in enumerate(collection):
                        self.current_env.define(node.key_var, idx)
                        self.current_env.define(node.value_var, value)
                        try:
                            for stmt in node.body:
                                self.eval_statement(stmt)
                        except ContinueException:
                            continue
                except BreakException:
                    pass
            elif isinstance(collection, dict):
                # Dict iteration
                try:
                    for key, value in collection.items():
                        self.current_env.define(node.key_var, key)
                        self.current_env.define(node.value_var, value)
                        try:
                            for stmt in node.body:
                                self.eval_statement(stmt)
                        except ContinueException:
                            continue
                except BreakException:
                    pass
            else:
                raise RuntimeError(f"Cannot iterate over {type(collection)}")

        elif isinstance(node, Break):
            raise BreakException()

        elif isinstance(node, Continue):
            raise ContinueException()

        elif isinstance(node, FunctionCall):
            # 函数调用
            return self.eval_expression(node)

        else:
            raise Exception(f"Unknown statement type: {type(node)}")

    def eval_expression(self, node: ASTNode) -> Any:
        if isinstance(node, IntLiteral):
            return node.value

        elif isinstance(node, FloatLiteral):
            return node.value

        elif isinstance(node, StringLiteral):
            return node.value

        elif isinstance(node, BoolLiteral):
            return node.value

        elif isinstance(node, ArrayLiteral):
            return [self.eval_expression(elem) for elem in node.elements]

        elif isinstance(node, DictLiteral):
            result = {}
            for key_node, value_node in node.pairs:
                key = self.eval_expression(key_node)
                if not isinstance(key, str):
                    raise Exception("Dictionary keys must be strings")
                value = self.eval_expression(value_node)
                result[key] = value
            return result

        elif isinstance(node, Identifier):
            return self.current_env.get(node.name)

        elif isinstance(node, BinaryOp):
            left = self.eval_expression(node.left)
            right = self.eval_expression(node.right)
            return self.eval_binary_op(left, node.operator, right)

        elif isinstance(node, UnaryOp):
            operand = self.eval_expression(node.operand)
            return self.eval_unary_op(node.operator, operand)

        elif isinstance(node, IndexAccess):
            obj = self.eval_expression(node.object)
            index = self.eval_expression(node.index)

            if isinstance(obj, list):
                if not isinstance(index, int):
                    raise Exception("Array index must be an integer")
                if index < 0 or index >= len(obj):
                    raise Exception(f"Array index out of bounds: {index}")
                return obj[index]

            elif isinstance(obj, dict):
                if not isinstance(index, str):
                    raise Exception("Dictionary key must be a string")
                if index not in obj:
                    raise Exception(f"Dictionary key not found: {index}")
                return obj[index]

            elif isinstance(obj, str):
                if not isinstance(index, int):
                    raise Exception("String index must be an integer")
                if index < 0 or index >= len(obj):
                    raise Exception(f"String index out of bounds: {index}")
                return obj[index]

            else:
                raise Exception(f"Cannot index type {type(obj)}")

        elif isinstance(node, SliceAccess):
            obj = self.eval_expression(node.object)
            start = self.eval_expression(node.start)
            end = self.eval_expression(node.end)

            if not isinstance(start, int) or not isinstance(end, int):
                raise Exception("Slice indices must be integers")

            if isinstance(obj, list) or isinstance(obj, str):
                return obj[start:end]
            else:
                raise Exception(f"Cannot slice type {type(obj)}")

        elif isinstance(node, FunctionCall):
            func = self.current_env.get(node.name)

            # Evaluate arguments
            args = [self.eval_expression(arg) for arg in node.arguments]

            # Built-in function: 有一些函数, 可以直接用宿主语言的函数来实现
            if callable(func) and not isinstance(func, Function):
                return func(*args)

            # User-defined function: 用户自定义函数, 需要逐语句执行
            if isinstance(func, Function):
                if len(args) != len(func.params):
                    raise Exception(f"Function {func.name} expects {len(func.params)} arguments, got {len(args)}")

                # Create new environment for function
                func_env = Environment(func.env)
                for param, arg in zip(func.params, args): # 把函数参数变量, 放入 func_env(而func_env乃继承自上一层的变量作用域)
                    func_env.define(param, arg)

                # Save and switch environment
                prev_env = self.current_env
                self.current_env = func_env

                try:
                    for stmt in func.body:
                        self.eval_statement(stmt)         # 循环逐语句执行用户自定义函数中的语句
                    return None
                except ReturnException as e:
                    return e.value
                finally:
                    self.current_env = prev_env           # 还原函数执行前的环境

            raise Exception(f"{node.name} is not a function")

        else:
            raise Exception(f"Unknown expression type: {type(node)}")

    def eval_binary_op(self, left: Any, op: str, right: Any) -> Any:
        if op == '+':
            if isinstance(left, str) or isinstance(right, str):
                return str(left) + str(right)
            return left + right

        elif op == '-':
            return left - right

        elif op == '*':
            return left * right

        elif op == '/':
            if right == 0:
                raise Exception("Division by zero")
            if isinstance(left, int) and isinstance(right, int):
                return left // right
            return left / right

        elif op == '%':
            if right == 0:
                raise Exception("Modulo by zero")
            return left % right

        elif op == '==':
            return left == right

        elif op == '!=':
            return left != right

        elif op == '<':
            return left < right

        elif op == '<=':
            return left <= right

        elif op == '>':
            return left > right

        elif op == '>=':
            return left >= right

        elif op == 'and':
            return self.is_truthy(left) and self.is_truthy(right)

        elif op == 'or':
            return self.is_truthy(left) or self.is_truthy(right)

        else:
            raise Exception(f"Unknown binary operator: {op}")

    def eval_unary_op(self, op: str, operand: Any) -> Any:
        if op == '-':
            return -operand
        elif op == 'not':
            return not self.is_truthy(operand)
        else:
            raise Exception(f"Unknown unary operator: {op}")

    def is_truthy(self, value: Any) -> bool:
        if isinstance(value, bool):
            return value
        if value is None:
            return False
        if isinstance(value, (int, float)):
            return value != 0
        if isinstance(value, str):
            return len(value) > 0
        if isinstance(value, (list, dict)):
            return len(value) > 0
        return True
