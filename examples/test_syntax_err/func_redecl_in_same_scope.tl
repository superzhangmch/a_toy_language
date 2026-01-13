# should fail: function redefinition in same scope
func foo(a) { return a; }
func foo(a) { return a + 1; }
