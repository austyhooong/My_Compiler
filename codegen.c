// AT&T input is in first regiter, output in second register
// prolog :prepares the registers and stack space for the function prior to execution
// rbp : fixed base pointer of stack frame
// rsp : top of the stack
// rax : accmulator
// rcx : counter

/*
    static data regions
    - preceded by .data directive

    labels
    -denotes the address of the data
    ex:
    var:
        .byte 64 // declaring a byte referred to as a location var with value 64
        .byte 10 // declaring a byte with no label at location var + 1 with value 10
    foo:
        .zero 10 // array of size 10 initialized with 0
    str:
        .string "hello" // special notation for initializing string

    address calculation:
    mov %cl, (%esi, %eax, 4) // move the contents of CL into the byte at address ESI + EAS * 4

 */

#include "au_cc.h"

static int depth;
static char *argreg[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
static Function *current_fn;

static void gen_expr(Node *node);

static int count(void)
{
    static int i = 1;
    return i++;
}

static void push(void)
{
    printf("    push %%rax\n");
    ++depth;
}

static void pop(char *arg)
{
    printf("    pop %s\n", arg);
    --depth;
}

// align to nearest multiple of given align
// -1 to ensure the exact align stays the same
// ex n = 8, align = 8 => (8 + 8 - 1) / 8 * 8 = 8
static int align_to(int n, int align)
{
    return (n + align - 1) / align * align;
}

static void gen_addr(Node *node)
{
    switch (node->kind)
    {
    case ND_VAR:
        printf("    lea %d(%%rbp), %%rax\n", node->var->offset);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    }

    error_tok(node->tok, "not an lvalue");
}

// load a value from where &rax is pointing to
static void load(Type *ty)
{
    if (ty->kind == TY_ARRAY)
    {
        // cannot load value of entire array thus convert it to a pointer referencing the first element of array
        return;
    }
    printf("    mov (%%rax), %%rax\n");
}

// store %rax to an address that the stack top is pointing to
static void store(void)
{
    pop("%rdi");
    printf("    mov %%rax, (%%rdi)\n");
}

static void gen_expr(Node *node)
{
    switch (node->kind)
    {
    case ND_NUM:
        printf("    mov $%d, %%rax\n", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        printf("    neg %%rax\n");
        return;
    case ND_VAR:
        gen_addr(node);
        load(node->ty);
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        load(node->ty);
        return;
    case ND_ADDR: // lea the address
        gen_addr(node->lhs);
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push(); // push rax to top of stack
        gen_expr(node->rhs);
        store();
        return;
    case ND_FUNCALL:
    {
        int num_args = 0;
        for (Node *arg = node->args; arg; arg = arg->next)
        {
            gen_expr(arg);
            push();
            ++num_args;
        }

        for (int i = num_args - 1; i >= 0; --i)
            pop(argreg[i]);
        printf("    mov $0, %%rax\n");
        printf("    call %s\n", node->funcname);
        return;
    }
    }

    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("%rdi");

    switch (node->kind)
    {
    case ND_ADD:
        printf("    add %%rdi, %%rax\n");
        return;
    case ND_SUB:
        printf("    sub %%rdi, %%rax\n");
        return;
    case ND_MUL:
        printf("    imul %%rdi, %%rax\n");
        return;
    case ND_DIV:
        printf("    cqo\n");        // extend RAX to 128 bits by setting it in RDX and RAX
        printf("    idiv %%rdi\n"); // implicitly combine RDX and RAX as 128 bits
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
        printf("    cmp %%rdi, %%rax\n");
        if (node->kind == ND_EQ)
            printf("    sete %%al\n");
        else if (node->kind == ND_NE)
            printf("    setne %%al\n");
        else if (node->kind == ND_LT)
            printf("    setl %%al\n");
        else if (node->kind == ND_LE)
            printf("    setle %%al\n");
        printf("    movzb %%al, %%rax\n");
        return;
    }
    error_tok(node->tok, "invalide expression");
}

static void gen_stmt(Node *node)
{
    switch (node->kind)
    {
    case ND_IF:
    {
        int c = count();
        gen_expr(node->cond);
        printf("    cmp $0, %%rax\n");
        printf("    je .L.else.%d\n", c);
        gen_stmt(node->then);
        printf("    jmp .L.end.%d\n", c);
        printf(".L.else.%d:\n", c);
        if (node->els)
            gen_stmt(node->els);
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_FOR:
    {
        int c = count();
        if (node->init)
            gen_stmt(node->init);
        printf(".L.begin.%d:\n", c);
        if (node->cond)
        {
            gen_expr(node->cond);
            printf("    cmp $0, %%rax\n");
            printf("    je .L.end.%d\n", c);
        }
        gen_stmt(node->then);
        if (node->inc)
            gen_expr(node->inc);
        printf("    jmp .L.begin.%d\n", c);
        printf(".L.end.%d:\n", c);
        return;
    }
    case ND_BLOCK:
        for (Node *n = node->body; n; n = n->next)
        {
            gen_stmt(n);
        }
        return;
    case ND_RETURN:
        gen_expr(node->lhs);
        printf("    jmp .L.return.%s\n", current_fn->name);
        return;
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    }

    error_tok(node->tok, "invalid statement");
}

static void assign_lvar_offsets(Function *prog)
{
    for (Function *fn = prog; fn; fn = fn->next)
    {
        int offset = 0;
        for (Obj *var = fn->locals; var; var = var->next)
        {
            offset += var->ty->size;
            var->offset = -offset; // pushing stack downward to allocate memory
        }
        fn->stack_size = align_to(offset, 16);
    }
}

void codegen(Function *prog)
{
    assign_lvar_offsets(prog);
    for (Function *func = prog; func; func = func->next)
    {
        printf("    .global %s\n", func->name);
        printf("%s:\n", func->name);
        current_fn = func;

        // prologue
        printf("    push %%rbp\n");
        printf("    mov %%rsp, %%rbp\n");
        printf("    sub $%d, %%rsp\n", func->stack_size);

        // save passed-by-register arguments to the stack
        int i = 0;
        for (Obj *var = func->params; var; var = var->next)
            printf("    mov %s, %d(%%rbp)\n", argreg[i++], var->offset);

        // emit code
        gen_stmt(func->body);
        assert(depth == 0);

        printf(".L.return.%s:\n", func->name);
        printf("    mov %%rbp, %%rsp\n");
        printf("    pop %%rbp\n");
        printf("    ret\n");
    }
}