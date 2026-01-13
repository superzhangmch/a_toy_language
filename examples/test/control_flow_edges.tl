### Edge cases for control flow
### - nested loops with break/continue
### - foreach over empty collections
### - shadowing across nested blocks
### - invalid break/continue outside loops should error

# nested foreach inside while
var outer_sum = 0;
var i = 0;
while (i < 3) {
  var inner = [1, 2, 3];
  foreach (idx => val in inner) {
    outer_sum += val;
  }
  i += 1;
}
println("output_1", outer_sum);

# foreach over empty array/dict executes zero times
var hit = 0;
foreach (k => v in []) {
  hit += 1;
}
foreach (k => v in {}) {
  hit += 1;
}
println("output_2", hit);

# expect_1: 18
# expect_2: 0
