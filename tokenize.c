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

bool consume(Token **rest, Token *tok, char *str)
{
    if (equal(tok, str))
    {
        *rest = tok->next;
        return true;
    }

    *rest = tok;
    return false;
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

static bool is_ident_letter(char c)
{
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static bool is_ident_nonletter(char c)
{
    return is_ident_letter(c) || ('0' <= c && c <= '9');
}

static int read_op(char *p)
{
    if (start_with(p, "==") || start_with(p, "<=") || start_with(p, "!=") || start_with(p, ">="))
        return 2;
    return ispunct(*p) ? 1 : 0;
}

static bool is_keyword(Token *tok)
{
    static char *kw[] = {
        "return",
        "if",
        "else",
        "for",
        "while",
        "int"};

    for (int i = 0; i < sizeof(kw) / sizeof(*kw); ++i)
    {
        if (equal(tok, kw[i]))
            return true;
    }
    return false;
}

static void convert_keywords(Token *tok)
{
    for (Token *t = tok; t->kind != TK_EOF; t = t->next)
    {
        if (is_keyword(t))
            t->kind = TK_KEYWORD;
    }
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
        // identifier | keyword
        else if (is_ident_letter(*p))
        {
            char *start = p;
            do
            {
                ++p;
            } while (is_ident_nonletter(*p));
            cur = cur->next = new_token(TK_IDENT, start, p);
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