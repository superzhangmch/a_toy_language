# 测试用例编写指南

## 目录结构

- `examples/test/` - 正常功能测试用例（预期成功运行）
- `examples/test_syntax_err/` - 语法错误测试用例（预期失败）

## 测试用例格式

### 1. 基本格式（无标签）

适用于简单的输出验证：

```
# 这是注释
println("hello");
# expect: hello

println("world");
# expect: world
```

- 使用 `# expect: 预期输出` 注释来标记期望的输出
- 测试运行器会检查程序输出是否包含 "预期输出" 字符串
- 多个 expect 按顺序匹配

### 2. 带标签格式（推荐用于复杂测试）

当需要验证特定输出时，使用标签格式：

```
println("output_1", "first");
# expect_1: first

println("output_2", 42);
# expect_2: 42

var x = 100;
println("output_3", x);
# expect_3: 100
```

**格式规则：**
- 输出：`println("output_X", 值)` - X 可以是数字或标识符
- 期望：`# expect_X: 预期值` - X 必须与输出标签匹配
- 测试运行器会在包含 "output_X" 的那一行中查找 "预期值"

### 3. 特殊匹配模式

#### 包含匹配（用于复杂输出）

```
try {
  raise("error message");
} catch e {
  println("output_1", e);
}
# expect_1_has: caught in
# expect_1_has: error message
```

- `# expect_X_has: 子串` - 检查输出是否包含指定子串
- 可以有多个 `_has` 条件，全部匹配才算通过

## 完整示例

### 示例 1：简单功能测试

```
# 测试基本算术运算
var a = 10;
var b = 20;
println(a + b);
# expect: 30

println(a * b);
# expect: 200
```

### 示例 2：异常处理测试

```
# 测试异常捕获
var count = 0;

try {
  raise("test error");
} catch e {
  count = 1;
}

println("output_1", count);
# expect_1: 1

try {
  json_decode('invalid json');
} catch e {
  println("output_2", "caught");
}
# expect_2: caught
```

### 示例 3：循环和条件测试

```
# 测试循环
var sum = 0;
for (i = 1 .. 5) {
  sum += i;
}

println("output_1", sum);
# expect_1: 15

# 测试条件
var result = "";
if (sum > 10) {
  result = "large";
} else {
  result = "small";
}

println("output_2", result);
# expect_2: large
```

## 运行测试

### 运行所有测试

```bash
# 使用解释器后端
python run_tests.py --backend interpreter

# 使用 LLVM 后端
python run_tests.py --backend llvm
```

### 运行单个测试

```bash
# 解释器
./c_using_llvm/interpreter examples/test/your_test.tl

# LLVM 编译执行
./c_using_llvm/codegen_llvm examples/test/your_test.tl
./a.out
```

## 测试用例编写建议

1. **一个文件测试一个功能模块**
   - 例如：`string.tl` 测试字符串操作，`json_valid.tl` 测试 JSON 解析

2. **使用有意义的文件名**
   - 好的：`exception_mixed.tl`, `control_flow.tl`
   - 不好的：`test1.tl`, `my_test.tl`

3. **添加清晰的注释**
   - 在文件开头用 `###` 说明测试目的
   - 在每个测试点前用 `#` 说明测试内容

4. **使用标签避免歧义**
   - 当输出可能重复时，使用 `output_X` 标签
   - 确保每个标签唯一

5. **测试边界情况**
   - 空值、零值、边界值
   - 异常情况和错误处理
   - 嵌套结构和复杂场景

## 常见模式

### 测试函数

```
fun add(a, b) {
  return a + b;
}

println("output_1", add(2, 3));
# expect_1: 5
```

### 测试数组和字典

```
var arr = [1, 2, 3];
println("output_1", len(arr));
# expect_1: 3

var dict = {"key": "value"};
println("output_2", dict["key"]);
# expect_2: value
```

### 测试环境和作用域

```
var global = 10;

fun test() {
  var local = 20;
  return global + local;
}

println("output_1", test());
# expect_1: 30
```

### 测试垃圾回收

```
# 分配大量对象触发 GC
var arr = [];
for (i = 0 .. 200) {
  arr = append(arr, {"index": i});
}

println("output_1", len(arr));
# expect_1: 201
```

## 注意事项

1. **避免输出顺序依赖**
   - GC 可能在任何时候输出调试信息
   - 使用标签而不是依赖行号

2. **期望值必须精确**
   - 数字：`42` 不会匹配 `42.0`
   - 字符串：区分大小写
   - 布尔：`true` / `false`

3. **注释格式严格**
   - `# expect:` 后有一个空格
   - `# expect_X:` 中 X 必须与 `output_X` 匹配
   - `# expect_X_has:` 用于部分匹配

4. **测试文件编码**
   - 使用 UTF-8 编码
   - 避免使用制表符，用空格缩进
