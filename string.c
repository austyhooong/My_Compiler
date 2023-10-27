#include "au_cc.h"

char *format(char *fmt, ...)
{
    char *buf;
    size_t buflen;
    FILE *out = open_memstream(&buf, &buflen); // open a dynamic memory buffer stream
    // buf & buflen are updated upon fflush or fclose

    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap); // sends fmt to out
    va_end(ap);
    fclose(out);
    return buf;
}

// sprintf(buf, ".L..%d", id++);  copy second argument (expanded output) to first arg