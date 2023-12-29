// #: stringzing  operator
// - converts the specified argument into a quoted string
// ex: assert(a + b, some_variable, #some_variable) 
// => assert(a + b, some_variable, "some_variable");
#define ASSERT(x, y) assert(x, y, #y)

void assert(int expected, int actual, char* code);
int printf();