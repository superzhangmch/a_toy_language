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
class ArrayLiteral(ASTNode):
    elements: List[ASTNode]


@dataclass
class DictLiteral(ASTNode):
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
    name: str
    params: List[str]
    body: List[ASTNode]


@dataclass
class FunctionCall(ASTNode):
    name: str
    arguments: List[ASTNode]


@dataclass
class Return(ASTNode):
    value: Optional[ASTNode]


@dataclass
class IfStatement(ASTNode):
    condition: ASTNode
    then_block: List[ASTNode]
    else_block: Optional[List[ASTNode]]


@dataclass
class WhileStatement(ASTNode):
    condition: ASTNode
    body: List[ASTNode]


@dataclass
class Break(ASTNode):
    pass


@dataclass
class Continue(ASTNode):
    pass
