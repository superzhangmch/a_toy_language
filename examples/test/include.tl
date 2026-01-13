### test include/include_once
### 1. include a file containing code snippet, and include in a func body.
### 2. include_once: 设计case
### 3. 其他

include "include_files/include_lib1.tl";

println("output_1", add_inc(5));  # use function from included file

func foo(v) {
  var local_base = v;
  include "include_files/include_body.tl";  # inserted inside function body
}
foo(7);

var included_var = 100;
include_once "include_files/include_once_snippet.tl";
include_once "include_files/include_once_snippet.tl";
include_once "include_files/include_once_snippet.tl";  # second time should be ignored
println("output_3", included_var);

include "include_files/include_wrapper.tl";  # includes include_once snippet inside
println("output_5", included_var);

# expect_1: 15
# expect_2: 14
# expect_3: 101
# expect_4: 102
# expect_5: 101
