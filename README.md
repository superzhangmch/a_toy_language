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

if / for / while 的 block 中定义的变量, 都是本地的; try/catch 的非本地

### 数据类型
- **bool**: true|false
- **int**: 整数
- **float**: 浮点数
- **string**: 字符串
- **array**: 数组
- **dict**: 字典
- **null**: null

对于 array: 
```python
var arr = [1, 2, 3, 4, 5]
var slice = arr[1:4]  # [2, 3, 4]
```

### 操作符
- 算术: `+`, `-`, `*`, `/`, `%`
  * +: 支持字符串、数组拼接
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
    ...  break/continue ...
}
for (idx = $st .. $end) { ...  break/continue ... } # there should be spaces surrounding .., like for (i = 1 .. 10), not for (i = 1..10)
for (idx = $end .. $st) { ...  break/continue ... }

# 遍历
for (key => val in $dict) {
   .../break/continue/...
}
for (idx => list[idx] in $list) {
   .../break/continue/...
}
```

### 函数
```python
fun factorial(n) {
    if (n <= 1) {
        return 1
    }
    return n * factorial(n - 1)
}

var result = factorial(10)
print(result)  # 输出: 3628800
```

### Class

```python
class Counter {
  var count = 0
  var _hidden = "secret"

  fun init(name, start) {
    this.name = name
    this.count = start
  }
  var name = ""

  fun inc() {
    this.count = this.count + 1
    return this.count
  }

  fun _gen_label()
  {
    return this.name + ": " + str(this.count)
  }
  fun label() {
    return this._gen_label();
  }

  fun copySecret(other) {
    # 同一个类内可以访问私有成员
    return other._hidden
  }
}

var c = new Counter("c1", 1)
println(c.label())
println(c.inc())
println(c.label())
println(c.copySecret(c))
```

### 异常处理
```python
try {
  ... code ... # 不引入新的变量scope
} catch err_msg {
  ... code ... # 不引入新的变量scope
}

# how to raise an exception
raise('err-msg')
assert(false, 'err-msg')
```

### include/include_once
```
include $code_file
include_once $code_file
```

就像 c 的#include, 直接把代码片段插入, 仿佛原文件就是插入后的.

### 内置函数

- `print(x, y, ..)` - 打印
- `println(x, ..)` - 打印同时换行
- `p(x, ..)` - same as println

- `len(x)` - 获取长度
- `append(arr, val)` - 添加到数组
- `remove(dict_or_list, dict_key_or_list_idx)` - return true if dict_key_or_list_idx exists, or else return false

类型:
- `str(x)` - 转换为字符串
- `int(x)` - 转换为整数
- `float(x)` - 转换为浮点数
- `type(x)` - 获取类型

字符串:
- `str_split(str, seperator)`
- `str_join(str_list, seperator)`
- `str_trim(str_list, [chars_to_trim])`
- `str_format("%d-%s", a, b)`

文件:
- `file_size(filename)`
- `file_exist(filename)`
- `file_read(filename)` - 读取文件
- `file_write(content, filename)` - 写入文件
- `file_append(content, filename)`

正则:
- `regexp_match(regexp, str)`
- `regexp_find(regexp, str)` - 返回正则中的括号指定的匹配的字符串, 返回字符串数组, 找不到匹配返回 []. 多个匹配只返回第一个
- `regexp_replace(regexp, str, replace_with)`

json:
- `json_encode(list|dict|int|..)`
- `json_decode(json_str) => list|dict|int|..)` 
  - json_str: pseudo-json, 单双引号都行, list或dict最后一个元素后可以有逗号
  - 如果届时失败, 抛异常

数学:
- `ceil|floor|round|sin|cos|asin|acos|log|exp|sqrt(num)`
- `pow(num1, num2)`
- `random([arg1, arg2])`

系统级:
- `input()` - 读取用户输入
- `cmd_args()` - 获得命令行参数. 获得不带脚本名的所有命令行参数到数组里
- `gc_run()` - 垃圾回收
- `gc_stat()` - 垃圾回收信息统计

note: llvm 模式, 复杂函数是怎么编译成 binary 的? c 语言实现并编译成 bin, 然后 llvm 直接调用 c 实现

### 其他

- 每个语句后可以有分号(;). 如果一行多个语句, 每个语句后应该有分号
- 注释: '#'
