### test types
### 1. type trans: int(..), str(int|float|true|false|null|dict|json), float(..)
### 2. len(str|list|dict)
### 3. list: append(..)
### 4. dict/lsit: remove items
### 5. dict/list: for iteration
### 6. type(..): for class obj, should return class name
### 7. dict: add new kv, delete kv, update the value of a key
### 8. list: update the value of a element
### 9. list/string: get element or slice:  xx[idx], xx[x:y]

var i = int("10");
var f = float("2.5");
var s1 = str(123);
var s2 = str(true);
var s3 = str(null);
println("output_1", i, f, s1, s2, s3);

println("output_2", len("abcd"), len([1,2,3]), len({"a":1,"b":2}));

var arr = [1];
append(arr, 2);
append(arr, 3);
remove(arr, 1);  # remove middle element
println("output_3", arr);

var d = {"x": 1};
d["y"] = 2;
remove(d, "x");
println("output_4", d["y"]);

var sum_arr = 0;
for (idx => ele in arr) {
  sum_arr += ele;
}
println("output_5", sum_arr);

var sum_dict = 0;
for (k => v in d) {
  sum_dict += v;
}
println("output_6", sum_dict);

class Foo {
  var v = 0;
  fun init(x) { this.v = x; }
  fun get() { return this.v; }
}
var foo = new Foo(9);
println("output_7", type(foo));

arr[0] = 99;
d["y"] = 5;
println("output_8", arr[0], d["y"]);

var arr_slice = arr[0:2];
var str_slice = "hello"[1:4];
println("output_9", arr_slice);
println("output_10", "hello"[1], str_slice);

# expect_1: 10 2.5 123 true null
# expect_2: 4 3 2
# expect_3: [1, 3]
# expect_4: 2
# expect_5: 4
# expect_6: 2
# expect_7: Foo
# expect_8: 99 5
# expect_9: [99, 3]
# expect_10: e ell
