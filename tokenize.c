#include "au_cc.h"
static char *current_input;

void error(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt); // initializes ap to arguments after fmt
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

static void verror_at(char *loc, char *fmt, va_list ap)
{
    int pos = loc - current_input;
    fprintf(stderr, "%s\n", current_input);
    fprintf(stderr, "%*s", pos, ""); // print pos amount of space
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_at(char *loc, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror_at(loc, fmt, ap);
}

void error_tok(Token *tok, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->loc, fmt, ap);
}

bool equal(Token *tok, char *op)
{
    return memcmp(tok->loc, op, tok->len) == 0 && op[tok->len] == '\0'; // memcmp: compares the first tok->len bytes
}

Token *skip(Token *tok, char *s)
{
    if (!equal(tok, s))
    {
        error_tok(tok, "expected '%s'", s);
    }
    return tok->next;
}

static int get_number(Token *tok)
{
    if (tok->kind != TK_NUM)
        error_tok(tok, "expected a number");
    return tok->val;
}

Token *new_token(TokenKind kind, char *start, char *end)
{
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->loc = start;
    tok->len = end - start;
    return tok;
}

// bool is_operator(char *op)
// {
//     return *op == '<' || *op == '>' || *op == '+' || *op == '-' || *op == '/' || *op == '*' || *op == '(' || *op == ')';
// }

static bool start_with(char *p, char *q)
{
    return strncmp(p, q, strlen(q)) == 0;
}

static int read_op(char *p)
{
    if (start_with(p, "==") || start_with(p, "<=") || start_with(p, "!=") || start_with(p, ">="))
        return 2;
    return ispunct(*p) ? 1 : 0;
}

Token *tokenize(char *p)
{
    current_input = p;
    Token head = {};
    Token *cur = &head;

    while (*p)
    {
        int op_len = read_op(p);
        if (isspace(*p))
            ++p;
        else if (isdigit(*p))
        {
            cur = cur->next = new_token(TK_NUM, p, p);
            char *q = p;
            cur->val = strtoul(p, &p, 10);
            cur->len = p - q;
        }
        else if ('a' <= *p && *p <= 'z')
        {
            cur = cur->next = new_token(TK_IDENT, p, p + 1);
            ++p;
        }
        else if (op_len)
        {
            cur = cur->next = new_token(TK_OPERATOR, p, p + op_len);
            p += cur->len;
        }
        else
        {
            error_at(p, "invalid token");
        }
    }
    cur = cur->next = new_token(TK_EOF, p, p);
    return head.next;
}