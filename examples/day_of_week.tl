# Day of Week Calculator
# Input: yyyy-mm-dd format
# Output: Day of the week (Monday, Tuesday, etc.)


fun get_week_day(date_str) {
    # Parse year from positions 0-3
    var year_str = date_str[0:4]
    var year = int(year_str)
    
    # Parse month from positions 5-6
    var month_str = date_str[5:7]
    var month = int(month_str)
    
    # Parse day from positions 8-10
    var day_str = date_str[8:10]
    var day = int(day_str)
    
    # Zeller's Congruence algorithm
    # Adjust for January and February (treat as months 13 and 14 of previous year)
    if (month < 3) {
        month = month + 12
        year = year - 1
    }
    
    # Calculate century and year of century
    var q = day
    var m = month
    var k = year % 100
    var j = year / 100
    
    # Zeller's formula: h = (q + ((13*(m+1))/5) + k + (k/4) + (j/4) - 2*j) % 7
    var h = q + ((13 * (m + 1)) / 5) + k + (k / 4) + (j / 4) - (2 * j)
    h = h % 7
    
    # Adjust for negative modulo
    while (h < 0) {
        h = h + 7
    }
    
    # Map result to day name
    # Zeller's result: 0=Saturday, 1=Sunday, 2=Monday, ..., 6=Friday
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
    return day_name
}

while (true) {
    var date_str = str_trim(input("Enter date (yyyy-mm-dd): "))
    if (date_str == 'exit') { break }
    var day_name = get_week_day(date_str);
    p("Day of week: ", day_name)
}
