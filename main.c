#include "au_cc.h"

int main(int argc, char **argv)
{
    if (argc != 2)
        error("%s: invalid number of arguments\n", argv[0]);

    // printf("    mov $%ld, %%rax\n", get_number(tok)); // strtol converts the beginning of operations into long int and stores the rest of them in &operations)

    // tokenize and parse
    Token *tok = tokenize(argv[1]);
    Obj *prog = parse(tok);
    codegen(prog);
    return 0;
}