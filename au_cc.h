#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef enum
{
    TK_OPERATOR,
    TK_NUM,
    TK_EOF,
} TokenKind;

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
    ND_EXPR_STMT, // expression statement
} NodeKind;

typedef struct Token Token;
struct Token
{
    TokenKind kind;
    Token *next;
    int val; // if TK_NUM
    char *loc;
    int len; // token len (ex: lenght of the integer)
};

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);
void error_tok(Token *tok, char *fmt, ...);
bool equal(Token *tok, char *op);
Token *skip(Token *tok, char *op);
Token *tokenize(char *input);

// abstract syntax tree
typedef struct Node Node;
struct Node
{
    NodeKind kind;
    Node *next;
    Node *lhs;
    Node *rhs;
    int val;
};

Node *parse(Token *tok);

// codegen.c
void codegen(Node *node);