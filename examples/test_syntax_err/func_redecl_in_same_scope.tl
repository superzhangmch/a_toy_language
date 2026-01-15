# should fail: function redefinition in same scope
fun foo(a) { return a; }
fun foo(a) { return a + 1; }
