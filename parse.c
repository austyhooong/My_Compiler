/*

BFS (Nackus Naur Form) format:
context free syntax consists of both non terminal & terminal symbol

non terminal: exists on the left hand side that can be expanded to zero or more terminals

Extended symbol rules (EBFS):
A*: zero or more
A?: A or null
A | B: A or B


This file contains recursive descent parser for C.
since C does not allow returning more than one value, only the current node is returned and remaining part of the input tokens are returned via pointer through the input argument
*/

#include "au_cc.h"

// scope for local or global variables
typedef struct Var_scope Var_scope;

struct Var_scope
{
    Var_scope *next;
    char *name;
    Obj *var;
};

// scope for struct tags or union tags
typedef struct TagScope TagScope;
struct TagScope
{
    TagScope *next;
    char *name;
    Type *ty;
};
// represents a block scope
typedef struct Scope Scope;
struct Scope
{
    Scope *next;

    // C has 2 block copes:
    // 1) varialbes
    // 2) struct tags
    Var_scope *vars;
    TagScope *tags;
};

// all local variable instances created
static Obj *locals;

// all global variables
static Obj *globals;

static Scope *scope = &(Scope){};

static Type *declspec(Token **rest, Token *tok);
static Type *declarator(Token **rest, Token *tok, Type *ty);
static Node *declaration(Token **rest, Token *tok);
static Node *compound_stmt(Token **rest, Token *tok);
static Node *stmt(Token **rest, Token *tok);
static Node *expr_stmt(Token **rest, Token *tok);
static Node *expr(Token **rest, Token *tok);
static Node *assign(Token **rest, Token *tok);
static Node *equality(Token **rest, Token *tok);
static Node *relational(Token **rest, Token *tok);
static Node *add(Token **rest, Token *tok);
static Node *mul(Token **rest, Token *tok);
static Node *unary(Token **rest, Token *tok);
static Node *primary(Token **rest, Token *tok);
static Node *postfix(Token **rest, Token *tok);
static Type *struct_decl(Token **rest, Token *tok);
static Type *union_decl(Token **rest, Token *tok);

// nested scope can access external scope
static void enter_scope(void)
{
    Scope *sc = calloc(1, sizeof(Scope));
    sc->next = scope;
    scope = sc;
}

static void leave_scope(void)
{
    scope = scope->next;
}

static Obj *find_var(Token *tok)
{
    for (Scope *sc = scope; sc; sc = sc->next)
    {
        for (Var_scope *vs = sc->vars; vs; vs = vs->next)
            if (equal(tok, vs->name))
                return vs->var;
    }
    return NULL;
}

