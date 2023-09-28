#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, "%s: invalid number of arguments\n", argv[0]);
        return 1;
    }

    char *operations = argv[1];
    printf("    .global main\n");
    printf("main:\n");
    printf("    mov $%ld, %%rax\n", strtol(operations, &operations, 10)); // strtol converts the beginning of operations into long int and stores the rest of them in &operations)

    while (*operations)
    {
        if (*operations == '+')
        {
            ++operations;
            printf("    add $%ld, %%rax\n", strtol(operations, &operations, 10));
        }
        else if (*operations == '-')
        {
            ++operations;
            printf("    sub $%ld, %%rax\n", strtol(operations, &operations, 10));
        }
        else
        {
            fprintf(stderr, "unexpected operator: '%c'", *operations);
            return 1;
        }
    }
    printf("    ret\n");
    return 0;
}
