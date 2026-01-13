### test for math builtins: ceil,floor,round,sin,cos,asin,acos,log,exp,pow,random
### - round: 保留0, 1,2,3 位
### - round: 1. 直接调用; 2. 返回指定范围内的随机数

println("output_1", ceil(1.2), floor(1.8), round(1.5));
println("output_2", sin(0), cos(0));
println("output_3", pow(2, 3), exp(1), round(log(exp(1))));
println("output_4", random());
println("output_5", round(1.4), round(1.5), round(2.5), round(1.2345, 0), round(1.2345, 1), round(1.2345, 2), round(1.2345, 3));
var r1 = random();
var r2 = random(5, 6);
println("output_6", int(r1 >= 0 and r1 <= 1), int(r2 >= 5 and r2 <= 6), r2);
println("output_7", sqrt(9));

# expect_1: 2 1 2
# expect_2: 0 1
# expect_3: 8 2.71828 1
# expect_4_has: .
# expect_5: 1 2 3 1 1.2 1.23 1.235
# expect_6: 1 1
# expect_7: 3
