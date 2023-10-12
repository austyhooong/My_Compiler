// AT&T input is in first regiter, output in second register
// prolog :prepares the registers and stack space for the function prior to execution
// rbp : fixed base pointer of stack frame
// rsp : top of the stack
#include "au_cc.h"

static int depth;

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
    if (node->kind == ND_VAR)
    {
        printf("    lea %d(%%rbp), %%rax\n", node->var->offset);
        return;
    }

    error("not an lvalue");
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
        printf("    mov (%%rax), %%rax\n");
        return;
    case ND_ASSIGN:
        gen_addr(node->lhs);
        push(); // push rax to top of stack
        gen_expr(node->rhs);
        pop("%rdi"); // store top of stack to rdi (lhs)
        printf("    mov %%rax, (%%rdi)\n");
        return;
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
    error("invalide expression");
}

static void gen_stmt(Node *node)
{
    switch (node->kind)
    {
    case ND_RETURN:
        gen_expr(node->lhs);
        printf("    jmp .L.return\n");
        return;
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    }

    error("invalid statement");
}

static void assign_lvar_offsets(Function *prog)
{
    int offset = 0;
    for (Obj *var = prog->locals; var; var = var->next)
    {
        offset += 8;
        var->offset = -offset;
    }
    prog->stack_size = align_to(offset, 16);
}

void codegen(Function *prog)
{
    assign_lvar_offsets(prog);
    printf("    .global main\n");
    printf("main:\n");

    // prologue
    printf("    push %%rbp\n");
    printf("    mov %%rsp, %%rbp\n");
    printf("    sub $%d, %%rsp\n", prog->stack_size);

    for (Node *n = prog->body; n; n = n->next)
    {
        gen_stmt(n);
        assert(depth == 0);
    }

    printf(".L.return:\n");
    printf("    mov %%rbp, %%rsp\n");
    printf("    pop %%rbp\n");
    printf("    ret\n");
}