"""
Interpreter for the tiny language
Executes the AST

通过执行 Interpreter.interpret(): 内部是循环执行 AST 中的各个 statement(语句)
"""

import sys
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


class ClassValue:
    def __init__(self, name: str, members: List[VarDeclaration], methods: List[FunctionDef], env: Environment):
        self.name = name
        self.members = members
        self.methods = {m.name: m for m in methods}
        self.env = env  # 定义类时所在的作用域，实例化时可见


class ClassInstance:
    def __init__(self, class_value: ClassValue):
        self.class_value = class_value
        self.fields: Dict[str, Any] = {}  # 成员变量存储

class TinyException(Exception):
    def __init__(self, message: str):
        super().__init__(message)
        self.message = message


class Interpreter:
    def __init__(self):
        self.global_env = Environment()
        self.current_env = self.global_env
        self.this_stack: List[ClassInstance] = []  # 跟踪当前方法调用链上的实例，用于权限校验
        self.setup_builtins()

    def setup_builtins(self):
        '''
        内置函数
        '''
        # Built-in functions
        def builtin_print(*args):
            # Print without automatic newline
            print(*args, end='')
            return None

        def builtin_println(*args):
            # Print with automatic newline
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
            elif val is None:
                return "null"
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

        def builtin_remove(obj, key_or_idx):
            if isinstance(obj, list):
                if not isinstance(key_or_idx, int):
                    return False
                if key_or_idx < 0 or key_or_idx >= len(obj):
                    return False
                obj.pop(key_or_idx)
                return True
            if isinstance(obj, dict):
                if not isinstance(key_or_idx, str):
                    return False
                if key_or_idx not in obj:
                    return False
                del obj[key_or_idx]
                return True
            return False

        def builtin_math(op, *args):
            import math, random, builtins
            if not isinstance(op, str):
                raise Exception("math() first arg must be operation string")
            name = op
            if name in ("sin", "cos", "asin", "acos", "log", "exp", "ceil", "floor", "round"):
                if len(args) != 1:
                    raise Exception(f"math({name}) requires 1 argument")
                val = float(args[0])
                if name == "round":
                    return builtins.round(val)
                fn = getattr(math, name)
                return fn(val)
            if name == "pow":
                if len(args) != 2:
                    raise Exception("math(pow) requires 2 arguments")
                return math.pow(float(args[0]), float(args[1]))
            if name == "random":
                if len(args) == 0:
                    return random.random()
                if len(args) == 2:
                    a, b = float(args[0]), float(args[1])
                    return random.uniform(a, b)
                raise Exception("math(random) requires 0 or 2 arguments")
            raise Exception(f"math(): unsupported op {name}")

        def builtin_json_decode(s):
            import json, re
            if not isinstance(s, str):
                raise Exception("json_decode expects a string")
            def normalize(txt: str) -> str:
                txt = re.sub(r"(?m),\s*([}\]])", r"\1", txt)  # remove trailing commas
                txt = re.sub(r"(?i)\\btrue\\b", "true", txt)
                txt = re.sub(r"(?i)\\bfalse\\b", "false", txt)
                txt = re.sub(r"(?i)\\bnull\\b", "null", txt)
                # convert single-quoted strings to double
                txt = re.sub(r"'([^'\\\\]*(?:\\\\.[^'\\\\]*)*)'", lambda m: '\"' + m.group(1).replace('\"', '\\\\\"') + '\"', txt)
                return txt
            try:
                return json.loads(s)
            except Exception:
                try:
                    return json.loads(normalize(s))
                except Exception:
                    raise TinyException("Invalid JSON string")

        def builtin_json_encode(obj):
            import json
            return json.dumps(obj)

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

        def builtin_regexp_match(pattern, text):
            """Check if pattern matches text"""
            import re
            try:
                return 1 if re.search(pattern, text) else 0
            except Exception as e:
                raise Exception(f"Invalid regex pattern '{pattern}': {e}")

        def builtin_regexp_find(pattern, text):
            """Find all matches of pattern in text, returns capture groups if present"""
            import re
            try:
                matches = re.findall(pattern, text)
                # If matches contain tuples (capture groups), flatten them
                if matches and isinstance(matches[0], tuple):
                    # Flatten all tuples into a single list
                    result = []
                    for match in matches:
                        result.extend(match)
                    return result
                return matches
            except Exception as e:
                raise Exception(f"Invalid regex pattern '{pattern}': {e}")

        def builtin_regexp_replace(pattern, text, replacement):
            """Replace all matches of pattern in text with replacement"""
            import re
            try:
                return re.sub(pattern, replacement, text)
            except Exception as e:
                raise Exception(f"Invalid regex pattern '{pattern}': {e}")

        def builtin_str_split(text, separator):
            """Split text by separator"""
            if not isinstance(text, str) or not isinstance(separator, str):
                raise Exception("str_split() requires two string arguments")
            if not separator:
                raise Exception("str_split() separator cannot be empty")
            return text.split(separator)

        def builtin_str_join(arr, separator):
            """Join array elements with separator"""
            if not isinstance(arr, list):
                raise Exception("str_join() requires an array")
            if not isinstance(separator, str):
                raise Exception("str_join() separator must be a string")
            return separator.join(str(x) for x in arr)

        def builtin_cmd_args():
            """Get command line arguments (excluding interpreter and script name)"""
            # sys.argv[0] is 'main.py', sys.argv[1] is the script file
            # sys.argv[2:] are the actual arguments passed to the script
            return sys.argv[2:] if len(sys.argv) > 2 else []

        self.global_env.define('print', builtin_print)
        self.global_env.define('println', builtin_println)
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
        self.global_env.define('remove', builtin_remove)
        self.global_env.define('math', builtin_math)
        self.global_env.define('json_encode', builtin_json_encode)
        self.global_env.define('json_decode', builtin_json_decode)
        self.global_env.define('read', builtin_read)
        self.global_env.define('write', builtin_write)
        self.global_env.define('regexp_match', builtin_regexp_match)
        self.global_env.define('regexp_find', builtin_regexp_find)
        self.global_env.define('regexp_replace', builtin_regexp_replace)
        self.global_env.define('str_split', builtin_str_split)
        self.global_env.define('str_join', builtin_str_join)
        self.global_env.define('cmd_args', builtin_cmd_args)

    def is_internal_access(self, instance: ClassInstance) -> bool:
        """
        是否从同一个类的方法内部访问，用于处理私有成员/方法
        """
        return bool(self.this_stack) and self.this_stack[-1].class_value == instance.class_value

    def get_member(self, instance: ClassInstance, name: str):
        if not isinstance(instance, ClassInstance):
            raise Exception("Member access only valid on class instances")
        is_private = name.startswith('_')
        if is_private and not self.is_internal_access(instance):
            raise Exception(f"Cannot access private member '{name}' of class {instance.class_value.name}")

        if name in instance.fields:
            return instance.fields[name]

        if name in instance.class_value.methods:
            # 返回一个可调用对象，供 obj.method(...) 间接使用
            def bound_method(*args):
                return self.call_method(instance, name, list(args))
            return bound_method

        raise Exception(f"Member '{name}' not found on class {instance.class_value.name}")

    def set_member(self, instance: ClassInstance, name: str, value: Any):
        if not isinstance(instance, ClassInstance):
            raise Exception("Member assignment only valid on class instances")

        is_private = name.startswith('_')
        if is_private and not self.is_internal_access(instance):
            raise Exception(f"Cannot access private member '{name}' of class {instance.class_value.name}")

        if name not in instance.fields:
            raise Exception(f"Member '{name}' not defined on class {instance.class_value.name}")
        instance.fields[name] = value

    def instantiate_class(self, class_value: ClassValue, args: List[Any]) -> ClassInstance:
        instance = ClassInstance(class_value)

        # 实例化时，为成员变量求值
        prev_env = self.current_env
        init_env = Environment(class_value.env)
        init_env.define('this', instance)
        init_env.define('self', instance)
        self.current_env = init_env
        self.this_stack.append(instance)
        try:
            for member in class_value.members:
                instance.fields[member.name] = self.eval_expression(member.value)
        finally:
            self.this_stack.pop()
            self.current_env = prev_env

        # 调用构造函数 init（若存在）
        if 'init' in class_value.methods:
            init_method = class_value.methods['init']
            if len(args) != len(init_method.params):
                raise Exception(f"Constructor for {class_value.name} expects {len(init_method.params)} arguments, got {len(args)}")
            self.call_method(instance, 'init', args)
        elif args:
            raise Exception(f"Class {class_value.name} constructor does not take arguments")

        return instance

    def call_method(self, instance: ClassInstance, method_name: str, args: List[Any]):
        if not isinstance(instance, ClassInstance):
            raise Exception("Method call only valid on class instances")

        method_def = instance.class_value.methods.get(method_name)
        if not method_def:
            raise Exception(f"Method '{method_name}' not found on class {instance.class_value.name}")

        is_private = method_name.startswith('_')
        if is_private and not self.is_internal_access(instance):
            raise Exception(f"Cannot access private method '{method_name}' of class {instance.class_value.name}")

        if len(args) != len(method_def.params):
            raise Exception(f"Method {method_name} expects {len(method_def.params)} arguments, got {len(args)}")

        func_env = Environment(instance.class_value.env)
        func_env.define('this', instance)
        func_env.define('self', instance)
        for param, arg in zip(method_def.params, args):
            func_env.define(param, arg)

        prev_env = self.current_env
        self.current_env = func_env
        self.this_stack.append(instance)
        try:
            for stmt in method_def.body:
                self.eval_statement(stmt)
            return None
        except ReturnException as e:
            return e.value
        finally:
            self.this_stack.pop()
            self.current_env = prev_env

    def interpret(self, program: Program):
        '''
        遍历执行每个 statement, 从而完成程序执行. note: 一个循环是作为一个 statement 出现的.
        每个 statement 会根据解析出的 AST, 而内部包含子 statement, 形成了层层嵌套的树结构
        '''
        for statement in program.statements:
            self.eval_statement(statement)

    def _execute_block(self, statements):
        """Execute statements in a new block scope."""
        prev_env = self.current_env
        self.current_env = Environment(prev_env)
        try:
            for stmt in statements:
                self.eval_statement(stmt)
        finally:
            self.current_env = prev_env

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
            elif isinstance(node.target, MemberAccess):
                obj = self.eval_expression(node.target.object)
                self.set_member(obj, node.target.member, value)

        elif isinstance(node, FunctionDef):
            # 函数定义
            func = Function(node.name, node.params, node.body, self.current_env)
            self.current_env.define(node.name, func)

        elif isinstance(node, ClassDef):
            class_value = ClassValue(node.name, node.members, node.methods, self.current_env)
            self.current_env.define(node.name, class_value)

        elif isinstance(node, Return):
            value = None if node.value is None else self.eval_expression(node.value)
            raise ReturnException(value)

        elif isinstance(node, IfStatement):
            # if-else-then: 调用宿主语言的 if/else/then
            condition = self.eval_expression(node.condition)
            if self.is_truthy(condition):
                self._execute_block(node.then_block)
            elif node.else_block:
                self._execute_block(node.else_block)

        elif isinstance(node, WhileStatement):
            # while 循环: 调用宿主语言的 while 完成
            while True:
                condition = self.eval_expression(node.condition)
                if not self.is_truthy(condition):
                    break
                try:
                    self._execute_block(node.body)
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
                        try:
                            prev_env = self.current_env
                            iter_env = Environment(prev_env)
                            iter_env.define(node.key_var, idx)
                            iter_env.define(node.value_var, value)
                            self.current_env = iter_env
                            for stmt in node.body:
                                self.eval_statement(stmt)
                        except ContinueException:
                            continue
                        finally:
                            self.current_env = prev_env
                except BreakException:
                    pass
            elif isinstance(collection, dict):
                # Dict iteration
                try:
                    for key, value in collection.items():
                        try:
                            prev_env = self.current_env
                            iter_env = Environment(prev_env)
                            iter_env.define(node.key_var, key)
                            iter_env.define(node.value_var, value)
                            self.current_env = iter_env
                            for stmt in node.body:
                                self.eval_statement(stmt)
                        except ContinueException:
                            continue
                        finally:
                            self.current_env = prev_env
                except BreakException:
                    pass
            else:
                raise RuntimeError(f"Cannot iterate over {type(collection)}")

        elif isinstance(node, TryCatch):
            try:
                for stmt in node.try_block:
                    self.eval_statement(stmt)
            except TinyException as ex:
                prev_env = self.current_env
                catch_env = Environment(prev_env)
                catch_env.define(node.catch_var, ex.message)
                self.current_env = catch_env
                for stmt in node.catch_block:
                    self.eval_statement(stmt)
                self.current_env = prev_env

        elif isinstance(node, Raise):
            msg_val = self.eval_expression(node.expr)
            msg = self.to_string(msg_val)
            loc = f"{getattr(node, 'file', '<input>')}:{getattr(node, 'line', 0)}"
            raise TinyException(f"{loc}: {msg}")

        elif isinstance(node, Assert):
            cond = self.eval_expression(node.expr)
            if not self.is_truthy(cond):
                msg_val = self.eval_expression(node.msg) if node.msg is not None else "Assertion failed"
                msg = self.to_string(msg_val)
                loc = f"{getattr(node, 'file', '<input>')}:{getattr(node, 'line', 0)}"
                raise TinyException(f"{loc}: {msg}")

        elif isinstance(node, Break):
            raise BreakException()

        elif isinstance(node, Continue):
            raise ContinueException()

        elif isinstance(node, FunctionCall):
            # 函数调用
            return self.eval_expression(node)

        elif isinstance(node, MethodCall):
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

        elif isinstance(node, NullLiteral):
            return None

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

        elif isinstance(node, MemberAccess):
            obj = self.eval_expression(node.object)
            return self.get_member(obj, node.member)

        elif isinstance(node, BinaryOp):
            left = self.eval_expression(node.left)
            right = self.eval_expression(node.right)
            # Array concatenation with +
            if node.operator == '+' and isinstance(left, list) and isinstance(right, list):
                return left + right
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

        elif isinstance(node, MethodCall):
            obj = self.eval_expression(node.object)
            args = [self.eval_expression(arg) for arg in node.arguments]
            return self.call_method(obj, node.method, args)

        elif isinstance(node, NewExpression):
            class_value = self.current_env.get(node.class_name)
            if not isinstance(class_value, ClassValue):
                raise Exception(f"{node.class_name} is not a class")
            args = [self.eval_expression(arg) for arg in node.arguments]
            return self.instantiate_class(class_value, args)

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

        elif op == 'in':
            if isinstance(right, list):
                return any(left == elem for elem in right)
            elif isinstance(right, dict):
                if not isinstance(left, str):
                    raise Exception("Dictionary keys for 'in' must be strings")
                return left in right
            elif isinstance(right, str):
                if not isinstance(left, str):
                    raise Exception("Substring for 'in' must be a string")
                return left in right
            else:
                raise Exception(f"'in' not supported for {type(right)}")

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

    def to_string(self, value: Any) -> str:
        try:
            return str(value)
        except Exception:
            return "<object>"
