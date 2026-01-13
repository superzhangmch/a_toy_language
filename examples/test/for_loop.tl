### test numeric for loop: for (idx = st .. end) {...}

var sum1 = 0;
for (i = 1 .. 3) {
  sum1 += i;
}
println("output_1", sum1);

var sum2 = 0;
for (j = 3 .. 1) {
  sum2 += j;
}
println("output_2", sum2);

var last = -1;
for (k = 5 .. 5) {
  last = k;
}
println("output_3", last);

# expect_1: 6
# expect_2: 6
# expect_3: 5
