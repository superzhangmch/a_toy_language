# C/LLVM 编译器实现

包含三个编译器：
- C 解释器
- C 转译编译器: AST => C code => gcc 编译得到 bin (这个不支持所有功能)
- LLVM 后端编译器: AST => LLVM IR => llvm 编译得到 bin

(补充说明: 所以三个, 是因为让 AI 直接生成可编译成 binary 的, 它居然给包了个 gcc 实现; 后来只好让它弄了个 llvm 的)

## 文件说明

### 核心组件
- `core/tiny.l` - Flex 词法分析器定义
- `core/tiny.y` - Bison 语法分析器定义
- `core/ast.h` - AST 节点定义和操作
- `core/ast.c` - AST 节点定义和操作

### (1). 解释器
- `interpreter.h/interpreter.c` - C 解释器实现
- `interpreter_main.c` - 解释器驱动程序

### (2). C 转译编译器
- `c_codegen.h/c_codegen.c` - C 代码生成器
- `c_codegen_main.c` - C 转译编译器驱动

### (3). LLVM 后端编译器
- `codegen_llvm.h/codegen_llvm.c` - LLVM IR 代码生成器
- `codegen_llvm_main.c` - LLVM 编译器驱动

----

### 构建系统
- `Makefile` - 构建配置

## 构建

```bash
make               # 构建所有三个编译器
make interpreter   # 只构建 C 解释器
make c_codegen     # 只构建 C 转译编译器
make codegen_llvm  # 只构建 LLVM 编译器
make clean         # 清理构建文件
```

## 使用方法

### 1. C 解释器 (`interpreter`)

```bash
./interpreter program.tl
```

### 2. C 转译编译器 (`c_codegen`)

将 Tiny 代码转译为 C，然后用 GCC 编译(Dict 功能不支持):

```bash
./c_codegen program.tl -o myprogram
./myprogram
```

**架构**:
```
Tiny → C 代码 → GCC → 机器码
```


### 3. LLVM 后端编译器 (`codegen_llvm`) 

将 Tiny 代码编译为 LLVM IR，然后用 LLVM 编译

```bash
# 编译并运行
./codegen_llvm program.tl -o myprogram
./myprogram

# 只生成 LLVM IR（用于学习/调试）
./codegen_llvm program.tl --emit-llvm -o program.ll
cat program.ll
```

**架构**:
```
Tiny → LLVM IR → LLVM 优化 → clang → 机器码
```

**特点**:
- ✅ 现代编译器架构
- ✅ 强大的 LLVM 优化器
- ✅ 支持所有 LLVM 目标（ARM64, x86-64, RISC-V, etc）
- ✅ 可读的中间表示（LLVM IR）
- ✅ 与 Rust, Swift, Clang 使用相同的后端
- ⚠️ 需要安装 LLVM

## 依赖安装

### macOS
```bash
brew install flex bison llvm
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"
```

## LLVM IR 示例

输入 Tiny 代码:
```python
func add(a, b) {
    return a + b
}
var result = add(5, 3)
print(result)
```

生成的 LLVM IR:
```llvm
define %Value @add(%Value %param_a, %Value %param_b) {
  %a = alloca %Value
  store %Value %param_a, %Value* %a
  %b = alloca %Value
  store %Value %param_b, %Value* %b
  %t1 = load %Value, %Value* %a
  %t2 = load %Value, %Value* %b
  %t0 = call %Value @binary_op(%Value %t1, i32 0, %Value %t2)
  ret %Value %t0
}

define i32 @main() {
  %t4 = call %Value @make_int(i64 5)
  %t5 = call %Value @make_int(i64 3)
  %t3 = call %Value @add(%Value %t4, %Value %t5)
  call void @print_value(%Value %t3)
  ret i32 0
}
```

## 调试技巧

### 查看生成的 C 代码

修改 `c_codegen_main.c`，注释掉 `unlink(c_file)`:

```c
// unlink(c_file);  // 注释掉这行
```

然后查看生成的 C 文件：
```bash
./c_codegen program.tl
cat /tmp/tiny_*.c
```

### 查看 LLVM IR

```bash
./codegen_llvm program.tl --emit-llvm -o program.ll
cat program.ll

# 验证 IR 是否有效
llvm-as program.ll -o program.bc
llvm-dis program.bc -o -
```

### 查看优化后的 IR

```bash
# 生成 IR
./codegen_llvm program.tl --emit-llvm -o program.ll

# 优化 IR
opt -O2 program.ll -S -o program_opt.ll

# 对比优化前后
diff program.ll program_opt.ll
```

### 查看生成的汇编

```bash
# 从 LLVM IR 生成汇编
llc program.ll -o program.s
cat program.s
```

## 测试

```bash
# 运行所有测试
make test        # Python 解释器
make test-llvm   # LLVM 编译器
```
