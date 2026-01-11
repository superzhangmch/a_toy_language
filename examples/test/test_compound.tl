# Test compound assignment operators

var x = 10
print("x = ")
print(x)

x += 5
print("After x += 5: ")
print(x)

x -= 3
print("After x -= 3: ")
print(x)

x *= 2
print("After x *= 2: ")
print(x)

x /= 4
print("After x /= 4: ")
print(x)

# Test with expression
var y = 100
y += x * 2
print("y = 100, y += x * 2: ")
print(y)
