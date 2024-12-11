#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GN 54

int main(int argc, char *argv[])
{
    char *GSIP = "localhost";
    int GSPort = 58000+GN;

    for(int i = 0; i < argc; i++)
    {
        printf("argv[%d]: %s\n", i, argv[i]);
        if(!strcmp(argv[i], "-n"))
        {
            printf("n found\n");
            GSIP = argv[i+1];
            i++;
        }
        else if(!strcmp(argv[i], "-p"))
        {
            printf("p found\n");
            GSPort = atoi(argv[i+1]);
            i++;
        }
    }

    printf("GSIP: %s\n", GSIP);
    printf("GSPort: %d\n", GSPort);
    
    return 0;
}
