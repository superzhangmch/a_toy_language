var gg = 123;
class Adder {
  var x = 5;

  var _y = 'YY';

  func init(a) {
    this.x = a;
  }

  func get_local_gg() {
    var gg = 321
    return gg
  }
  func get_gg()
  {
    return gg; # get global func
  }

  func _get_y() {
    return this._y
  }
  func get_y() {
    return this._get_y();
  }
  func add(x) {
    return this.x + x;
  }

  func get_x() {return this.x}
}

var a = new Adder(1);
println("output_1", a.add(1));
# expect_1: 2

println("output_2", a.add(5))
# expect_2: 6

var b = new Adder(7);
println("output_3", b.add(3));
# expect_3: 10

var y = b.get_y()
println ("output_4", y)
# expect_4: YY

y = b.x
println ("output_5", y)
# expect_5: 7

b.x += 100
println ("output_6", b.x)
# expect_6: 107

println ("output_7", b.get_local_gg())
# expect_7: 321

println ("output_8", b.get_gg())
# expect_8: 123
