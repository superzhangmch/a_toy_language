var raw1 = "{'a': 1, bad}";
try {
  json_decode(raw1);
} catch e {
  println("output_1=", "error1");
}
# expect_1: error1

var raw2 = "{'a':[1,2}";
try {
  json_decode(raw2);
} catch e {
  println("output_2=error2");
}
# expect_2: error2

var raw3 = "{'a': 1, 'b': [1,2,3}"; # mismatched brace
try {
  json_decode(raw3);
} catch e {
  println("output_3", "error3");
}
# expect_3: error3

var raw4 = "\"aaaa";
try {
  json_decode(raw4);
} catch e {
  println("output_4", "error4");
}
# expect_4: error4
