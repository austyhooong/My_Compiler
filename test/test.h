// #: stringzing  operator
// - converts the specified argument into a quoted string
// ex: assert(a + b, some_variable, #some_variable) 
// #some_variable => "some_variable"
#define ASSERT(x, y) assert(x, y, #y)

int printf();