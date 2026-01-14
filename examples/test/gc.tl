### Test garbage collection
### 1. Object retention across automatic GC

# Create arrays that should be retained
var keep1 = [100, 200, 300];
var keep2 = [400, 500, 600];

println("output_1", keep1, keep2);

# Create many temporary objects to trigger automatic GC
var i = 0;
while (i < 120) {
    var temp = [i, i*2, i*3];
    i += 1;
}

# Arrays should still be valid after auto GC
println("output_2", keep1, keep2);

# expect_1: [100, 200, 300] [400, 500, 600]
# expect_2: [100, 200, 300] [400, 500, 600]
