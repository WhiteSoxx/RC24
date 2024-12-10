#include <stdio.h>
#include <stdlib.h>



int main(int argc, char *argv[])
{
    for(int i = 0; i < argc; i++)
    {
        printf("argv[%d]: %s\n", i, argv[i]);
    }

    char *GSIP = "localhost";
    int GSPort = 1234;

    if (argv[1] != NULL)
    {
        GSIP = argv[2];

    }

    if (argv[3] != NULL)
    {
        GSPort = atoi(argv[4]);
    }

    printf("GSIP: %s\n", GSIP);
    printf("GSPort: %d\n", GSPort);
    
    return 0;
}
