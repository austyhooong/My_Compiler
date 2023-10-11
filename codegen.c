// AT&T input is in first regiter, output in second register
// prolog :prepares the registers and stack space for the function prior to execution
// rbp : fixed base pointer of stack frame
// rsp : top of the stack
#include "au_cc.h"

static int depth;

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

static void gen_addr(Node *node)
{
    if (node->kind == ND_VAR)
    {
        int offset = (node->name - 'a' + 1) * 8;
        printf("    lea %d(%%rbp), %%rax\n", -offset);
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
    if (node->kind == ND_EXPR_STMT)
    {
        gen_expr(node->lhs);
        return;
    }

    error("invalid statement");
}

void codegen(Node *node)
{
    printf("    .global main\n");
    printf("main:\n");

    // prologue
    printf("    push %%rbp\n");
    printf("    mov %%rsp, %%rbp\n");
    printf("    sub $208, %%rsp\n");

    for (Node *n = node; n; n = n->next)
    {
        gen_stmt(n);
        assert(depth == 0);
    }

    printf("    mov %%rbp, %%rsp\n");
    printf("    pop %%rbp\n");
    printf("    ret\n");
}