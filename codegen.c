// AT&T input is in first regiter, output in second register
// prolog :prepares the registers and stack space for the function prior to execution
// rbp : fixed base pointer of stack frame
// rsp : top of the stack
// rax : accmulator
// rcx : counter
// rip : instrution pointer referring to the the next assembly instruction below
// rdi : first argument
// rsi, rdx, rcx, r8, r9 : second argument...
// a(%rip): calculate rel32 displacement to reach global variable a from rip

// mov (b (byte), w (2byte), l(4byte), q(8byte))
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

static FILE* output_file;
static int depth;
static char* argreg8[] = { "%dil", "%sil", "%dl", "%cl", "%r8b", "%r9b" };
static char* argreg16[] = { "%di", "%si", "%dx", "%cx", "%r8w", "%r9w" };
static char* argreg32[] = { "%edi", "%esi", "%edx", "%ecx", "%r8d", "%r9d" };
static char* argreg64[] = { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" };
static Obj* current_fn;

static void gen_expr(Node* node);
static void gen_stmt(Node* node);

static void println(char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(output_file, fmt, ap);
    va_end(ap);
    fprintf(output_file, "\n");
}

static int count(void)
{
    static int i = 1;
    return i++;
}

static void push(void)
{
    println("    push %%rax");
    ++depth;
}

static void pop(char* arg)
{
    println("    pop %s", arg);
    --depth;
}

// round up 'n' to the nearest multiple of 'align'.
// ex: align_to(5, 8) returns 8 and align_to(11, 8) returns 16
int align_to(int n, int align)
{
    return (n + align - 1) / align * align;
}

static void gen_addr(Node* node)
{
    switch (node->kind)
    {
    case ND_VAR:
        if (node->var->is_local)
        {
            // local variable
            println("    lea %d(%%rbp), %%rax", node->var->offset);
        }
        else
        {
            // global variable
            println("    lea %s(%%rip), %%rax", node->var->name);
        }
        return;
    case ND_DEREF:
        gen_expr(node->lhs);
        return;
    case ND_COMMA:
        gen_expr(node->lhs);
        gen_addr(node->rhs);
        return;
    case ND_MEMBER:
        gen_addr(node->lhs);
        println("  add $%d, %%rax", node->member->offset);
        return;
    }

    error_tok(node->tok, "not an lvalue");
}

// load a value from where &rax is pointing to rax
static void load(Type* ty)
{
    if (ty->kind == TY_ARRAY || ty->kind == TY_STRUCT || ty->kind == TY_UNION)
    {
        // cannot load value of entire array thus convert it to a pointer referencing the first element of array
        // array decay occurs here
        return;
    }
    // movs : sign extend
    // movz : zero extend
    if (ty->size == 1)
        println("    movsbq (%%rax), %%rax"); // movsb: move one byte and sign extend to 8 bytes
    else if (ty->size == 2)
        println("   movswq (%%rax), %%rax");
    else if (ty->size == 4)
        println("   movsxd (%%rax), %%rax");
    else
        println("    mov (%%rax), %%rax");
}

// store %rax to an address that the stack top is pointing to
static void
store(Type* ty)
{
    pop("%rdi");

    if (ty->kind == TY_STRUCT || ty->kind == TY_UNION)
    {
        for (int i = 0; i < ty->size; ++i)
        {
            println("   mov %d(%%rax), %%r8b", i);
            println("   mov %%r8b, %d(%%rdi)", i);
        }
        return;
    }
    if (ty->size == 1)
        println("    mov %%al, (%%rdi)");
    else if (ty->size == 2)
        println("   mov %%ax, (%%rdi)");
    else if (ty->size == 4)
        println("   mov %%eax, (%%rdi)");
    else
        println("   mov %%rax, (%%rdi)");
}

enum { I8, I16, I32, I64 };

static int getTypeId(Type* ty) {
    switch (ty->kind) {
    case TY_CHAR:
        return I8;
    case TY_SHORT:
        return I16;
    case TY_INT:
        return I32;
    }
    return I64;
}

// tables for type casts
static char i32i8[] = "movsbl %al, %eax";   // moving only 8 bits to 32 bits
static char i32i16[] = "movswl %ax, %eax";
static char i32i64[] = "movsxd %eax, %rax";

static char* cast_table[][10] = {
    {NULL, NULL, NULL, i32i64},
    {i32i8, NULL, NULL, i32i64},
    {i32i8, i32i16, NULL, i32i64},
    {i32i8, i32i16, NULL, NULL}
};

static void cast(Type* from, Type* to) {
    if (to->kind == TY_VOID)
        return;

    int t1 = getTypeId(from);
    int t2 = getTypeId(to);
    if (cast_table[t1][t2]) {
        println("   %s", cast_table[t1][t2]);
    }
}

static void gen_expr(Node* node)
{
    println("   .loc 1 %d", node->tok->line_num);
    switch (node->kind)
    {
    case ND_NUM:
        println("    mov $%ld, %%rax", node->val);
        return;
    case ND_NEG:
        gen_expr(node->lhs);
        println("    neg %%rax");
        return;
    case ND_VAR:
    case ND_MEMBER:
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
        gen_addr(node->lhs); // lhs is stored in rax
        push();              // push rax to top of stack
        gen_expr(node->rhs);
        store(node->ty);
        return;
    case ND_STMT_EXPR:
        for (Node* n = node->body; n; n = n->next)
        {
            gen_stmt(n);
        }
        return;
    case ND_COMMA:
        gen_expr(node->lhs);
        gen_expr(node->rhs);
        return;
    case ND_CAST:
        gen_expr(node->lhs);
        cast(node->lhs->ty, node->ty);
        return;
    case ND_FUNCALL:
    {
        int num_args = 0;
        for (Node* arg = node->args; arg; arg = arg->next)
        {
            gen_expr(arg);
            push();
            ++num_args;
        }

        for (int i = num_args - 1; i >= 0; --i)
            pop(argreg64[i]);
        println("    mov $0, %%rax");
        println("    call %s", node->funcname);
        return;
    }
    }

    gen_expr(node->rhs);
    push();
    gen_expr(node->lhs);
    pop("%rdi");

    // eax: 32 bit; rax: 64 bit
    char* ax, * di;

    if (node->lhs->ty->kind == TY_LONG || node->lhs->ty->base) {
        ax = "%rax";
        di = "%rdi";
    }
    else {
        ax = "%eax";
        di = "%edi";
    }

    switch (node->kind)
    {
    case ND_ADD:
        println("    add %s, %s", di, ax);
        return;
    case ND_SUB:
        println("    sub %s, %s", di, ax);
        return;
    case ND_MUL:
        println("    imul %s, %s", di, ax);
        return;
    case ND_DIV:
        if (node->lhs->ty->size == 8)
            println("    cqo");        // extend RAX to 128 bits by setting it in RDX and RAX
        else
            println("   cdq");
        println("    idiv %s", di); // implicitly combine RDX and RAX as 128 bits
        return;
    case ND_EQ:
    case ND_NE:
    case ND_LT:
    case ND_LE:
        println("    cmp %s, %s", di, ax);
        if (node->kind == ND_EQ)
            println("    sete %%al");
        else if (node->kind == ND_NE)
            println("    setne %%al");
        else if (node->kind == ND_LT)
            println("    setl %%al");
        else if (node->kind == ND_LE)
            println("    setle %%al");
        println("    movzb %%al, %%rax");
        return;
    }
    error_tok(node->tok, "invalide expression");
}

static void gen_stmt(Node* node)
{
    println("   .loc 1 %d", node->tok->line_num);

    switch (node->kind)
    {
    case ND_IF:
    {
        int c = count();
        gen_expr(node->cond);
        println("    cmp $0, %%rax");
        println("    je .L.else.%d", c);
        gen_stmt(node->then);
        println("    jmp .L.end.%d", c);
        println(".L.else.%d:", c);
        if (node->els)
            gen_stmt(node->els);
        println(".L.end.%d:", c);
        return;
    }
    case ND_FOR:
    {
        int c = count();
        if (node->init)
            gen_stmt(node->init);
        println(".L.begin.%d:", c);
        if (node->cond)
        {
            gen_expr(node->cond);
            println("    cmp $0, %%rax");
            println("    je .L.end.%d", c);
        }
        gen_stmt(node->then);
        if (node->inc)
            gen_expr(node->inc);
        println("    jmp .L.begin.%d", c);
        println(".L.end.%d:", c);
        return;
    }
    case ND_BLOCK:
        for (Node* n = node->body; n; n = n->next)
        {
            gen_stmt(n);
        }
        return;
    case ND_RETURN:
        gen_expr(node->lhs);
        println("    jmp .L.return.%s", current_fn->name);
        return;
    case ND_EXPR_STMT:
        gen_expr(node->lhs);
        return;
    }

    error_tok(node->tok, "invalid statement");
}

static void assign_lvar_offsets(Obj* prog)
{
    for (Obj* fn = prog; fn; fn = fn->next)
    {
        if (!fn->is_function)
            continue;

        int offset = 0;
        for (Obj* var = fn->locals; var; var = var->next)
        {
            offset += var->ty->size;
            offset = align_to(offset, var->ty->align);
            var->offset = -offset; // pushing stack downward to allocate memory
        }
        fn->stack_size = align_to(offset, 16);
    }
}

static void emit_data(Obj* prog)
{
    for (Obj* var = prog; var; var = var->next)
    {
        if (var->is_function)
            continue;

        println("    .data");
        println("    .global %s", var->name);
        println("%s:", var->name);

        if (var->init_data)
        {
            for (int i = 0; i < var->ty->size; ++i)
            {
                println("    .byte %d", var->init_data[i]);
            }
        }
        else
        {
            println("    .zero %d", var->ty->size);
        }
    }
}

static void store_gp(int r, int offset, int sz)
{
    switch (sz)
    {
    case 1:
        println("   mov %s, %d(%%rbp)", argreg8[r], offset);
        return;
    case 2:
        println("   mov %s, %d(%%rbp)", argreg16[r], offset);
        return;
    case 4:
        println("   mov %s, %d(%%rbp)", argreg32[r], offset);
        return;
    case 8:
        println("   mov %s, %d(%%rbp)", argreg64[r], offset);
        return;
    }
}

static void emit_text(Obj* prog)
{
    for (Obj* func = prog; func; func = func->next)
    {
        if (!func->is_function || !func->is_definition)
            continue;
        println("    .global %s", func->name);
        println("    .text");
        println("%s:", func->name);
        current_fn = func;

        // prologue
        println("    push %%rbp");
        println("    mov %%rsp, %%rbp");
        println("    sub $%d, %%rsp", func->stack_size);

        // save passed-by-register arguments to the stack
        int i = 0;
        for (Obj* var = func->params; var; var = var->next)
            store_gp(i++, var->offset, var->ty->size);

        // emit code
        gen_stmt(func->body);
        assert(depth == 0);

        println(".L.return.%s:", func->name);
        println("    mov %%rbp, %%rsp");
        println("    pop %%rbp");
        println("    ret");
    }
}

void codegen(Obj* prog, FILE* out)
{
    output_file = out;

    assign_lvar_offsets(prog);
    emit_data(prog);
    emit_text(prog);
}