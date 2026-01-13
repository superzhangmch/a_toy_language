### test operators
### - "*+-*/" works for both int and float
###   * int op float => float;
### - op="+": it can concat strings or list
### - cmp `==`, `!=`, `<`, `<=`, `>`, `>=`: works for strings / numbers / bool / null
###   * true > false
###   * str cmp: use the dictionary order
### - 逻辑: `&&`, `||`, `!`
### - 赋值: `=`, `+=`, `-=`, `*=`, `/=`: 除了对数字的操作外:
###   * (1). += works for str/list;
###   * (2). arr[idx] 能用; dict[key] 也能用

# arithmetic and mixed numeric types
var a = 2 + 3;
var b = 7 - 2;
var c = 2 * 3;
var d = float(7) / 2;
var e = float(3) + float(15) / 10;
println("output_1", a, b, c, d, e);
println("output_13", 7 % 3, float(7) % 2);

# precedence and grouping
var p1 = 2 + 3 * 4;        # 2 + 12 = 14
var p2 = (2 + 3) * 4;      # 5 * 4 = 20
var p3 = 10 - 6 / 2;       # 10 - 3 = 7
var p4 = (10 - 6) / 2;     # 4 / 2 = 2
println("output_12", p1, p2, p3, p4);

# concat strings and lists
println("output_2", "he" + "llo");
var la = [1, 2] + [3];
println("output_3", la);

# comparisons: numbers, bool, strings, null
println("output_4", int(3 == 3), int(3 != 2), int(3 > 2), int(2 <= 2));
println("output_5", int(true > false), int(false < true));
println("output_6", int("b" > "a"), int("aa" < "ab"));
println("output_7", int(null == null), int(null != 0));

# logical ops (keywords and !)
println("output_8", int(true and false), int(true or false), int(!false), int(!1));

# compound assignment with numbers
var x = 5;
x += 2;
x -= 1;
x *= 2;
x /= 3;
println("output_9", x);

# compound assignment on list and elements/dict
var arr = [1];
arr += [2, 3];
arr[0] += 10;
println("output_10", arr);

var m = {"k": 1};
m["k"] += 4;
println("output_11", m["k"]);

# expect_1: 5 5 6 3.5 4.5
# expect_2: hello
# expect_3: [1, 2, 3]
# expect_4: 1 1 1 1
# expect_5: 1 1
# expect_6: 1 1
# expect_7: 1 1
# expect_8: 0 1 1 0
# expect_9: 4
# expect_10: [11, 2, 3]
# expect_11: 5
# expect_12: 14 20 7 2
# expect_13: 1 1
