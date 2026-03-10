#include <stdio.h>

int main()
{
    char *a = (char *)malloc(24);
    char *b = (char *)malloc(40);
    char *c = (char *)malloc(8);
    printf("allocated a=%p b=%p c=%p\n", (void *)a, (void *)b, (void *)c);

    free(b);
    printf("freed b\n");

    char *d = (char *)malloc(32);
    printf("allocated d=%p\n", (void *)d);

    free(a);
    free(c);
    free(d);

    char *p = (char *)malloc(10);
    strcpy(p, "hello");
    p = (char *)realloc(p, 100);
    printf("realloc result: %s\n", p);
    free(p);

    return 0;
}
