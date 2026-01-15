try {
  raise("boom");
  println("nope");
} catch e {
  println("caught_raise");
}
# expect: caught_raise

try {
  assert(false, "fail");
  println("nope2");
} catch e {
  println("caught_assert");
}
# expect: caught_assert

println("ok_after");
# expect: ok_after

try {
  raise("boom");
  println("nope");
} catch e {
  println("output_1", e);
}
# expect_1_has: caught in

try {
  var xx_json = json_decode('{"xx: 1}')
  println(xx_json);
} catch e {
  println("output_2", e);
}
# expect_2_has: caught in


fun add(a, b) {
  assert (type(a) in ["int", "float"], "a type error:"+type(a));
  assert (type(b) in ["int", "float"], "a type error:"+type(b));
  return a+b;
}

try {
  var xx_add = add("11", "22");
  println(xx_add);
} catch e {
  println("output_2", e);
}
# expect_2_has: caught in
# expect_2_has: a type error:string

# try-catch block does not create a new local scope; var redefinition in same scope is forbidden
var xx = 1111
try {
  xx = add("11", "22");
} catch e {
  xx = 3333;
}
println("output_3", xx)
# expect_3: 3333

var yy = 2222;
try {
  assert(false, "force");
  yy = 3333;
} catch e {
  yy = 4444;
}
println("output_4", yy)
# expect_4: 4444

yy = 2222;
try {
  yy = 3333;
} catch e {
  yy = 4444;
}
println("output_5", yy)
# expect_5: 3333
