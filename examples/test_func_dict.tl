# Test user-defined functions
func add(a, b) {
    return a + b
}

func factorial(n) {
    if (n <= 1) {
        return 1
    }
    return n * factorial(n - 1)
}

var result = add(10, 20)
print("add(10, 20) = ")
print(result)

var fact = factorial(5)
print("factorial(5) = ")
print(fact)

# Test dictionary
var person = {"name": "Alice", "age": 30}
print("Name: ")
print(person["name"])
print("Age: ")
print(person["age"])

# Add new key
person["city"] = "Beijing"
print("City: ")
print(person["city"])
