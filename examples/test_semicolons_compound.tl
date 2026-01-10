# Test semicolons and compound assignment improvements

# Test 1: Semicolons allow multiple statements on one line
var x = 10; var y = 20; print(x); print(y)

# Test 2: Compound assignment with simple variables
var z = 5
z += 3; print(z)   # Output: 8
z -= 2; print(z)   # Output: 6
z *= 2; print(z)   # Output: 12
z /= 3; print(z)   # Output: 4

# Test 3: Compound assignment with array index (C interpreter only)
var arr = [1, 2, 3, 4, 5]
arr[2] += 10
print("Array element after += 10:")
print(arr[2])      # Output: 13

# Test 4: Multiple operations
var a = 1; var b = 2
var c = a + b; print(c)  # Output: 3
