#include <stdio.h>

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "parser.h"

void cd(tline* line){
    char* home_dir;
    int handler;

    if (line->commands[0].argv[1] == NULL)
    {
        home_dir = getenv("HOME");
        handler = chdir(home_dir);
        if (handler<0)
        {
            fprintf(stderr,"Error al cambiar de directorio\n");
        }
    } 
    else
    {
        handler = chdir(line->commands[0].argv[1]);
        if (handler<0)
        {
            fprintf(stderr,"Error al cambiar al directorio: %s\n",line->commands[0].argv[1]);
        }
        
    }
    
    
}

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

        if (strcmp(line->commands[0].argv[0],"cd") == 0)
        {
            cd(line);
        }
        
        execute(line);
        printf("==> ");	
	}
	return 0;
}
