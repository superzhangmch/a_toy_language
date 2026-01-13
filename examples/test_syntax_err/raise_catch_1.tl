# should fail: catch block redeclares existing var in same scope
var xx = 1111;
try {
  raise("boom");
} catch e {
  var xx = 4444;
}
