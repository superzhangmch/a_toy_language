### test and or nor
### null as false / int 0 as false / empty string as false / empty list as false / empty dict as false
###  * 要证明他们在 if 或 while 中可用
### as true: number != 0 / non-empty str, list, dict, class_obj
###  * 要证明他们在 if 或 while 中可用
### if (a) /if ("") both are valid

var t1 = int((null or 1) and ("" or 2));
println("output_1", t1);

var t2 = int((0 or "x") and ([] or 3));
println("output_2", t2);

var t3 = int(({} or "dict") and ("str" or 0));
println("output_3", t3);

class C { func init() {} }
var obj = new C();
if (obj) {
  println("output_4", "obj_true");
}
var empty_str = "";
var truth_empty = int(empty_str);
println("output_5", truth_empty);
if (0) {
  println("output_6", "zero_true?");
} else {
  println("output_6", "zero_false");
}

if (null) { println("output_7", "null_true"); } else { println("output_7", "null_false"); }
if ("") { println("output_8", "empty_str_true"); } else { println("output_8", "empty_str_false"); }
if ([]) { println("output_9", "empty_list_true"); } else { println("output_9", "empty_list_false"); }
if ({}) { println("output_10", "empty_dict_true"); } else { println("output_10", "empty_dict_false"); }

if (123) { println("output_11", "int_true"); }
if ("abc") { println("output_12", "str_true"); }
if ([1,2]) { println("output_13", "list_true"); }
if ({"k":1}) { println("output_14", "dict_true"); }

var w = 0;
while (w) {
  println("output_15", "should_not_print");
  w -= 1;
}
var w2 = 2;
while (w2) {
  println("output_16", w2);
  w2 -= 1;
}

# expect_1: 1
# expect_2: 1
# expect_3: 1
# expect_4: obj_true
# expect_5: 0
# expect_6: zero_false
# expect_7: null_false
# expect_8: empty_str_false
# expect_9: empty_list_false
# expect_10: empty_dict_false
# expect_11: int_true
# expect_12: str_true
# expect_13: list_true
# expect_14: dict_true
# expect_16: 2
