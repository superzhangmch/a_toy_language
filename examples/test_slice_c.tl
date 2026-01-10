# Test slice feature with C interpreter
var arr = []
append(arr, 1)
append(arr, 2)
append(arr, 3)
append(arr, 4)
append(arr, 5)
append(arr, 6)
append(arr, 7)
append(arr, 8)
append(arr, 9)
append(arr, 10)

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

# Test file I/O
var content = "This is a test file.\nLine 2\nLine 3"
write(content, "test_output_c.txt")
print("File written")

var read_content = read("test_output_c.txt")
print("File content:")
print(read_content)
