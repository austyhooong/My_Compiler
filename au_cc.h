#define _POSIX_C_SOURCE 200809L // make available functionalities from 2008 edition
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// tokenize.c
typedef enum
{
    TK_OPERATOR,
    TK_IDENT, // identifier
    TK_NUM,
    TK_EOF,
    TK_KEYWORD,
} TokenKind;

typedef struct Type Type;
typedef struct Token Token;
struct Token
{
    TokenKind kind;
    Token *next;
    int val;   // if TK_NUM
    char *loc; // token location
    int len;   // token len (ex: lenght of the integer)
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
bool equal(Token *tok, char *op);
Token *skip(Token *tok, char *op);
bool consume(Token **rest, Token *tok, char *str);
Token *
tokenize(char *input);

// parse.c
typedef struct Node Node;
typedef struct Obj Obj;
struct Obj
{
    Obj *next;
    char *name; // variable name
    Type *ty;
    int offset; // offset from RBP
};

// Function
typedef struct Function Function;
struct Function
{
    Function *next;
    char *name;
    Obj *params;

    Node *body;
    Obj *locals; // local variables
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
    ND_ADDR,      // address &
    ND_DEREF,     // dereference *
    ND_VAR,       // variable
    ND_EXPR_STMT, // expression statement
    ND_RETURN,
    ND_BLOCK,   // {...}
    ND_IF,      // if statement
    ND_FOR,     // for || while statement
    ND_FUNCALL, // function call
} NodeKind;

// abstract syntax tree
struct Node
{
    NodeKind kind;
    Node *next;
    Type *ty;   // Type: value or pointer
    Token *tok; // representative token
    Node *lhs;
    Node *rhs;
    int val;
    Obj *var;   // kind == ND_VAR
    Node *body; // kind == ND_BLOCK

    char *funcname; // function call
    Node *args;

    // if || for statement
    Node *cond;
    Node *then;
    Node *els;
    Node *init;
    Node *inc;
};

Function *parse(Token *tok);

// type.c
typedef enum
{
    TY_INT,
    TY_PTR,
    TY_FUNC,
} TypeKind;

struct Type
{
    TypeKind kind;

    // pointer
    Type *base;

    // declaration
    Token *name;

    // Function type
    Type *return_ty;
    Type *params;
    Type *next;
};

extern Type *ty_int;
bool is_integer(Type *ty);
Type *copy_type(Type *ty);
void add_type(Node *node);
Type *pointer_to(Type *base);
Type *func_type(Type *return_ty);

// codegen.c
void codegen(Function *prog);