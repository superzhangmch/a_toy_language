### Test mixed exception types (runtime + interpreter)
### This tests the dual exception system integration

# Test 1: Catch runtime exception (json_decode)
try {
  var bad = json_decode('{"invalid}');
} catch e {
  println("caught_json");
}
# expect: caught_json

# Test 2: Catch interpreter exception (raise)
try {
  raise("boom");
} catch e {
  println("caught_raise");
}
# expect: caught_raise

# Test 3: Catch runtime exception (assert)
func validate(x) {
  assert(type(x) == "int", "must be int");
  return x;
}

try {
  var result = validate("string");
} catch e {
  println("caught_assert");
}
# expect: caught_assert

# Test 4: Multiple sequential try-catch with mixed exceptions
var count = 0;

try {
  json_decode('bad1');
} catch e {
  count += 1;
}

try {
  raise("error");
} catch e {
  count += 1;
}

try {
  assert(false, "fail");
} catch e {
  count += 1;
}

println(count);
# expect: 3

# Test 5: Variables survive multiple exception handlers
var state = "initial";

try {
  json_decode('bad2');
} catch e {
  state = "after_json";
}

try {
  raise("err");
} catch e {
  state = "after_raise";
}

println(state);
# expect: after_raise

# Test 6: Nested try-catch with mixed exceptions
try {
  try {
    json_decode('nested_bad');
  } catch inner {
    println("inner");
    raise("re-raise");
  }
} catch outer {
  println("outer");
}
# expect: inner
# expect: outer
