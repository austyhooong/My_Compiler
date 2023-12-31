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
typedef struct VarScope VarScope;

struct VarScope
{
    VarScope* next;
    char* name;
    Obj* var;
    Type* type_def; // typedef int t => t is parsed as variable but contains this information
};

// scope for struct tags or union tags
typedef struct TagScope TagScope;
struct TagScope
{
    TagScope* next;
    char* name;
    Type* ty;
};
// represents a block scope
typedef struct Scope Scope;
struct Scope
{
    Scope* next;

    // C has 2 block copes:
    // 1) varialbes
    // 2) struct tags
    VarScope* vars;
    TagScope* tags;
};

// variable attributes such as typedef or extern
typedef struct {
    bool is_typedef;
} VarAttr;

// all local variable instances created
static Obj* locals;

// all global variables
static Obj* globals;

static Scope* scope = &(Scope) {};

// points to the funciton ofject the parser is currently parsing
static Obj* current_fn;

static bool is_typename(Token* tok);
static Type* declspec(Token** rest, Token* tok, VarAttr* attr);
static Type* declarator(Token** rest, Token* tok, Type* ty);
static Node* declaration(Token** rest, Token* tok, Type* basety);
static Node* compound_stmt(Token** rest, Token* tok);
static Node* stmt(Token** rest, Token* tok);
static Node* expr_stmt(Token** rest, Token* tok);
static Node* expr(Token** rest, Token* tok);
static Node* assign(Token** rest, Token* tok);
static Node* equality(Token** rest, Token* tok);
static Node* relational(Token** rest, Token* tok);
static Node* add(Token** rest, Token* tok);
static Node* mul(Token** rest, Token* tok);
static Node* unary(Token** rest, Token* tok);
static Node* primary(Token** rest, Token* tok);
static Node* postfix(Token** rest, Token* tok);
static Type* struct_decl(Token** rest, Token* tok);
static Type* union_decl(Token** rest, Token* tok);
static Token* parse_typedef(Token* tok, Type* basety);
static Node* cast(Token** rest, Token* tok);

// nested scope can access external scope
static void enter_scope(void)
{
    Scope* sc = calloc(1, sizeof(Scope));
    sc->next = scope;
    scope = sc;
}

static void leave_scope(void)
{
    scope = scope->next;
}

static VarScope* find_var(Token* tok)
{
    for (Scope* sc = scope; sc; sc = sc->next)
    {
        for (VarScope* vs = sc->vars; vs; vs = vs->next)
            if (equal(tok, vs->name))
                return vs;
    }
    return NULL;
}

static Type* find_tag(Token* tok)
{
    for (Scope* sc = scope; sc; sc = sc->next)
    {
        for (TagScope* sc2 = sc->tags; sc2; sc2 = sc2->next)
        {
            if (equal(tok, sc2->name))
                return sc2->ty;
        }
    }
    return NULL;
}
static Node* new_node(NodeKind kind, Token* tok)
{
    Node* node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->tok = tok;
    return node;
}

