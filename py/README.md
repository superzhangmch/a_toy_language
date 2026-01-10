# Python 解释器实现

纯 Python 实现的 Tiny 语言解释器。

## 文件说明

- `lexer.py` - 词法分析器（Tokenizer）
- `parser.py` - 语法分析器（生成 AST）
- `ast_nodes.py` - AST 节点定义
- `interpreter.py` - 解释器（执行 AST）
- `main.py` - 主程序入口

## 使用方法

```bash
python3 main.py program.tl
```

## 架构

```
program.tl → Lexer → Tokens → Parser → AST → Interpreter → 执行
```

## 示例

```bash
# 运行阶乘程序
python3 main.py ../examples/test_fact.tl

# 输出: 3628800
```
