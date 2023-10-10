/*

BFS (Nackus Naur Form) format:
context free syntax consists of both non terminal & terminal symbol

non terminal: exists on the left hand side that can be expanded to zero or more terminals

Extended symbol rules (EBFS):
A*: zero or more
A?: A or null
A | B: A or B


*/

#include "au_cc.h"
static Node *expr(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);

static Node *new_node(NodeKind kind)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs)
{
    Node *node = new_node(kind);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_unary(NodeKind kind, Node *expr)
{
    Node *node = new_node(kind);
    node->lhs = expr;
    return node;
}

static Node *new_num(int val)
{
    Node *node = new_node(ND_NUM);
    node->val = val;
    return node;
}

// expr = equality
static Node *expr(Token **rest, Token *tok)
{
    return equality(rest, tok);
}
static Node *equality(Token **rest, Token *tok)
{
    Node *node = relational(&tok, tok);

    while (true)
    {
        if (equal(tok, "=="))
        {
            node = new_binary(ND_EQ, node, relational(&tok, tok->next));
        }
        else if (equal(tok, "!="))
        {
            node = new_binary(ND_NE, node, relational(&tok, tok->next));
        }
        *rest = tok;
        return node;
    }
}

static Node *relational(Token **rest, Token *tok)
{
    Node *node = add(&tok, tok);
    while (true)
    {
        if (equal(tok, "<"))
        {
            node = new_binary(ND_LT, node, add(&tok, tok->next));
        }
        else if (equal(tok, "<="))
        {
            node = new_binary(ND_LE, node, add(&tok, tok->next));
        }
        else if (equal(tok, ">"))
        {
            node = new_binary(ND_LT, add(&tok, tok->next), node);
        }
        else if (equal(tok, ">="))
        {
            node = new_binary(ND_LE, node, add(&tok, tok->next));
        }
        *rest = tok;
        return node;
    }
}

static Node *add(Token **rest, Token *tok)
{
    Node *node = mul(&tok, tok);
    while (true)
    {
        if (equal(tok, "+"))
        {
            node = new_binary(ND_ADD, node, mul(&tok, tok->next));
            continue;
        }
        else if (equal(tok, "-"))
        {
            node = new_binary(ND_SUB, node, mul(&tok, tok->next));
            continue;
        }
        *rest = tok;
        return node;
    }
}

static Node *mul(Token **rest, Token *tok)
{
    Node *node = unary(&tok, tok);
    while (true)
    {
        if (equal(tok, "*"))
        {
            node = new_binary(ND_MUL, node, primary(&tok, tok->next));
            continue;
        }
        else if (equal(tok, "/"))
        {
            node = new_binary(ND_DIV, node, primary(&tok, tok->next));
            continue;
        }
        *rest = tok;
        return node;
    }
}

static Node *unary(Token **rest, Token *tok)
{
    if (equal(tok, "+"))
    {
        return unary(rest, tok->next);
    }
    else if (equal(tok, "-"))
    {
        return new_unary(ND_NEG, unary(rest, tok->next));
    }
    return primary(rest, tok);
}

static Node *primary(Token **rest, Token *tok)
{
    if (equal(tok, "("))
    {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }
    if (tok->kind == TK_NUM)
    {
        Node *node = new_num(tok->val);
        *rest = tok->next;
        return node;
    }
    error_tok(tok, "unexpected expression");
}

Node *parse(Token *tok)
{
    Node *node = expr(&tok, tok);
    if (tok->kind != TK_EOF)
        error_tok(tok, "extra token");
    return node;
}