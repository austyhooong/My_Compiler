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
        "int",
        "sizeof",
        "char"};

    for (int i = 0; i < sizeof(kw) / sizeof(*kw); ++i)
    {
        if (equal(tok, kw[i]))
            return true;
    }
    return false;
}

static int read_escaped_ch(char *p)
{
    // string literals are read as it is (escaped sequence being escaped)
    //  escaped characters are inheritantly interpreted by the compiler
    //  thus no post-processing is
    switch (*p)
    {
    case 'a':
        return '\a';
    case 'b':
        return '\b';
    case 't':
        return '\t';
    case 'n':
        return '\n';
    case 'v':
        return '\v';
    case 'f':
        return '\f';
    case 'r':
        return '\r';
    case 'e': // suppported in GNU C extension
        return 27;
    default:
        return *p;
    }
}

// find closing double-quote
static char *string_literal_end(char *p)
{
    char *start = p;
    for (; *p != '"'; ++p)
    {
        if (*p == '\n' || *p == '\0')
            error_at(start, "unclosed string literal");
        if (*p == '\\')
            ++p;
    }
    return p;
}

static Token *read_string_literal(char *start)
{
    char *end = string_literal_end(start + 1);
    char *buf = calloc(1, end - start);
    int len = 0;

    for (char *p = start + 1; p < end;)
    {
        // escaped characters within string must be explicitly parsed
        if (*p == '\\')
        {
            buf[len++] = read_escaped_ch(p + 1);
            p += 2;
        }
        else
        {
            buf[len++] = *p++;
        }
    }

    Token *tok = new_token(TK_STR, start, end + 1); // store "..."
    tok->ty = array_of(ty_char, len + 1);
    tok->str = buf;
    return tok;
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
        else if (*p == '"')
        {
            cur = cur->next = read_string_literal(p);
            p += cur->len;
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