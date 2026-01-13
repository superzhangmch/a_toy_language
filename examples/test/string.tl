### test
### regexp_match(regexp, str)
### regexp_find(regexp, str)
###    - 返回正则中的括号指定的匹配的字符串, 返回字符串数组, 找不到匹配返回 []. 多个匹配只返回第一个
###    - eg:  regexp="([a-z]+)://([a-z0-9\.]+).(com|edu)/" str=http://aa.bb.com => ["http", "aa.bb", "com"]
### regexp_replace(regexp, str, replace_with)
###    - regexp 可能带引用的. 读代码看引用怎么写
### str_split(str, seperator)
### str_join(str_list, seperator)

var m = regexp_match("([a-z]+)://([a-z0-9\\.]+)\\.(com|edu)/", "http://aa.bb.com/");
println("output_1", m);

var f = regexp_find("([a-z]+)://([a-z0-9\\.]+)\\.(com|edu)/", "http://aa.bb.com/ rest");
println("output_2", f);

var r = regexp_replace("h([a-z]+)p", "http hp", "X\\1Y");
println("output_3", r);

var parts = str_split("a,b,c", ",");
println("output_4", parts);
println("output_5", str_join(parts, "|"));
var r2 = regexp_replace("h([a-z]+)p", "http hp", "X\\1\\1Y");
println("output_6", r2);
var ml1 = """aa
bb""";
var ml2 = '''11
22''';
println("output_7", len(ml1), len(ml2));
println("output_8", str_trim("  hi \n"));
println("output_9", str_trim("--xyz--", "-"));
println("output_10", str_format("%d-%.3s", 12, "abcdef"));

# expect_1: 1
# expect_2: ["http", "aa.bb", "com"]
# expect_3: XttY hp
# expect_4: ["a", "b", "c"]
# expect_5: a|b|c
# expect_6: XttttY hp
# expect_7: 5 5
# expect_8: hi
# expect_9: xyz
# expect_10: 12-abc