static Node* new_binary(NodeKind kind, Node* lhs, Node* rhs, Token* tok)
{
    Node* node = new_node(kind, tok);
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

static Node* new_unary(NodeKind kind, Node* expr, Token* tok)
{
    Node* node = new_node(kind, tok);
    node->lhs = expr;
    return node;
}

static Node* new_num(int64_t val, Token* tok)
{
    Node* node = new_node(ND_NUM, tok);
    node->val = val;
    return node;
}

static Node* new_long(int64_t val, Token* tok) {
    Node* node = new_node(ND_NUM, tok);
    node->val = val;
    node->ty = ty_long;
    return node;
}

static Node* new_var_node(Obj* var, Token* tok)
{
    Node* node = new_node(ND_VAR, tok);
    node->var = var;
    return node;
}

Node* new_cast(Node* expr, Type* ty) {
    add_type(expr);

    Node* node = calloc(1, sizeof(Node));
    node->kind = ND_CAST;
    node->tok = expr->tok;
    node->lhs = expr;
    node->ty = copy_type(ty);
    return node;
}

static VarScope* push_scope(char* name)
{
    VarScope* vs = calloc(1, sizeof(VarScope));
    vs->name = name;
    vs->next = scope->vars;
    scope->vars = vs;
    return vs;
}

static Obj* new_var(char* name, Type* ty)
{
    Obj* var = calloc(1, sizeof(Obj));
    var->name = name;
    var->ty = ty;
    push_scope(name)->var = var;
    return var;
}

static Obj* new_lvar(char* name, Type* ty)
{
    Obj* var = new_var(name, ty);
    var->is_local = true;
    var->next = locals;
    var->ty = ty;
    locals = var;
    return var;
}

static Obj* new_gvar(char* name, Type* ty)
{
    Obj* var = new_var(name, ty);
    var->next = globals;
    globals = var;
    return var;
}

static char* new_unique_name(void)
{
    static int id = 0;
    return format(".L..%d", id++);
}

static Obj* new_anon_gvar(Type* ty)
{
    return new_gvar(new_unique_name(), ty);
}

static Obj* new_string_literal(char* p, Type* ty)
{
    Obj* var = new_anon_gvar(ty);
    var->init_data = p;
    return var;
}

static char*
get_ident(Token* tok)
{
    if (tok->kind != TK_IDENT)
        error_tok(tok, "expected an identifier");
    return strndup(tok->loc, tok->len);
}

static Type* find_typedef(Token* tok) {
    if (tok->kind == TK_IDENT) {
        VarScope* sc = find_var(tok);
        if (sc)
            return sc->type_def;
    }
    return NULL;
}

static int get_number(Token* tok)
{
    if (tok->kind != TK_NUM)
        error_tok(tok, "expected a number");
    return tok->val;
}

static void push_tag_scope(Token* tok, Type* ty)
{
    TagScope* sc = calloc(1, sizeof(TagScope));
    sc->name = strndup(tok->loc, tok->len);
    sc->ty = ty;
    sc->next = scope->tags;
    scope->tags = sc;
}

// declspec (type) = "void" | "char" | "short" | "int" | "long" | struct-decl | union-decl
// the order of typename is irrelevant thus for the following function, it counts eacch typename using bits and hardcode possible combination as a bit representation to compare at the end; this hardcoding allows order agnositc comparison
static Type* declspec(Token** rest, Token* tok, VarAttr* attr)
{
    enum {
        VOID = 1 << 0,
        CHAR = 1 << 2,
        SHORT = 1 << 4,
        INT = 1 << 6,
        LONG = 1 << 8,
        OTHER = 1 << 10
    };

    Type* ty = ty_int;
    int counter = 0;

    while (is_typename(tok)) {
        // handles "typedef" keyword
        if (equal(tok, "typedef")) {
            if (!attr)
                error_tok(tok, "storage class specifier is not allowed in this context");
            attr->is_typedef = true;
            tok = tok->next;
            continue;
        }

        // handles user defined types
        Type* second_ty = find_typedef(tok);    //declaration of typedef will be skipped?
        if (equal(tok, "struct") || equal(tok, "union") || second_ty) {
            if (counter)
                break;

            if (equal(tok, "struct")) {
                ty = struct_decl(&tok, tok->next);
            }
            else if (equal(tok, "union")) {
                ty = union_decl(&tok, tok->next);
            }
            else {
                ty = second_ty;
                tok = tok->next;
            }
            counter += OTHER;
            continue;
        }

        // handles built-int type
        if (equal(tok, "void")) {
            counter += VOID;
        }
        else if (equal(tok, "char")) {
            counter += CHAR;
        }
        else if (equal(tok, "short")) {
            counter += SHORT;
        }
        else if (equal(tok, "int")) {
            counter += INT;
        }
        else if (equal(tok, "long")) {
            counter += LONG;
        }
        else {
            unreachable();
        }

        switch (counter) {
        case VOID:
            ty = ty_void;
            break;
        case CHAR:
            ty = ty_char;
            break;
        case SHORT:
        case SHORT + INT:
            ty = ty_short;
            break;
        case INT:
            ty = ty_int;
            break;
        case LONG:
        case LONG + LONG:
        case LONG + INT:
            ty = ty_long;
            break;
        default:
            error_tok(tok, "invalide type");
        }
        tok = tok->next;
    }
    *rest = tok;
    return ty;
}

// func-params = "(" (param ("," param)*)? ")"
static Type* func_params(Token** rest, Token* tok, Type* ty)
{
    Type head = {};
    Type* cur = &head;

    while (!equal(tok, ")"))
    {
        if (cur != &head)
        {
            tok = skip(tok, ",");
        }
        Type* basety = declspec(&tok, tok, NULL);   //cannot declare typedef in function parameter thus null
        Type* ty = declarator(&tok, tok, basety);
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
static Type* type_suffix(Token** rest, Token* tok, Type* ty) {
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

// declarator = "*"* ("(" ident ")" | "(" declarator ")" | ident) type-suffix
static Type* declarator(Token** rest, Token* tok, Type* ty)
{
    while (consume(&tok, tok, "*")) {
        ty = pointer_to(ty);
    }

    if (equal(tok, "("))
    {
        Token* start = tok;
        Type dummy = {};
        declarator(&tok, start->next, &dummy);
        tok = skip(tok, ")");
        ty = type_suffix(rest, tok, ty);
        return declarator(&tok, start->next, ty);
    }
    if (tok->kind != TK_IDENT)
        error_tok(tok, "expected a variable name");

    ty = type_suffix(rest, tok->next, ty);
    ty->name = tok;
    return ty;
}

// abstract-declarator = "*"* ( "(" abstract-declarator ")" )?
// type that is not associated with a variable name
// int * | int *[3] => array of 3 pointers to int | int (*) [5] => pointer to array of 5 int | int *() => function with no parameter and returning a pointer to int
// used for argument to sizeof and cast
static Type* abstract_declarator(Token** rest, Token* tok, Type* ty) {
    while (equal(tok, "*")) {
        ty = pointer_to(ty);
        tok = tok->next;
    }

    if (equal(tok, "(")) {
        // set the base point
        Token* start = tok;
        Type dummy = {};
        // skip the content inside () as this has the higher priority and will be used as a base point at the end
        abstract_declarator(&tok, start->next, &dummy);
        tok = skip(tok, ")");
        ty = type_suffix(rest, tok, ty);
        // content () will the base point to the type declared outside
        return abstract_declarator(&tok, start->next, ty);
    }
    return type_suffix(rest, tok, ty);
}

static Type* typename(Token** rest, Token* tok) {
    Type* ty = declspec(&tok, tok, NULL);
    return abstract_declarator(rest, tok, ty);
}

// declaration = declspec (declarator ("=" expr) ? ("," declarator ("=" expr)?)*)? ";"
static Node* declaration(Token** rest, Token* tok, Type* basety)
{
    Node head = {};
    Node* cur = &head;
    int i = 0;

    while (!equal(tok, ";"))
    {
        if (i++ > 0)
        {
            tok = skip(tok, ",");
        }

        Type* ty = declarator(&tok, tok, basety);
        if (ty->kind == TY_VOID) {
            error_tok(tok, "variable declared as void");
        }

        Obj* var = new_lvar(get_ident(ty->name), ty);

        if (!equal(tok, "="))
            continue;

        Node* lhs = new_var_node(var, ty->name);
        Node* rhs = assign(&tok, tok->next);
        Node* node = new_binary(ND_ASSIGN, lhs, rhs, tok);
        cur = cur->next = new_unary(ND_EXPR_STMT, node, tok);
    }

    Node* node = new_node(ND_BLOCK, tok);
    node->body = head.next;
    *rest = tok->next;
    return node;
}

// return true if a give token represents a type | typedef
static bool is_typename(Token* tok)
{
    static char* kw[] = {
        "void", "char", "short", "int", "long", "struct", "union", "typedef"
    };

    for (int i = 0; i < sizeof(kw) / sizeof(*kw); ++i) {
        if (equal(tok, kw[i])) {
            return true;
        }
    }
    return find_typedef(tok);
}

// stmt = "return" expr ";"
//     || "if" "(" expr ")" stmt "else" stmt
//     || "for" "(" expr-stmt ";" expr? ";" expr? ")" stmt
static Node* stmt(Token** rest, Token* tok)
{
    if (equal(tok, "return"))
    {
        Node* node = new_node(ND_RETURN, tok);
        Node* exp = expr(&tok, tok->next);
        *rest = skip(tok, ";");

        add_type(exp);
        node->lhs = new_cast(exp, current_fn->ty->return_ty);
        return node;
    }
    if (equal(tok, "if"))
    {
        Node* node = new_node(ND_IF, tok);
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
        Node* node = new_node(ND_FOR, tok);
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
        Node* node = new_node(ND_FOR, tok);
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
static Node* compound_stmt(Token** rest, Token* tok)
{
    Node* node = new_node(ND_BLOCK, tok);
    Node head = {};
    Node* cur = &head;

    enter_scope();

    while (!equal(tok, "}"))
    {
        // declaration
        if (is_typename(tok)) {
            VarAttr attr = {};
            Type* basety = declspec(&tok, tok, &attr);
            if (attr.is_typedef) {
                // typedef is stored within VarScope
                tok = parse_typedef(tok, basety);
                continue;
            }
            cur = cur->next = declaration(&tok, tok, basety);
        }
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
static Node* expr_stmt(Token** rest, Token* tok)
{
    if (equal(tok, ";"))
    {
        *rest = tok->next;
        return new_node(ND_BLOCK, tok);
    }
    Node* node = new_node(ND_EXPR_STMT, tok);
    node->lhs = expr(&tok, tok);
    *rest = skip(tok, ";");
    return node;
}
// expr = assign ("," expr) ?
static Node* expr(Token** rest, Token* tok)
{
    Node* node = assign(&tok, tok);

    if (equal(tok, ","))
        return new_binary(ND_COMMA, node, expr(rest, tok->next), tok);

    *rest = tok;
    return node;
}

static Node* assign(Token** rest, Token* tok)
{
    Node* node = equality(&tok, tok);

    if (equal(tok, "="))
    {
        node = new_binary(ND_ASSIGN, node, assign(&tok, tok->next), tok);
    }
    *rest = tok;
    return node;
}

static Node* equality(Token** rest, Token* tok)
{
    Node* node = relational(&tok, tok);

    while (true)
    {
        Token* start = tok;
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

static Node* relational(Token** rest, Token* tok)
{
    Node* node = add(&tok, tok);
    while (true)
    {
        Token* start = tok;
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
// The following function accomodates the above distinction
static Node* new_add(Node* lhs, Node* rhs, Token* tok)
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
        Node* tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    // ptr + (num * sizeof(*ptr))
    rhs = new_binary(ND_MUL, rhs, new_long(lhs->ty->base->size, tok), tok);
    return new_binary(ND_ADD, lhs, rhs, tok);
}

static Node* new_sub(Node* lhs, Node* rhs, Token* tok)
{
    add_type(lhs);
    add_type(rhs);

    if (is_integer(lhs->ty) && is_integer(rhs->ty))
        return new_binary(ND_SUB, lhs, rhs, tok);

    // ptr - num
    if (lhs->ty->base && is_integer(rhs->ty))
    {
        rhs = new_binary(ND_MUL, rhs, new_long(lhs->ty->base->size, tok), tok);
        add_type(rhs);
        Node* node = new_binary(ND_SUB, lhs, rhs, tok);
        node->ty = lhs->ty;
        return node;
    }

    // ptr - ptr
    if (lhs->ty->base && rhs->ty->base)
    {
        Node* node = new_binary(ND_SUB, lhs, rhs, tok);
        node->ty = ty_int;
        return new_binary(ND_DIV, node, new_num(lhs->ty->base->size, tok), tok);
    }

    error_tok(tok, "invalid operands");
}

static Node* add(Token** rest, Token* tok)
{
    Node* node = mul(&tok, tok);
    while (true)
    {
        Token* start = tok;
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

// mul = cast ("*" cast | "/" cast)*
static Node* mul(Token** rest, Token* tok)
{
    Node* node = cast(&tok, tok);
    while (true)
    {
        Token* start = tok;
        if (equal(tok, "*"))
        {
            node = new_binary(ND_MUL, node, cast(&tok, tok->next), start);
            continue;
        }
        else if (equal(tok, "/"))
        {
            node = new_binary(ND_DIV, node, cast(&tok, tok->next), start);
            continue;
        }
        *rest = tok;
        return node;
    }
}

// cast = "(" type-name ")" cast | unary
static Node* cast(Token** rest, Token* tok) {
    if (equal(tok, "(") && is_typename(tok->next)) {
        Token* start = tok;
        Type* ty = typename(&tok, tok->next);
        tok = skip(tok, ")");
        Node* node = new_cast(cast(rest, tok), ty);
        node->tok = start;
        return node;
    }
    return unary(rest, tok);
}

// unary = ("+" | "-" | "*" | "&") unary
//        | postfix
static Node* unary(Token** rest, Token* tok)
{
    if (equal(tok, "+"))
    {
        return cast(rest, tok->next);
    }
    else if (equal(tok, "-"))
    {
        return new_unary(ND_NEG, cast(rest, tok->next), tok);
    }
    else if (equal(tok, "*"))
    {
        return new_unary(ND_DEREF, cast(rest, tok->next), tok);
    }
    else if (equal(tok, "&"))
    {
        return new_unary(ND_ADDR, cast(rest, tok->next), tok);
    }
    return postfix(rest, tok);
}

// struct-member = (declspec declarator ("," declarator)* ";")*
static void struct_members(Token** rest, Token* tok, Type* ty)
{
    Member head = {};
    Member* cur = &head;

    while (!equal(tok, "}"))
    {
        Type* basety = declspec(&tok, tok, NULL);
        int i = 0;

        while (!consume(&tok, tok, ";"))
        {
            if (i++)
            {
                tok = skip(tok, ",");
            }

            Member* mem = calloc(1, sizeof(Member));
            mem->ty = declarator(&tok, tok, basety);
            mem->name = mem->ty->name;
            cur = cur->next = mem;
        }
    }

    *rest = tok->next;
    ty->members = head.next;
}

// struct-union-decl = ident? ("{" struct-members)?
static Type* struct_union_decl(Token** rest, Token* tok)
{
    // read a tag
    Token* tag = NULL;
    if (tok->kind == TK_IDENT)
    {
        tag = tok;
        tok = tok->next;
    }

    if (tag && !equal(tok, "{"))
    {
        Type* ty = find_tag(tag);
        if (!ty)
            error_tok(tag, "unkonw struct type");
        *rest = tok;
        return ty;
    }

    // construct a struct object
    Type* ty = calloc(1, sizeof(Type));
    ty->kind = TY_STRUCT;
    struct_members(rest, tok->next, ty);
    ty->align = 1;

    // register the struct type if a name was given
    if (tag)
        push_tag_scope(tag, ty);
    return ty;
}

static Type* struct_decl(Token** rest, Token* tok)
{
    Type* ty = struct_union_decl(rest, tok);
    ty->kind = TY_STRUCT;

    // assign offsets within the struct to members
    int offset = 0;
    for (Member* mem = ty->members; mem; mem = mem->next)
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
static Type* union_decl(Token** rest, Token* tok)
{
    Type* ty = struct_union_decl(rest, tok);
    ty->kind = TY_UNION;

    // union size and alignement are based on the largest member
    // struct members are all initialized to 0 using calloc
    for (Member* mem = ty->members; mem; mem = mem->next)
    {
        if (ty->align < mem->ty->align)
            ty->align = mem->ty->align;
        if (ty->size < mem->ty->size)
            ty->size = mem->ty->size;
    }
    ty->size = align_to(ty->size, ty->align);
    return ty;
}

static Member* get_struct_member(Type* ty, Token* tok)
{
    for (Member* mem = ty->members; mem; mem = mem->next)
        if (mem->name->len == tok->len && !strncmp(mem->name->loc, tok->loc, tok->len))
            return mem;
    error_tok(tok, "no such member");
}

// create node for the struct member (tok); struct and its member "type" have been created in lhs during declspec
static Node* struct_ref(Node* lhs, Token* tok)
{
    add_type(lhs);
    if (lhs->ty->kind != TY_STRUCT && lhs->ty->kind != TY_UNION)
        error_tok(lhs->tok, "not a struct or a union");

    Node* node = new_unary(ND_MEMBER, lhs, tok);
    node->member = get_struct_member(lhs->ty, tok);
    return node;
}

// postfix = primary( "[" expr "]" | "." ident | "->" ident)*
static Node* postfix(Token** rest, Token* tok)
{
    Node* node = primary(&tok, tok);

    for (;;)
    {
        if (equal(tok, "["))
        {
            // x[y] => *(x + y);
            Token* start = tok;
            Node* idx = expr(&tok, tok->next);
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
static Node* funcall(Token** rest, Token* tok)
{
    Token* start = tok;
    tok = tok->next->next;

    VarScope* sc = find_var(start);
    if (!sc) {
        error_tok(start, "implicit declaration of a function");
    }
    if (!sc->var || sc->var->ty->kind != TY_FUNC) {
        error_tok(start, "not a function");
    }

    Type* ty = sc->var->ty->return_ty;
    Node head = {};
    Node* cur = &head;

    while (!equal(tok, ")"))
    {
        if (cur != &head)
            tok = skip(tok, ",");
        cur = cur->next = assign(&tok, tok);
        add_type(cur);
    }

    *rest = skip(tok, ")");

    Node* node = new_node(ND_FUNCALL, start);
    node->funcname = strndup(start->loc, start->len);
    node->ty = ty;
    node->args = head.next;
    return node;
}

// primary = "(" "{" stmt+ "}" ")" |
//           "(" expr ")" | 
//           "sizeof" "(" typename ")" | 
//           "sizeof" unary | ident func-args? | str | num
static Node* primary(Token** rest, Token* tok)
{
    Token* start = tok;

    if (equal(tok, "(") && equal(tok->next, "{"))
    {
        // this is a GNU statement expression
        Node* node = new_node(ND_STMT_EXPR, tok);
        node->body = compound_stmt(&tok, tok->next->next)->body;
        *rest = skip(tok, ")");
        return node;
    }
    if (equal(tok, "("))
    {
        Node* node = expr(&tok, tok->next);
        *rest = skip(tok, ")");
        return node;
    }
    if (equal(tok, "sizeof") && equal(tok->next, "(") && is_typename(tok->next->next)) {
        Type* ty = typename(&tok, tok->next->next);
        *rest = skip(tok, ")");
        return new_num(ty->size, start);
    }

    if (equal(tok, "sizeof"))
    {
        Node* node = unary(rest, tok->next);
        add_type(node);
        return new_num(node->ty->size, tok);
    }

    if (tok->kind == TK_IDENT)
    {
        // function call
        if (equal(tok->next, "("))
            return funcall(rest, tok);

        // variable
        VarScope* sc = find_var(tok);
        if (!sc || !sc->var)
            error_tok(tok, "undefined variable");
        // var = new_lvar(strndup(tok->loc, tok->len));
        // strndup: creates null terminated copy of first param with at most second param bytes
        *rest = tok->next;
        return new_var_node(sc->var, tok);
    }

    if (tok->kind == TK_STR)
    {
        Obj* var = new_string_literal(tok->str, tok->ty);
        *rest = tok->next;
        return new_var_node(var, tok);
    }

    if (tok->kind == TK_NUM)
    {
        Node* node = new_num(tok->val, tok);
        *rest = tok->next;
        return node;
    }
    error_tok(tok, "unexpected expression");
}

// parse declaration of typedef
static Token* parse_typedef(Token* tok, Type* basety) {
    bool first = true;
    while (!consume(&tok, tok, ";")) {
        if (!first)
            tok = skip(tok, ",");
        first = false;
        Type* ty = declarator(&tok, tok, basety);
        push_scope(get_ident(ty->name))->type_def = ty;
    }
    return tok;
}

// created in reverse order
static void create_param_lvars(Type* param)
{
    if (param)
    {
        create_param_lvars(param->next);
        new_lvar(get_ident(param->name), param);
    }
}

// declaration/definition of function
static Token* function(Token* tok, Type* basety)
{
    Type* ty = declarator(&tok, tok, basety);
    Obj* fn = new_gvar(get_ident(ty->name), ty);
    fn->is_function = true;
    fn->is_definition = !consume(&tok, tok, ";");

    if (!fn->is_definition)
        return tok;

    current_fn = fn;
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

static Token* global_variable(Token* tok, Type* basety)
{
    bool first = true;

    while (!consume(&tok, tok, ";"))
    {
        if (!first)
            tok = skip(tok, ",");
        first = false;

        Type* ty = declarator(&tok, tok, basety);
        new_gvar(get_ident(ty->name), ty);
    }
    return tok;
}

// look ahead of tokens and return true if a give token is a start of a function def/declaration
static bool is_function(Token* tok)
{
    if (equal(tok->next, ";"))
    {
        return false;
    }

    Type dummy = {};
    Type* ty = declarator(&tok, tok, &dummy);
    return ty->kind == TY_FUNC;
}

// program = (typedef | function-definition | global-varaibles)*
Obj* parse(Token* tok)
{
    globals = NULL;
    while (tok->kind != TK_EOF)
    {
        VarAttr attr = {};
        Type* basety = declspec(&tok, tok, &attr);

        // typedef
        if (attr.is_typedef) {
            tok = parse_typedef(tok, basety);
            continue;
        }

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