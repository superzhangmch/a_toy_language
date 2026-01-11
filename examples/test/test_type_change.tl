# Test dynamic typing - variable type changes

var a = "xx"
var b = 1

print("Initial values:")
print("a = ")
print(a)
print("b = ")
print(b)

print("Type of a: ")
print(type(a))
print("Type of b: ")
print(type(b))

# Now assign a to b
b = a

print("After b = a:")
print("b = ")
print(b)
print("Type of b: ")
print(type(b))

# b should now be a string "xx"
# Let's verify we can do string operations on it
var c = b + " world"
print("b + ' world' = ")
print(c)
