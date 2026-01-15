# should error: wrong number of args in function call
fun add(a, b) {
  return a + b;
}

add(1);         # missing one arg
add(1, 2, 3);   # one extra arg
