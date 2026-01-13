var raw = "{'a':1,'b':[1,2,],}";
println(json_decode(raw));
# expect: {"a": 1, "b": [1, 2]}

var raw2 = "{'x': null, 'y': True}";
println(json_decode(raw2));
# expect: {"x": null, "y": true}

var rawc = "{'z': [1,2,3,],}";
println(json_decode(rawc));
# expect: {"z": [1, 2, 3]}

var raw3 = "\"hello\"";
println(json_decode(raw3));
# expect: hello

raw3 = "123";
println(type(json_decode(raw3)));
# expect: int

raw3 = "true";
println(type(json_decode(raw3)));
# expect: bool
println(json_decode(raw3));
# expect: true

raw3 = "{}";
println(json_decode(raw3));
# expect: {}

raw3 = "null";
println(json_decode(raw3));
# expect: null
println(type(json_decode(raw3)));
# expect: null

raw3 = "[1,2]";
println(json_decode(raw3));
# expect: [1, 2]

raw3 = "[]";
println(json_decode(raw3));
# expect: []

raw3 = '{"a": "aaa\"bbb",}';
println(json_decode(raw3)["a"]);
# expect: aaa"bbb

raw3 = '{"a": "aaa\"b\\\\bb",}';
println(json_decode(raw3)["a"]);
# expect: aaa"b\\bb

raw3 = "{'a': 1, 'b': ['a1', 2]}"
println(json_decode(raw3)["a"]);
# expect: 1
println(json_decode(raw3)["b"][0]);
# expect: a1
println(json_decode(raw3)["b"][1]);
# expect: 2
println(type(json_decode(raw3)["b"][1]));
# expect: int
