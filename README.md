代码由 AI 实现, 且存下, 供我学习.

```
$ROOT/
├── py/              # Python 解释器实现
├── c_using_llvm/    # C/LLVM 编译器实现
└── examples/        # 示例程序
```

## 语言特性

### 变量定义

```python

var a = '1'; # 一次定义一个, 必须当场赋值
```

### 数据类型
- **int**: 整数
- **float**: 浮点数
- **string**: 字符串
- **array**: 数组
- **dict**: 字典（仅解释器）

对于 array: 
```python
var arr = [1, 2, 3, 4, 5]
var slice = arr[1:4]  # [2, 3, 4]
```

### 操作符
- 算术: `+`, `-`, `*`, `/`, `%`
- 比较: `==`, `!=`, `<`, `<=`, `>`, `>=`
- 逻辑: `&&`, `||`, `!`
- 复合赋值: `+=`, `-=`, `*=`, `/=`

### 控制流
```python
# 条件语句
if (condition) {
    ...
} else if (condition) {
    ...
} else {
    ...
}

# 循环
while (condition) {
    ...
    break
    continue
}
```

### 函数
```python
func factorial(n) {
    if (n <= 1) {
        return 1
    }
    return n * factorial(n - 1)
}

var result = factorial(10)
print(result)  # 输出: 3628800
```

### 内置函数
- `print(x)` - 打印值
- `len(x)` - 获取长度
- `str(x)` - 转换为字符串
- `int(x)` - 转换为整数
- `float(x)` - 转换为浮点数
- `type(x)` - 获取类型
- `append(arr, val)` - 添加到数组
- `input()` - 读取用户输入
- `read(filename)` - 读取文件
- `write(content, filename)` - 写入文件

### 其他

每个语句后可以有分号(;). 如果一行多个语句, 每个语句后应该有分号
