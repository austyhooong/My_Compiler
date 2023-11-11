#include "au_cc.h"

// input filename
static char *current_filename;

// input string
static char *current_input;

void error(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt); // initializes ap to arguments after fmt
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

// reports an error message in the following format and exit
// ex:
//  foo.c:10: x = y + 1;
//                ^ <error message>

static void verror_at(int line_num, char *loc, char *fmt, va_list ap)
{
    // find a beginning of the loc
    char *line = loc;
    while (current_input < line && line[-1] != '\n')
        line--;

    char *end = loc;
    while (*end != '\n')
        ++end;

    // print out the line: foo.c:10:
    int indent = fprintf(stderr, "%s:%d: ", current_filename, line_num);

    // x = y + 1;
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    // show the error message
    int pos = loc - line + indent;

    fprintf(stderr, "%*s", pos, ""); // print pos amount of space
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_at(char *loc, char *fmt, ...)
{
    int line_num = 1;
    for (char *p = current_input; p < loc; ++p)
        if (*p == '\n')
            ++line_num;

    va_list ap;
    va_start(ap, fmt);
    verror_at(line_num, loc, fmt, ap);
}

void error_tok(Token *tok, char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->line_num, tok->loc, fmt, ap);
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

// parse A-F 1-9 a-f
static int parse_hex(char c)
{
    if ('0' <= c && c <= '9')
    {
        return c - '0';
    }
    if ('a' <= c && c <= 'f')
        return c - 'a' + 10;
    return c - 'A' + 10;
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
        "char",
        "struct"};

    for (int i = 0; i < sizeof(kw) / sizeof(*kw); ++i)
    {
        if (equal(tok, kw[i]))
            return true;
    }
    return false;
}

static int read_escaped_ch(char **new_pos, char *p)
{
    // octal number starts with \ and followed by at most 3 digits (must be less than 8)
    if ('0' <= *p && *p <= '7')
    {
        // read an octal number
        int c = *p++ - '0';
        if ('0' <= *p && *p <= '7')
        {
            c = (c << 3) + (*p++ - '0');
            if ('0' <= *p && *p <= '7')
            {
                c = (c << 3) + (*p++ - '0');
            }
        }
        *new_pos = p;
        return c;
    }
    if (*p == 'x')
    {
        // parse hexdecimal
        ++p;
        if (!isxdigit(*p))
        {
            error_at(p, "invalid hex escape sequence");
        }

        int c = 0;
        for (; isxdigit(*p); ++p)
        {
            c = (c << 4) + parse_hex(*p);
        }
        *new_pos = p;
        return c;
    }

    *new_pos = p + 1;

    // string literals are read as it is
    // escaped characters are inheritantly interpreted by the compiler
    // thus no post-processing is
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
        // escape characters within string must be explicitly parsed
        if (*p == '\\')
        {
            buf[len++] = read_escaped_ch(&p, p + 1);
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

// initialize line number into all the tokens
// -> finds a line number by parsing the entire current input from the beginning
static void add_line_numbers(Token *tok)
{
    char *p = current_input;
    int num = 1;

    do
    {
        if (p == tok->loc)
        {
            tok->line_num = num;
            tok = tok->next;
        }
        if (*p == '\n')
            ++num;
    } while (*p++);
}

static Token *tokenize(char *filename, char *p)
{
    current_filename = filename;
    current_input = p;
    Token head = {};
    Token *cur = &head;

    while (*p)
    {
        // skip line comments
        if (start_with(p, "//"))
        {
            p += 2;
            while (*p != '\n')
                ++p;
            continue;
        }

        // skip block comments
        if (start_with(p, "/*"))
        {
            char *end = strstr(p + 2, "*/");
            if (!end)
                error_at(p, "unclosed block comment");
            p = end + 2;
            continue;
        }

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
    add_line_numbers(head.next);
    convert_keywords(head.next);
    return head.next;
}

// return the contents of the given file
static char *read_file(char *path)
{
    FILE *fp;

    // by convention, "-" refers to stdin
    if (strcmp(path, "-") == 0)
    {
        fp = stdin;
    }
    else
    {
        fp = fopen(path, "r");
        if (!fp)
        {
            // strerror: searches an internal array for the errno and returns a pointer to the error message
            error("cannot open %s: %s", path, strerror(errno));
        }
    }

    char *buf;
    size_t buflen;
    // dynamically open a stream for writing to a memory buffer (&buf)
    // updated upon fflush or fclose
    FILE *out = open_memstream(&buf, &buflen);

    // read the entire file
    for (;;)
    {
        char buf2[4096];
        // reads data from fp into the array pointed to by buf2
        int n = fread(buf2, 1, sizeof(buf2), fp);
        if (n == 0)
            break;
        // writes data from buf2 to given stream pointed by out
        fwrite(buf2, 1, n, out);
    }

    if (fp != stdin)
        fclose(fp);

    // explicitly flushing the buffer to the stream
    fflush(out);

    // make sure that the last line is properly terminated with '\n'
    if (buflen == 0 || buf[buflen - 1] != '\n')
        fputc('\n', out);
    // writes a character to the specified stream and advances the position indicator of the stream
    fputc('\0', out);
    fclose(out);
    return buf;
}

Token *tokenize_file(char *path)
{
    char *p = read_file(path);
    return tokenize(path, p);
}
