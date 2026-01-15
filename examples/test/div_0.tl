var a = 123
try {
  a = 12/0;
} catch e{
  println ("output_1", "error")
}
# expect_1: error
println("output_2", a)
# expect_2: 123

try {
  a = 12/0.;
} catch e{
  println ("output_3", "error")
}
# expect_3: error
println("output_4", a)
# expect_4: 123
