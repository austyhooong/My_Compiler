#define _POSIX_C_SOURCE 200809L // make available functionalities from 2008 edition
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>

// string.c

char* format(char* fmt, ...);

// tokenize.c
typedef enum
{
    TK_OPERATOR,
    TK_IDENT, // identifier
    TK_NUM,
    TK_EOF,
    TK_KEYWORD,
    TK_STR, // string literals
} TokenKind;

typedef struct Type Type;
typedef struct Token Token;
typedef struct Node Node;
typedef struct Member Member;

struct Token
{
    TokenKind kind;
    Token* next;
    int64_t val; // if TK_NUM (int64_t = exactly 64 bits)
    char* loc;   // token location
    int len;     // token len (ex: length of the integer (123 => 3))
    Type* ty;    // for TK_STR
    char* str;   // string literal with terminating '\0'

    int line_num; // line number
};

void error(char* fmt, ...);
void error_at(char* loc, char* fmt, ...);
void error_tok(Token* tok, char* fmt, ...);
bool equal(Token* tok, char* op);
Token* skip(Token* tok, char* op);
bool consume(Token** rest, Token* tok, char* str);
Token*
tokenize_file(char* filename);

#define unreachable() \
    error("internal error at %s:%d", __FILE__, __LINE__);
// parse.c

// variable or function
typedef struct Obj Obj;
struct Obj
{
    Obj* next;
    char* name; // variable name
    Type* ty;
    int offset; // offset from RBP

    bool is_local;    // local or global/function
    bool is_function; // global variable or function
    bool is_definition;

    // global variable
    char* init_data;

    Obj* params;
    Node* body;
    Obj* locals; // local variables
    int stack_size;
};

typedef enum
{
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_NUM, // integer
    ND_NEG, // unary "-"
    ND_EQ,  // ==
    ND_NE,
    ND_LT,
    ND_LE,
    ND_ASSIGN,    // =
    ND_COMMA,     // ,
    ND_ADDR,      // address &
    ND_DEREF,     // dereference *
    ND_VAR,       // variable
    ND_EXPR_STMT, // expression statement
    ND_RETURN,
    ND_BLOCK,     // {...}
    ND_STMT_EXPR, // statement expression ({})
    ND_IF,        // if statement
    ND_FOR,       // for || while statement
    ND_FUNCALL,   // function call
    ND_MEMBER,    // . (struct member access)
} NodeKind;

// abstract syntax tree
struct Node
{
    NodeKind kind;
    Node* next;
    Type* ty;   // Type: value or pointer
    Token* tok; // representative token
    Node* lhs;
    Node* rhs;
    int64_t val;
    Obj* var; // kind == ND_VAR

    // block || statement expression
    Node* body; // kind == ND_BLOCK

    // struct member access
    Member* member;

    char* funcname; // function call
    Node* args;

    // if || for statement
    Node* cond;
    Node* then;
    Node* els;
    Node* init;
    Node* inc;
};

Obj* parse(Token* tok);

// type.c
typedef enum
{
    TY_VOID,
    TY_CHAR,
    TY_SHORT,
    TY_INT,
    TY_LONG,
    TY_PTR,
    TY_FUNC,
    TY_ARRAY,
    TY_STRUCT,
    TY_UNION,
} TypeKind;

struct Type
{
    TypeKind kind;

    int size;  // sizeof() value
    int align; // alignment

    // pointer to or array of type.
    // same member is used to represent pointer/array duality
    // for resolution of a pointer, this member is examined instead of kind member to determine the equivalence of pointer thus array of T is treated as a pointer to T as required by the C spec
    Type* base;

    // declaration
    Token* name;

    // array
    int array_len;

    // struct
    Member* members;

    // Function type
    Type* return_ty;
    Type* params;
    Type* next;
};

// struct member
struct Member
{
    Member* next;
    Type* ty;
    Token* name;
    int offset;
};


extern Type* ty_void;
extern Type* ty_char;
extern Type* ty_short;
extern Type* ty_int;
extern Type* ty_long;

bool is_integer(Type* ty);
Type* copy_type(Type* ty);
void add_type(Node* node);
Type* pointer_to(Type* base);
Type* func_type(Type* return_ty);
Type* array_of(Type* base, int size);

// codegen.c
void codegen(Obj* prog, FILE* out);
int align_to(int n, int align);