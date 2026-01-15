### test fun
### 1. in fun block, global vars should be accessable
### 2. in fun block: locally defined vars should hide global var with the same name
### 3. return with value should work
### 4. return without value should work(return null)
### 5. 递归调用should work
### 6. args can be class objs

var g = 5;
var shadow = 100;

fun see_globals() {
  println("output_1", g);
  var shadow = 1;
  println("output_2", shadow);
  println("output_3", g);
}

fun ret_val() {
  return 42;
}

fun ret_null() {
  return;
}

fun fact(n) {
  if (n <= 1) {
    return 1;
  }
  return n * fact(n - 1);
}

class Box {
  var v = 0;
  fun init(x) {
    this.v = x;
  }
  fun get() {
    return this.v;
  }
}

fun take_box(b) {
  return b.get();
}

see_globals();
println("output_4", ret_val());
println("output_5", ret_null());
println("output_6", fact(4));

var b = new Box(7);
println("output_7", take_box(b));

# expect_1: 5
# expect_2: 1
# expect_3: 5
# expect_4: 42
# expect_5: null
# expect_6: 24
# expect_7: 7
