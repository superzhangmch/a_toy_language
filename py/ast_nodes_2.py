"""
AST Node definitions for the tiny language
"""

from dataclasses import dataclass
from typing import List, Optional, Any


@dataclass
class ASTNode:
    pass


@dataclass
class Program(ASTNode):
    statements: List[ASTNode]


@dataclass
class IntLiteral(ASTNode):
    value: int


@dataclass
class FloatLiteral(ASTNode):
    value: float


@dataclass
class StringLiteral(ASTNode):
    value: str


@dataclass
class BoolLiteral(ASTNode):
    value: bool


@dataclass
class NullLiteral(ASTNode):
    pass


@dataclass
class TryCatch(ASTNode):
    try_block: List[ASTNode]
    catch_var: str
    catch_block: List[ASTNode]


@dataclass
class Raise(ASTNode):
    expr: ASTNode


@dataclass
class Assert(ASTNode):
    expr: ASTNode
    msg: Optional[ASTNode]


@dataclass
class ArrayLiteral(ASTNode):
    elements: List[ASTNode]


@dataclass
class DictLiteral(ASTNode):
    # {k: v, ..}
    pairs: List[tuple[ASTNode, ASTNode]]  # List of (key, value) pairs


@dataclass
class Identifier(ASTNode):
    name: str


@dataclass
class BinaryOp(ASTNode):
    left: ASTNode
    operator: str
    right: ASTNode


@dataclass
class UnaryOp(ASTNode):
    operator: str
    operand: ASTNode


@dataclass
class IndexAccess(ASTNode):
    object: ASTNode
    index: ASTNode


@dataclass
class SliceAccess(ASTNode):
    object: ASTNode
    start: ASTNode
    end: ASTNode


@dataclass
class VarDeclaration(ASTNode):
    name: str
    value: ASTNode


@dataclass
class Assignment(ASTNode):
    target: ASTNode  # Can be Identifier or IndexAccess
    value: ASTNode


@dataclass
class FunctionDef(ASTNode):
    '''
    func name(params) {
        body
    }
    '''
    name: str
    params: List[str]
    body: List[ASTNode]


@dataclass
class FunctionCall(ASTNode):
    '''
    函数调用: name(arguments)
    '''
    name: str
    arguments: List[ASTNode]


@dataclass
class MethodCall(ASTNode):
    '''
    对象方法调用: obj.method(arguments)
    '''
    object: ASTNode
    method: str
    arguments: List[ASTNode]


@dataclass
class MemberAccess(ASTNode):
    '''
    对象属性访问: obj.member
    '''
    object: ASTNode
    member: str


@dataclass
class Return(ASTNode):
    value: Optional[ASTNode]


@dataclass
class IfStatement(ASTNode):
    '''
    if (condition) {
        then_block
    } else {
        else_block
    }
    '''
    condition: ASTNode                    # if 的判断条件
    then_block: List[ASTNode]             # if 分支代码
    else_block: Optional[List[ASTNode]]   # else 分支代码


@dataclass
class WhileStatement(ASTNode):
    '''
    while (condition) {
        body
    }
    '''
    condition: ASTNode
    body: List[ASTNode]


@dataclass
class ForeachStatement(ASTNode):
    '''
    foreach(key_var => value_var in collection) {
        body
    }
    '''
    key_var: str
    value_var: str
    collection: ASTNode
    body: List[ASTNode]


@dataclass
class Break(ASTNode):
    pass


@dataclass
class Continue(ASTNode):
    pass


@dataclass
class ClassDef(ASTNode):
    '''
    class Name { var fields; func methods() { ... } }
    '''
    name: str
    members: List[VarDeclaration]
    methods: List[FunctionDef]


@dataclass
class NewExpression(ASTNode):
    '''
    new ClassName(args)
    '''
    class_name: str
    arguments: List[ASTNode]
