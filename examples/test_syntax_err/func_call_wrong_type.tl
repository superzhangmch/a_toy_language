# should fail: passing wrong type to a function expecting numbers
func add_num(a, b) {
  return a + b;
}

add_num("x", 2);
