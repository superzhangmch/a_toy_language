### tests for 
###  - if-then-else
###  - foreach(idx=> ele in arr) { FOREACH_BLOCK }
###    - idx and ele should be local variables as in the FOREACH_BLOCK
###  - foreach(k=>v in dict) {..}
###    - k and v should be local variables as in the FOREACH_BLOCK
###  - while(..) {...}
###  - break, continue
###  - var scope: if{NEW_SCOPE} else if {NEW_SCOPE} else {NEW_SCOPE}/ foreach(..) {NEW_SCOPE}/ while (..) {NEW_SCOPE}
###  - in NEW_SCOPE: global variables are accessable; locally defined variable using the same global var name will hide that global var

var g = "global";

# if / else if / else create new scopes; global visible, local shadow stays inside
if (false) {
  println("no");
} else if (true) {
  println("output_1", g);      # sees global
  var g = "shadow";
  println("output_2", g);      # sees shadow
} else {
  println("never");
}
println("output_3", g);        # still global

# foreach over array with continue/break
var nums = [1, 2, 3, 4];
var sum_arr = 0;
foreach (i => n in nums) {
  if (n < 0) {  # unreachable but covers continue syntax
    continue;
  }
  if (n < 0) {  # unreachable but covers break syntax
    break;
  }
  sum_arr += n;
}
println("output_4", sum_arr);

# foreach over dict
var d = {"a": 1, "b": 2};
var cnt = 0;
var sum_dict = 0;
foreach (k => v in d) {
  cnt += 1;
  sum_dict += v;
}
println("output_5", cnt);
println("output_6", sum_dict);

# while with continue/break; block is new scope, can shadow globals
var wsum = 0;
var i = 0;
while (i < 3) {
  var g = "loop_local";  # shadows global inside loop body
  i += 1;
  if (i < 0) {  # unreachable continue/break to cover syntax
    continue;
  }
  if (i < 0) {
    break;
  }
  wsum += i;
}
println("output_7", wsum);

# foreach block variables are local and do not leak or overwrite outer vars
var idx = "outer_idx";
var ele = "outer_ele";
foreach (idx => ele in nums) {
  # shadow outer names inside loop
  if (idx == 0) {
    println("output_8", idx, ele);  # first element
  }
}
println("output_9", idx, ele);  # outer values remain

var k = "outer_k";
var v = "outer_v";
foreach (k => v in d) {
  if (k == "a") {
    println("output_10", k, v);
  }
}
println("output_11", k, v);  # outer values remain

# numeric for loop forward and backward
var s1 = 0;
for (t = 1 .. 4) { s1 += t; }
println("output_12", s1);

var s2 = 0;
for (u = 3 .. 1) { s2 += u; }
println("output_13", s2);

var s3 = 0;
for (p = 1 .. 5) {
  if (p == 2) { continue; }
  if (p == 4) { break; }
  s3 += p;
}
println("output_14", s3);
var u = 11

var a1 = 0;
if (a1 == 0) {
  var a2 = 1;
  if (a2 == 0) {
    var a2 = 3;
  }
  a1 = a2;
}
println("output_15", a1);

# expect_1: global
# expect_2: shadow
# expect_3: global
# expect_4: 10
# expect_5: 2
# expect_6: 3
# expect_7: 6
# expect_8_has: 0 1
# expect_9: outer_idx outer_ele
# expect_10_has: a 1
# expect_11: outer_k outer_v
# expect_12: 10
# expect_13: 6
# expect_14: 4
# expect_15: 1
