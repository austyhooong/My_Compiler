// #: stringzing  operator
// - converts the argument it precedes into a quoted string
// ex: assert(a + b, some_variable)
// #y => "some_variable"
#define ASSERT(x, y) assert(x, y, #y)