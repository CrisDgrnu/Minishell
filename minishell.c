#include <stdio.h>

#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>



#include "parser.h"

void execute(tline* line){
    int ncommands;
    pid_t pid;

    // Cada comando trata de ejecutarse en un subproceso
    for (ncommands = 0; ncommands < line->ncommands; ncommands++)
    {

        // Creacion de subproceso
        pid = fork();

        // Error al crear el subproceso
        if (pid<0)
        {
            fprintf(stderr,"Error creating subprocess\n");
            exit(1);
        }

        // Ejecucion del comando en un subproceso
        if (pid == 0)
        {
            execvp(line->commands[ncommands].filename,line->commands[ncommands].argv);
            exit(1);
        }    
    }
    
    // Esperar hasta que cada subproceso termine
    for (ncommands = 0; ncommands < line->ncommands; ncommands++)
    {
        wait(NULL);
    }
    
}

int main(void) {
	char buf[1024];
	tline * line;
    
	printf("==> ");	
	while (fgets(buf, 1024, stdin)) 
    {
		
        line = tokenize(buf);
		if (line==NULL) 
        {
			continue;
		}

        execute(line);
        printf("==> ");	
	}
	return 0;
}
