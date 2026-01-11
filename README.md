自定义一个结合 python / c, 像 js 的玩具语言, 代码由 AI 实现, 且存于此, 供我学习.

```
$ROOT/
├── py/              # Python 来实现它(解释执行)
├── c_using_llvm/    # C/LLVM 来实现
└── examples/        # 示例程序
```

## 语言特性

### 变量定义

```python

var a = '1'; # 一次定义一个, 必须当场赋值
```

### 数据类型
- **int**: 整数
- **bool**: true|false
- **float**: 浮点数
- **string**: 字符串
- **array**: 数组
- **dict**: 字典

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
- in 语法(返回 true | false): 
```python
"substr" in "string"
"ele" in ["ele", "abc", 11]
"k0" in {"k0": 1, "k1": "a"}
```

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
    break/continue
}

# 遍历

foreach (key => val in $dict) {
   .../break/continue/...
}

foreach (idx => list[idx] in $list) {
   .../break/continue/...
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

- `print(x, y, ..)` - 打印
- `println(x, ..)` - 打印同时换行

- `str(x)` - 转换为字符串
- `int(x)` - 转换为整数
- `float(x)` - 转换为浮点数
- `type(x)` - 获取类型
- `len(x)` - 获取长度
- `append(arr, val)` - 添加到数组

- `input()` - 读取用户输入
- `read(filename)` - 读取文件
- `write(content, filename)` - 写入文件

- `regexp_match(regexp, str)`
- `regexp_find(regexp, str)` - 返回正则中的括号指定的匹配的字符串, 返回字符串数组, 找不到匹配返回 []. 多个匹配只返回第一个
- `regexp_replace(regexp, str, replace_with)`

- `str_split(str, seperator)`
- `str_join(str_list, seperator)`

- `cmd_args()` - 获得命令行参数

note: llvm 模式, 复杂函数是怎么编译成 binary 的? c 语言实现并编译成 bin, 然后 llvm 直接调用 c 实现


### 其他

- 每个语句后可以有分号(;). 如果一行多个语句, 每个语句后应该有分号
- 注释: '#'
