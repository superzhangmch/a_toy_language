# should fail: compare int and string with '>'
var a = 11;
var b = "a";

if (a > b) {
  print("unexpected");
}
