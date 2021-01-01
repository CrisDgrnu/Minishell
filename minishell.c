#include <stdio.h>

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "parser.h"

// Cambiar de directorio
void cd(tline* line){
    char* home_dir;
    int handler;

    // Si se ejecuta cd sin argumentos, cambiamos a home
    if (line->commands[0].argv[1] == NULL)
    {
        // Intentamos cambiar de directorio
        home_dir = getenv("HOME");
        handler = chdir(home_dir);

        // En caso de error
        if (handler<0)
        {
            fprintf(stderr,"Error al cambiar de directorio\n");
        }
    } 
    else
    {   
        // Intentamos cambiar de directorio
        handler = chdir(line->commands[0].argv[1]);

        // En caso de error
        if (handler<0)
        {
            fprintf(stderr,"Error al cambiar al directorio: %s\n",line->commands[0].argv[1]);
        }
        
    }
}

// Cierra los pipes y libera la memoria utilizada por ellos
void close_pipes(tline* line,int** p){
    int i;
    for (i = 0; i < line->ncommands-1; i++)
    {
        close(p[i][0]);
        close(p[i][1]);
        free(p[i]);
    }
    free(p);
}

// Crea y devuelve los pipes 
int** create_pipes(tline* line){
    int** p;
    int cont;

    // Se reserva memoria guardar todos los pipes
    p = malloc(sizeof(int*)* line->ncommands-1);

    // Se guarda cada pipe en una posicion del array
    for (cont = 0; cont < line->ncommands-1; cont++)
    {
        p[cont] = malloc(sizeof(int)*2);
        pipe(p[cont]);
    } 

    return p;

}
void execute(tline* line){
    int ccmds;
    pid_t pid;
    int** pip;

    // Creacion de pipes
    if (line->ncommands > 1)
    {
        pip = create_pipes(line);
    }
    

    // Cada comando trata de ejecutarse en un subproceso
    for (ccmds = 0; ccmds < line->ncommands; ccmds++)
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
            
            // Ejecucion con pipes -> Deben redigirse las entradas y salidas
            if (line->ncommands > 1)
            {
                // Redireccion del primer pipe
                if (ccmds == 0)
                {
                    dup2(pip[0][1],1);
                }

                // Redirección del ultimo pipe
                else if (ccmds == (line-> ncommands-1))
                {
                    dup2(pip[ccmds-1][0],0);
                }

                // Redirección de pipes intermedios
                else
                {
                    dup2(pip[ccmds][1],1);
                    dup2(pip[ccmds-1][0],0);
                }

                // Cierre de pipes en subproceso
                close_pipes(line,pip);
            }
            
            
            execvp(line->commands[ccmds].filename,line->commands[ccmds].argv);
            exit(1);
        }            
    }
    
    // Cierre de pipes en proceso padre
    if (line->ncommands>1)
    {
        close_pipes(line,pip);
    }
    
    // Esperar hasta que cada subproceso termine
    for (ccmds = 0; ccmds < line->ncommands; ccmds++)
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
