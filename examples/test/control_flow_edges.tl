### Edge cases for control flow
### - nested loops with break/continue
### - for over empty collections
### - shadowing across nested blocks
### - invalid break/continue outside loops should error

# nested for inside while
var outer_sum = 0;
var i = 0;
while (i < 3) {
  var inner = [1, 2, 3];
  for (idx => val in inner) {
    outer_sum += val;
  }
  i += 1;
}
println("output_1", outer_sum);

# for over empty array/dict executes zero times
var hit = 0;
for (k => v in []) {
  hit += 1;
}
for (k => v in {}) {
  hit += 1;
}
println("output_2", hit);

# expect_1: 18
# expect_2: 0
