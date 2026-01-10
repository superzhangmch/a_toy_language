func factorial(n) {
    if (n <= 1) {
        return 1
    }
    return n * factorial(n - 1)
}

var result = factorial(5)
print(result)

var d = {"x": 1}
print(d["x"])
