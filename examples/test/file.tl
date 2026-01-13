### test file helpers: file_read/file_write/file_append/file_size/file_exist

var fn = "tmp_io.txt";
file_write("hello", fn);
println("output_1", file_size(fn));
println("output_2", file_exist(fn));
println("output_3", file_read(fn));
file_append("X", fn);
println("output_4", file_read(fn));
println("output_5", file_size(fn));

# expect_1: 5
# expect_2: true
# expect_3: hello
# expect_4: helloX
# expect_5: 6