static Type *find_tag(Token *tok)
{
    for (Scope *sc = scope; sc; sc = sc->next)
    {
        for (TagScope *sc2 = sc->tags; sc2; sc2 = sc2->next)
        {
            if (equal(tok, sc2->name))
                return sc2->ty;
        }
    }
    return NULL;
}
static Node *new_node(NodeKind kind, Token *tok)
{
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

static Node *new_binary(NodeKind kind, Node *lhs, Node *rhs, Token *tok)
{
    Node *node = new_node(kind, tok);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node *new_unary(NodeKind kind, Node *expr, Token *tok)
{
    Node *node = new_node(kind, tok);
    node->lhs = expr;
    return node;
}

static Node *new_num(int val, Token *tok)
{
    Node *node = new_node(ND_NUM, tok);
    node->val = val;
    return node;
}

static Node *new_var_node(Obj *var, Token *tok)
{
    Node *node = new_node(ND_VAR, tok);
    node->var = var;
    return node;
}

static Var_scope *push_scope(char *name, Obj *var)
{
    Var_scope *vs = calloc(1, sizeof(Var_scope));
    vs->name = name;
    vs->var = var;
    vs->next = scope->vars;
    scope->vars = vs;
    return vs;
}

static Obj *new_var(char *name, Type *ty)
{
    Obj *var = calloc(1, sizeof(Obj));
    var->name = name;
    var->ty = ty;
    push_scope(name, var);
    return var;
}

static Obj *new_lvar(char *name, Type *ty)
{
    Obj *var = new_var(name, ty);
    var->is_local = true;
    var->next = locals; // create linked list of up to 6 arguments
    var->ty = ty;
    locals = var;
    return var;
}

static Obj *new_gvar(char *name, Type *ty)
{
    Obj *var = new_var(name, ty);
    var->next = globals;
    globals = var;
    return var;
}

static char *new_unique_name(void)
{
    static int id = 0;
    return format(".L..%d", id++);
}

static Obj *new_anon_gvar(Type *ty)
{
    return new_gvar(new_unique_name(), ty);
}

static Obj *new_string_literal(char *p, Type *ty)
{
    Obj *var = new_anon_gvar(ty);
    var->init_data = p;
    return var;
}

static char *
get_ident(Token *tok)
{
    if (tok->kind != TK_IDENT)
        error_tok(tok, "expected an identifier");
    return strndup(tok->loc, tok->len);
}

static int get_number(Token *tok)
{
    if (tok->kind != TK_NUM)
        error_tok(tok, "expected a number");
    return tok->val;
}

static void push_tag_scope(Token *tok, Type *ty)
{
    TagScope *sc = calloc(1, sizeof(TagScope));
    sc->name = strndup(tok->loc, tok->len);
    sc->ty = ty;
    sc->next = scope->tags;
    scope->tags = sc;
}

// declspec = "char" | "int" (type) | struct-decl
static Type *declspec(Token **rest, Token *tok)
{
    if (equal(tok, "char"))
    {
        *rest = tok->next;
        return ty_char;
    }

    if (equal(tok, "int"))
    {
        *rest = skip(tok, "int");
        return ty_int;
    }

    if (equal(tok, "struct"))
        return struct_decl(rest, tok->next);

    if (equal(tok, "union"))
        return union_decl(rest, tok->next);

    error_tok(tok, "expected typename");
}

// func-params = "(" (param ("," param)*)? ")"
static Type *func_params(Token **rest, Token *tok, Type *ty)
{
    Type head = {};
    Type *cur = &head;

    while (!equal(tok, ")"))
    {
        if (cur != &head)
        {
            tok = skip(tok, ",");
        }
        Type *basety = declspec(&tok, tok);
        Type *ty = declarator(&tok, tok, basety);
        cur = cur->next = copy_type(ty);
    }

    ty = func_type(ty);
    ty->params = head.next;
    *rest = tok->next;
    return ty;
}

// type-suffix = "(" func-params
//              | "[" num "]" type_suffix
//              | ε
static Type *type_suffix(Token **rest, Token *tok, Type *ty)
{
    if (equal(tok, "("))
        return func_params(rest, tok->next, ty);

    if (equal(tok, "["))
    {
        int sz = get_number(tok->next);
        tok = skip(tok->next->next, "]");
        ty = type_suffix(rest, tok, ty);
        return array_of(ty, sz);
    }

    *rest = tok;
    return ty;
}

// declarator = "*"*ident type-suffix
static Type *declarator(Token **rest, Token *tok, Type *ty)
{
    while (consume(&tok, tok, "*"))
    {
        ty = pointer_to(ty);
    }

    if (tok->kind != TK_IDENT)
        error_tok(tok, "expected a variable name");

    ty = type_suffix(rest, tok->next, ty);
    ty->name = tok;
    return ty;
}

// declaration = declspec (declarator ("=" expr) ? ("," declarator ("=" expr)?)*)? ";"
static Node *declaration(Token **rest, Token *tok)
{
    Type *basety = declspec(&tok, tok);

    Node head = {};
    Node *cur = &head;
    int i = 0;

    while (!equal(tok, ";"))
    {
        if (i++ > 0)
        {
            tok = skip(tok, ",");
        }

        Type *ty = declarator(&tok, tok, basety);
        Obj *var = new_lvar(get_ident(ty->name), ty);

        if (!equal(tok, "="))
            continue;

        Node *lhs = new_var_node(var, ty->name);
        Node *rhs = assign(&tok, tok->next);
        Node *node = new_binary(ND_ASSIGN, lhs, rhs, tok);
        cur = cur->next = new_unary(ND_EXPR_STMT, node, tok);
    }

    Node *node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    *rest = tok->next;
    return node;
}
// return true if a give token represents a type
static bool is_typename(Token *tok)
{
    return equal(tok, "char") || equal(tok, "int") || equal(tok, "struct") || equal(tok, "union");
}

// stmt = "return" expr ";"
//     || "if" "(" expr ")" stmt "else" stmt
//     || "for" "(" expr-stmt ";" expr? ";" expr? ")" stmt
static Node *stmt(Token **rest, Token *tok)
{
    if (equal(tok, "return"))
    {
        Node *node = new_node(ND_RETURN, tok);
        node->lhs = expr(&tok, tok->next);
        *rest = skip(tok, ";");
        return node;
    }
    if (equal(tok, "if"))
    {
        Node *node = new_node(ND_IF, tok);
        tok = skip(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = skip(tok, ")");
        node->then = stmt(&tok, tok);
        if (equal(tok, "else"))
        {
            node->els = stmt(&tok, tok->next);
        }
        *rest = tok;
        return node;
    }
    if (equal(tok, "for"))
    {
        Node *node = new_node(ND_FOR, tok);
        tok = skip(tok->next, "(");

        node->init = expr_stmt(&tok, tok); // expr with ";" at the end

        if (!equal(tok, ";"))
        {
            node->cond = expr(&tok, tok);
        }
        tok = skip(tok, ";");

        if (!equal(tok, ")"))
        {
            node->inc = expr(&tok, tok);
        }
        tok = skip(tok, ")");

        node->then = stmt(rest, tok);
        return node;
    }
    if (equal(tok, "while"))
    {
        Node *node = new_node(ND_FOR, tok);
        tok = skip(tok->next, "(");
        node->cond = expr(&tok, tok);
        tok = skip(tok, ")");
        node->then = stmt(rest, tok);
        return node;
    }

    if (equal(tok, "{"))
        return compound_stmt(rest, tok->next);
    return expr_stmt(rest, tok);
}

// compound-stmt = (declaration | stmt)* "}"
static Node *compound_stmt(Token **rest, Token *tok)
{
    Node *node = new_node(ND_BLOCK, tok);
    Node head = {};
    Node *cur = &head;

    enter_scope();

    while (!equal(tok, "}"))
    {
        // declaration
        if (is_typename(tok))
            cur = cur->next = declaration(&tok, tok);
        else
            cur = cur->next = stmt(&tok, tok);
        add_type(cur);
    }

    leave_scope();

    node->body = head.next;
    *rest = tok->next;
    return node;
}

// expr-stmt = expr ? ";"
static Node *expr_stmt(Token **rest, Token *tok)
{
    if (equal(tok, ";"))
    {
        *rest = tok->next;
        return new_node(ND_BLOCK, tok);
    }
    Node *node = new_node(ND_EXPR_STMT, tok);
    node->lhs = expr(&tok, tok);
    *rest = skip(tok, ";");
    return node;
}
// expr = assign ("," expr) ?
static Node *expr(Token **rest, Token *tok)
{
    Node *node = assign(&tok, tok);

    if (equal(tok, ","))
        return new_binary(ND_COMMA, node, expr(rest, tok->next), tok);

    *rest = tok;
    return node;
}

static Node *assign(Token **rest, Token *tok)
{
    Node *node = equality(&tok, tok);

    if (equal(tok, "="))
    {
        node = new_binary(ND_ASSIGN, node, assign(&tok, tok->next), tok);
    }
    *rest = tok;
    return node;
}

static Node *equality(Token **rest, Token *tok)
{
    Node *node = relational(&tok, tok);

    while (true)
    {
        Token *start = tok;
        if (equal(tok, "=="))
        {
            node = new_binary(ND_EQ, node, relational(&tok, tok->next), start);
            continue;
        }
        else if (equal(tok, "!="))
        {
            node = new_binary(ND_NE, node, relational(&tok, tok->next), start);
            continue;
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
        Token *start = tok;
        if (equal(tok, "<"))
        {
            node = new_binary(ND_LT, node, add(&tok, tok->next), start);
        }
        else if (equal(tok, "<="))
        {
            node = new_binary(ND_LE, node, add(&tok, tok->next), start);
        }
        else if (equal(tok, ">"))
        {
            node = new_binary(ND_LT, add(&tok, tok->next), node, start);
        }
        else if (equal(tok, ">="))
        {
            node = new_binary(ND_LE, add(&tok, tok->next), node, start);
        }
        *rest = tok;
        return node;
    }
}

// pointer arithmatic
// Within C, "+" is overloaded to perform the pointer arithmetic.
// If P is a pointer, p + n, n * (sizeof(*p)) is added instead
// Following function accomodates the above distinction
static Node *new_add(Node *lhs, Node *rhs, Token *tok)
{
    add_type(lhs);
    add_type(rhs);

    if (is_integer(lhs->ty) && is_integer(rhs->ty))
    {
        return new_binary(ND_ADD, lhs, rhs, tok);
    }
    // pointer + pointer is not allowed
    if (lhs->ty->base && rhs->ty->base)
    {
        error_tok(tok, "invalid operands");
    }
    // num + ptr => ptr + num
    if (!lhs->ty->base && rhs->ty->base)
    {
        Node *tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    // ptr + (num * sizeof(*ptr))
    rhs = new_binary(ND_MUL, rhs, new_num(lhs->ty->base->size, tok), tok);
    return new_binary(ND_ADD, lhs, rhs, tok);
}

static Node *new_sub(Node *lhs, Node *rhs, Token *tok)
{
    add_type(lhs);
    add_type(rhs);

    if (is_integer(lhs->ty) && is_integer(rhs->ty))
        return new_binary(ND_SUB, lhs, rhs, tok);

    // ptr - num
    if (lhs->ty->base && is_integer(rhs->ty))
    {
        rhs = new_binary(ND_MUL, rhs, new_num(lhs->ty->base->size, tok), tok);
        add_type(rhs);
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->ty = lhs->ty;
        return node;
    }

    // ptr - ptr
    if (lhs->ty->base && rhs->ty->base)
    {
        Node *node = new_binary(ND_SUB, lhs, rhs, tok);
        node->ty = ty_int;
        return new_binary(ND_DIV, node, new_num(lhs->ty->base->size, tok), tok);
    }

    error_tok(tok, "invalid operands");
}

static Node *add(Token **rest, Token *tok)
{
    Node *node = mul(&tok, tok);
    while (true)
    {
        Token *start = tok;
        if (equal(tok, "+"))
        {
            node = new_add(node, mul(&tok, tok->next), start);
            continue;
        }
        else if (equal(tok, "-"))
        {
            node = new_sub(node, mul(&tok, tok->next), start);
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
        Token *start = tok;
        if (equal(tok, "*"))
        {
            node = new_binary(ND_MUL, node, primary(&tok, tok->next), start);
            continue;
        }
        else if (equal(tok, "/"))
        {
            node = new_binary(ND_DIV, node, primary(&tok, tok->next), start);
            continue;
        }
        *rest = tok;
        return node;
    }
}

// unary = ("+" | "-" | "*" | "&") unary
//        | postfix
static Node *unary(Token **rest, Token *tok)
{
    if (equal(tok, "+"))
    {
        return unary(rest, tok->next);
    }
    else if (equal(tok, "-"))
    {
        return new_unary(ND_NEG, unary(rest, tok->next), tok);
    }
    else if (equal(tok, "*"))
    {
        return new_unary(ND_DEREF, unary(rest, tok->next), tok);
    }
    else if (equal(tok, "&"))
    {
        return new_unary(ND_ADDR, unary(rest, tok->next), tok);
    }
    return postfix(rest, tok);
}

// struct-member = (declspec declarator ("," declarator)* ";")*
static void struct_members(Token **rest, Token *tok, Type *ty)
{
    Member head = {};
    Member *cur = &head;

    while (!equal(tok, "}"))
    {
        Type *basety = declspec(&tok, tok);
        int i = 0;

        while (!consume(&tok, tok, ";"))
        {
            if (i++)
            {
                tok = skip(tok, ",");
            }

            Member *mem = calloc(1, sizeof(Member));
            mem->ty = declarator(&tok, tok, basety);
            mem->name = mem->ty->name;
            cur = cur->next = mem;
        }
    }

    *rest = tok->next;
    ty->members = head.next;
}

// struct-union-decl = ident? ("{" struct-members)?
static Type *struct_union_decl(Token **rest, Token *tok)
{
    // read a tag
    Token *tag = NULL;
    if (tok->kind == TK_IDENT)
    {
        tag = tok;
        tok = tok->next;
    }

    if (tag && !equal(tok, "{"))
    {
        Type *ty = find_tag(tag);
        if (!ty)
            error_tok(tag, "unkonw struct type");
        *rest = tok;
        return ty;
    }

    // construct a struct object
    Type *ty = calloc(1, sizeof(Type));
    ty->kind = TY_STRUCT;
    struct_members(rest, tok->next, ty);
    ty->align = 1;

    // register the struct type if a name was given
    if (tag)
        push_tag_scope(tag, ty);
    return ty;
}

static Type *struct_decl(Token **rest, Token *tok)
{
    Type *ty = struct_union_decl(rest, tok);
    ty->kind = TY_STRUCT;

    // assign offsets within the struct to members
    int offset = 0;
    for (Member *mem = ty->members; mem; mem = mem->next)
    {
        offset = align_to(offset, mem->ty->align);
        mem->offset = offset;
        offset += mem->ty->size;

        // align based on the biggest size of members
        if (ty->align < mem->ty->align)
            ty->align = mem->ty->align;
    }
    ty->size = align_to(offset, ty->align);
    return ty;
}
// union-decl = struct-union-decl
static Type *union_decl(Token **rest, Token *tok)
{
    Type *ty = struct_union_decl(rest, tok);
    ty->kind = TY_UNION;

    // union size and alignement are based on the largest member
    // struct members are all initialized to 0 using calloc
    for (Member *mem = ty->members; mem; mem = mem->next)
    {
        if (ty->align < mem->ty->align)
            ty->align = mem->ty->align;
        if (ty->size < mem->ty->size)
            ty->size = mem->ty->size;
    }
    ty->size = align_to(ty->size, ty->align);
    return ty;
}

static Member *get_struct_member(Type *ty, Token *tok)
{
    for (Member *mem = ty->members; mem; mem = mem->next)
        if (mem->name->len == tok->len && !strncmp(mem->name->loc, tok->loc, tok->len))
            return mem;
    error_tok(tok, "no such member");
}

// create node for the struct member (tok); struct and its member "type" have been created in lhs during declspec
static Node *struct_ref(Node *lhs, Token *tok)
{
    add_type(lhs);
    if (lhs->ty->kind != TY_STRUCT && lhs->ty->kind != TY_UNION)
        error_tok(lhs->tok, "not a struct or a union");

    Node *node = new_unary(ND_MEMBER, lhs, tok);
    node->member = get_struct_member(lhs->ty, tok);
    return node;
}

// postfix = primary( "[" expr "]" | "." ident | "->" ident)*
static Node *postfix(Token **rest, Token *tok)
{
    Node *node = primary(&tok, tok);

    for (;;)
    {
        if (equal(tok, "["))
        {
            // x[y] => *(x + y);
            Token *start = tok;
            Node *idx = expr(&tok, tok->next);
            tok = skip(tok, "]");
            node = new_unary(ND_DEREF, new_add(node, idx, start), start);
            continue;
        }
        if (equal(tok, "."))
        {
            node = struct_ref(node, tok->next);
            tok = tok->next->next;
            continue;
        }
        if (equal(tok, "->"))
        {
            // x->y is tantamount to (*x).y
            node = new_unary(ND_DEREF, node, tok);
            node = struct_ref(node, tok->next);
            tok = tok->next->next;
            continue;
        }
        *rest = tok;
        return node;
    }
}

// when a function is called (this is defined somewhere else)
// funcall = ident "(" (assign ("," assign)*)? ")"
static Node *funcall(Token **rest, Token *tok)
{
    Token *start = tok;
    tok = tok->next->next;

    Node head = {};
    Node *cur = &head;

    while (!equal(tok, ")"))
    {
        if (cur != &head)
            tok = skip(tok, ",");
        cur = cur->next = assign(&tok, tok);
    }

    *rest = skip(tok, ")");

    Node *node = new_node(ND_FUNCALL, start);
    node->funcname = strndup(start->loc, start->len);
    node->args = head.next;
    return node;
}

// primary = "(" "{" stmt+ "}" ")"
//           "(" expr ")" | "sizeof" unary | ident func-args? | num
static Node *primary(Token **rest, Token *tok)
{
    if (equal(tok, "(") && equal(tok->next, "{"))
    {
        // this is a GNU statement expression
        Node *node = new_node(ND_STMT_EXPR, tok);
        node->body = compound_stmt(&tok, tok->next->next)->body;
        *rest = skip(tok, ")");
        return node;
    }
    if (equal(tok, "("))
    {
        Node *node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }

    if (equal(tok, "sizeof"))
    {
        Node *node = unary(rest, tok->next);
        add_type(node);
        return new_num(node->ty->size, tok);
    }

    if (tok->kind == TK_IDENT)
    {
        if (equal(tok->next, "("))
            return funcall(rest, tok);

        // variable
        Obj *var = find_var(tok);
        if (!var)
            error_tok(tok, "undefined variable");
        // var = new_lvar(strndup(tok->loc, tok->len));
        // strndup: creates null terminated copy of first param with at most second param bytes
        *rest = tok->next;
        return new_var_node(var, tok);
    }

    if (tok->kind == TK_STR)
    {
        Obj *var = new_string_literal(tok->str, tok->ty);
        *rest = tok->next;
        return new_var_node(var, tok);
    }

    if (tok->kind == TK_NUM)
    {
        Node *node = new_num(tok->val, tok);
        *rest = tok->next;
        return node;
    }
    error_tok(tok, "unexpected expression");
}

static void create_param_lvars(Type *param)
{
    if (param)
    {
        create_param_lvars(param->next);
        new_lvar(get_ident(param->name), param);
    }
}

// declaration/definition of function
static Token *function(Token *tok, Type *basety)
{
    Type *ty = declarator(&tok, tok, basety);
    Obj *fn = new_gvar(get_ident(ty->name), ty);
    fn->is_function = true;

    locals = NULL;
    enter_scope();
    create_param_lvars(ty->params);
    fn->params = locals;

    tok = skip(tok, "{");
    fn->body = compound_stmt(&tok, tok);
    fn->locals = locals;
    leave_scope();
    return tok;
}

static Token *global_variable(Token *tok, Type *basety)
{
    bool first = true;

    while (!consume(&tok, tok, ";"))
    {
        if (!first)
            tok = skip(tok, ",");
        first = false;

        Type *ty = declarator(&tok, tok, basety);
        new_gvar(get_ident(ty->name), ty);
    }
    return tok;
}

// look ahead of tokens and return true if a give token is a start of a function def/declaration
static bool is_function(Token *tok)
{
    if (equal(tok->next, ";"))
    {
        return false;
    }

    Type dummy = {};
    Type *ty = declarator(&tok, tok, &dummy);
    return ty->kind == TY_FUNC;
}

// program = (function-definition | global-varaibles)*
Obj *parse(Token *tok)
{
    globals = NULL;
    while (tok->kind != TK_EOF)
    {
        Type *basety = declspec(&tok, tok);

        // function
        if (is_function(tok))
        {
            tok = function(tok, basety);
            continue;
        }

        // global variables
        tok = global_variable(tok, basety);
    }
    return globals;
}