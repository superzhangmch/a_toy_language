var aa = '1234';
if (!("12" in aa)) {
  println('not_in_string');
} else {
  println('in_string');
}
# expect: in_string

if ("99" not_in aa) {
  println('not_in_string');
} else {
  println('in_string_again');
}
# expect: not_in_string

var arr = [1,2,3];
if (5 in arr) {
  println('in_array');
} else {
  println('not_in_array');
}
# expect: not_in_array

if (2 not_in arr) {
  println('not_in_array2');
} else {
  println('in_array2');
}
# expect: in_array2

var d = {'k': 1};
if ("k" in d) {
  println('in_dict');
} else {
  println('not_in_dict');
}
# expect: in_dict

if ("missing" not_in d) {
  println('not_in_dict');
} else {
  println('in_dict2');
}
# expect: not_in_dict
