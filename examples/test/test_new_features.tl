# Test slice feature
var arr = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
var sliced = arr[2:5]
print("Array slice [2:5]:")
var i = 0
while (i < len(sliced)) {
    print(sliced[i])
    i = i + 1
}

# Test string slice
var text = "Hello World"
var sub = text[0:5]
print("String slice [0:5]:")
print(sub)

# Test negative indices (Python supports this)
var end_slice = arr[5:10]
print("Array slice [5:10]:")
i = 0
while (i < len(end_slice)) {
    print(end_slice[i])
    i = i + 1
}

# Test file I/O
var content = "This is a test file.\nLine 2\nLine 3"
write(content, "test_output.txt")
print("File written")

var read_content = read("test_output.txt")
print("File content:")
print(read_content)
