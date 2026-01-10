# Day of Week Calculator - Test Version
# Tests several known dates

# Test 1: 2024-01-01 (should be Monday)
var date_str = "2024-01-01"
print("Testing: " + date_str)

var year_str = date_str[0:4]
var year = int(year_str)

var month_str = date_str[5:7]
var month = int(month_str)

var day_str = date_str[8:10]
var day = int(day_str)

# Zeller's Congruence algorithm
if (month < 3) {
    month = month + 12
    year = year - 1
}

var q = day
var m = month
var k = year % 100
var j = year / 100

var h = q + ((13 * (m + 1)) / 5) + k + (k / 4) + (j / 4) - (2 * j)
h = h % 7

while (h < 0) {
    h = h + 7
}

var day_name = ""

if (h == 0) {
    day_name = "Saturday"
} else if (h == 1) {
    day_name = "Sunday"
} else if (h == 2) {
    day_name = "Monday"
} else if (h == 3) {
    day_name = "Tuesday"
} else if (h == 4) {
    day_name = "Wednesday"
} else if (h == 5) {
    day_name = "Thursday"
} else if (h == 6) {
    day_name = "Friday"
}

print(day_name)

# Test 2: 2000-01-01 (should be Saturday)
print("Testing: 2000-01-01")
year = 2000
month = 1
day = 1

if (month < 3) {
    month = month + 12
    year = year - 1
}

q = day
m = month
k = year % 100
j = year / 100

h = q + ((13 * (m + 1)) / 5) + k + (k / 4) + (j / 4) - (2 * j)
h = h % 7

while (h < 0) {
    h = h + 7
}

if (h == 0) {
    day_name = "Saturday"
} else if (h == 1) {
    day_name = "Sunday"
} else if (h == 2) {
    day_name = "Monday"
} else if (h == 3) {
    day_name = "Tuesday"
} else if (h == 4) {
    day_name = "Wednesday"
} else if (h == 5) {
    day_name = "Thursday"
} else if (h == 6) {
    day_name = "Friday"
}

print(day_name)
