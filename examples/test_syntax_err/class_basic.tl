class Adder {
  var x = 0;

  var _y = 'YY';

  func init(a) {
    this.x = a;
  }

  func _get_y() {
    return this._y
  }
  func get_y() {
    return this._get_y();
  }
  func add(b) {
    return this.x + b;
  }
}

var y = b._get_y()
