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
} TokenKind;

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
Token *tokenize(char *input);

// parse.c
typedef struct Node Node;
typedef struct Obj Obj;
struct Obj
{
    Obj *next;
    char *name; // variable name
    int offset; // offset from RBP
};

// Function
typedef struct Function Function;
struct Function
{
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
    ND_VAR,       // variable
    ND_EXPR_STMT, // expression statement
} NodeKind;

// abstract syntax tree
struct Node
{
    NodeKind kind;
    Node *next;
    Node *lhs;
    Node *rhs;
    int val;
    Obj *var; // kind == ND_VAR
};

Function *parse(Token *tok);

// codegen.c
void codegen(Function *prog);