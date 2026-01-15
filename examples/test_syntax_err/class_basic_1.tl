class Adder {
  var x = 0;

  var _y = 'YY';

  fun init(a) {
    this.x = a;
  }

  fun _get_y() {
    return this._y
  }
  fun get_y() {
    return this._get_y();
  }
  fun add(b) {
    return this.x + b;
  }
}

var y = b._y()
