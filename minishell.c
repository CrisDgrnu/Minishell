#include <stdio.h>

#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "parser.h"
#include <fcntl.h>

// Cambiar de directorio
void cd(tline* line){
    char *home_dir;
    int handler;

    // Si se ejecuta cd sin argumentos, cambiamos a home
    if (line->commands[0].argv[1] == NULL)
    {
        // Intentamos cambiar de directorio
        home_dir = getenv("HOME");
        handler = chdir(home_dir);

        // En caso de error
        if (handler<0)
            fprintf(stderr,"Error al cambiar de directorio\n");
    } 
    else
    {   
        // Intentamos cambiar de directorio
        handler = chdir(line->commands[0].argv[1]);

        // En caso de error
        if (handler<0)
            fprintf(stderr,"Error al cambiar al directorio: %s\n",line->commands[0].argv[1]);
    }
}

// Crea las redirecciones
void create_redirect(int fhandler,int std_redirect){

    // En caso de error
    if (fhandler<0)
    {
        fprintf(stderr,"Error at redirecting to file descriptor %d",std_redirect);
        exit(1);
    }

    // Redireccion en función del descriptor de fichero
    dup2(fhandler,std_redirect);
    close(fhandler);
}

// Cambia las entradas/salidas en funcion del comando
void setup_redirects(tline* line, int ccmds){
    int fhandler;

    // Redirección de entrada estandar
    if (ccmds == 0 && (line->redirect_input != NULL))
    {
        fhandler = open(line->redirect_input,O_RDONLY);
        create_redirect(fhandler,0);
    }

    // Redirección de salida estandar
    if (ccmds == (line->ncommands-1) && (line->redirect_output != NULL))
    {
        fhandler = creat(line->redirect_output,0644);
        create_redirect(fhandler,1);
        
    }

    // Redirección de salida de error
    if (ccmds == (line->ncommands-1) && (line->redirect_error != NULL))
    {
        fhandler = creat(line->redirect_error,0644);
        create_redirect(fhandler,2);
        
    }
}

// Cierra los pipes y libera la memoria utilizada por ellos
void close_pipes(tline* line,int** p){
    int cont;
    for (cont = 0; cont < line->ncommands-1; cont++)
    {
        close(p[cont][0]);
        close(p[cont][1]);
        free(p[cont]);
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

// Configura las entradas y salidas de los comandos en funcion de los pipes
void setup_pipes(tline* line,int** pip,int ccmds){
    
    // Redireccion del primer pipe
    if (ccmds == 0)
        dup2(pip[0][1],1);

    // Redirección del ultimo pipe
    else if (ccmds == (line-> ncommands-1))
        dup2(pip[ccmds-1][0],0);

    // Redirección de pipes intermedios
    else
    {
        dup2(pip[ccmds][1],1);
        dup2(pip[ccmds-1][0],0);
    }

    // Cierre de pipes en subproceso
    close_pipes(line,pip);
}

void execute(tline* line){
    int ccmds;
    pid_t pid;
    int** pip;
    int *pids;

    pids = malloc(sizeof(pid_t) * line->ncommands);

    // Creacion de pipes
    if (line->ncommands > 1)
        pip = create_pipes(line);
    

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

        // Se guardan los pids de los procesos a ejecutar
        pids[ccmds] = pid;

        // Ejecucion del comando en un subproceso
        if (pid == 0)
        {
            // Si no hay procesos en background, se activan las señales
            if (!line->background)
            {
                signal(SIGINT,SIG_DFL);
                signal(SIGQUIT,SIG_DFL);
            }
            
            // Ejecucion con redirecciones
            if ((line->redirect_input != NULL) || (line->redirect_output != NULL) || (line->redirect_error != NULL))
                setup_redirects(line,ccmds);
            
            // Ejecucion con pipes -> Deben redigirse las entradas y salidas
            if (line->ncommands > 1)
                setup_pipes(line,pip,ccmds);
            
            // Ejecicion del comando
            execvp(line->commands[ccmds].filename,line->commands[ccmds].argv);
            exit(1);
        }            
    }
    
    // Cierre de pipes en proceso padre
    if (line->ncommands>1)
        close_pipes(line,pip);
    
    // Esperar a los procesos que no ejecutan en bacnkground
    if (!line->background)
    {
        for (ccmds = 0; ccmds < line->ncommands; ccmds++)
            waitpid(pids[ccmds],NULL,0);
        
    }

    // Mostrar aquellos que ejecuten en background
    else{
        for (ccmds = 0; ccmds < line->ncommands; ccmds++)
            printf("[%d]\n",pids[ccmds]);
        
    }
    free(pids);
    
}

// Manejador para procesos en background
void background_management(int sig){
    waitpid(WAIT_ANY,NULL,WNOHANG);
}

int main(void) {
	char buf[1024];
	tline *line;
    
    signal(SIGINT,SIG_IGN);
    signal(SIGQUIT,SIG_IGN);

    signal(SIGCHLD,background_management);

	printf("==> ");	
	while (fgets(buf, 1024, stdin)) 
    {
        line = tokenize(buf);
		if (line->ncommands>0){
            if (strcmp(line->commands[0].argv[0],"cd") == 0)
                cd(line);
        
            execute(line);
        } 
  
        printf("==> ");	
	}
	return 0;
}
