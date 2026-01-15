### Stress test environment handling with many exceptions
### Ensure environment stays consistent after multiple exceptions

# Test 1: Many sequential exceptions
var vars = {"a": 1, "b": 2, "c": 3};

for (i = 0 .. 10) {
  try {
    if (i % 2 == 0) {
      raise("even");
    } else {
      json_decode('{"bad}');
    }
  } catch e {
    vars["a"] += 1;
  }
}

println(vars["a"]);
# expect: 12

# Test 2: Environment survives exception in var declaration
var base = 100;

try {
  var bad = json_decode('invalid');
} catch e {
  base = 200;
}

try {
  var bad2 = json_decode('invalid2');
} catch e {
  base = 300;
}

println(base);
# expect: 300

# Test 3: Complex nesting with environment
var state = "start";

try {
  state = "try1";
  try {
    state = "try2";
    raise("inner");
  } catch e {
    state = "catch2";
  }
  state = "after_inner";
  raise("outer");
} catch e {
  state = "catch1";
}

println(state);
# expect: catch1

# Test 4: Functions called in exception handlers preserve environment
fun modify_state(val) {
  return val * 2;
}

var accumulator = 5;

for (i = 0 .. 5) {
  try {
    raise("test");
  } catch e {
    accumulator = modify_state(accumulator);
  }
}

println(accumulator);
# expect: 320

# Test 5: Dictionary operations survive exceptions
var data = {};

try {
  var x = json_decode('bad');
  data["key1"] = x;
} catch e {
  data["key1"] = "recovered1";
}

try {
  assert(false, "err");
  data["key2"] = "not_reached";
} catch e {
  data["key2"] = "recovered2";
}

println(data["key1"]);
println(data["key2"]);
# expect: recovered1
# expect: recovered2
