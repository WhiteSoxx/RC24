#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(int argc, char *argv[])
{
    int GSPort = 1234;
    int verbose = 0;

    for(int i = 0; i < argc; i++)
    {
        printf("argv[%d]: %s\n", i, argv[i]);
        if(!strcmp(argv[i], "-p"))
        {
            printf("p found\n");
            GSPort = atoi(argv[i+1]);
            i++;
        }
        else if(!strcmp(argv[i], "-v"))
        {
            printf("v found\n");
            verbose = 1;
        }
    }

    printf("GSPort: %d\n", GSPort);
    printf("verbose: %d\n", verbose);
    
    return 0;
}
