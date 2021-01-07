#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "parser.h"
#include <fcntl.h>

// Tipo de dato Job para almacenar procesos en segundo plano
typedef struct Job {
    pid_t pid; 
    char buf[1024];
    struct Job* next; 
} Job;

// Lista de procesos en segundo plano
typedef struct Jobs{
    Job* head;
} Jobs;

Jobs* jobs;

// Almacenar proceso en segundo plano
Job* createNode(char buf[1024],pid_t pid){
    Job* job = (Job *) malloc(sizeof(Job));
    strcpy(job->buf,buf);
    job->pid = pid;
    job->next = NULL;
    return job;
}

// Construir lista
Jobs* buildList(){
    jobs = (Jobs *) malloc(sizeof(Jobs));
    jobs->head = NULL;
    return jobs;
}

// Comprobar si la lista esta vacia
int isEmpty(){
    return jobs->head == NULL;
}

// Añadir proceso
void add(char buf[1024],pid_t pid){
    Job* job = createNode(buf,pid);
    job->next = jobs->head;
    jobs->head = job;
}

// Borrar proceso
void delete(pid_t pid){
    if (jobs->head)
    {
        if (pid == jobs->head->pid)
        {
            Job* deleted = jobs->head;
            jobs->head = jobs->head->next;
            free(deleted);
        }
        else{
            Job* job = jobs->head;
            while (pid != job->next->pid)
                job = job->next;

            Job* deleted = job->next;
            job->next = deleted->next;
            free(deleted); 
        }
               
    }
    
}

// Encontrar un trabajo en la lista
Job* find_job(int pid){
    if (jobs->head == NULL)
        return NULL;
    else{
        Job* job = jobs->head;
        while (job->next)
        {
            if (job->pid == pid)
                return job;
            job = job->next;
        }
        return NULL;
    }
}

// Ver procesos en segundo plano
void check_jobs(){
    Job* suc = jobs->head;
    while (suc != NULL){
        printf("[%d] Ejecutando  =>  %s",suc->pid,suc->buf);
        suc = suc->next;
    }
}

// Manejador para procesos en background
void background_management(int sig){
    pid_t pid = waitpid(WAIT_ANY,NULL,WNOHANG);
    if ((pid<0) && (find_job(pid) != NULL))
    {
        fprintf(stderr,"Error al eliminar proceso en segundo plano\n");
        exit(1);
    } 
    if (pid>0) 
		delete(pid);
}

// Traer ultimo proceso ejecutando en segundo plano a primer plano
void fg(){
    pid_t pid;
    // Si no hay procesos en segundo plano error
    if (isEmpty(jobs))
        fprintf(stderr,"No hay procesos en segundo plano\n");
    else
    {   
        signal(SIGCHLD,SIG_IGN);
        pid = jobs->head->pid;
        printf("%s\n",jobs->head->buf);
        waitpid(pid,NULL,0);
        signal(SIGCHLD,background_management);
        delete(pid);
    }    
}

// Traer proceso de segundo plano a primer plano por pid
void fg_pid(pid_t pid){
    if (isEmpty(jobs))
        fprintf(stderr,"No hay procesos en segundo plano\n");
    else
    {   
        signal(SIGCHLD,SIG_IGN);
        printf("%s\n",find_job(pid)->buf);
        waitpid(pid,NULL,0);
        signal(SIGCHLD,background_management);
        delete(pid);
    }    
}

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
        fprintf(stderr,"Error al redireccionar a %d",std_redirect);
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

void execute(tline* line,char buf[1024]){
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

        if (line->background)
            add(buf,pid);

        // Se guardan los pids de los procesos a ejecutar
        pids[ccmds] = pid;

        // Ejecucion del comando en un subproceso
        if (pid == 0)
        {

            // Si no hay procesos en background, se activan las señales por defecto
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
            if (line->commands[ccmds].filename==NULL){
				  fprintf(stderr,"%s: Error comando no encontrado\n",line->commands[ccmds].argv[0]);
            exit(1);

			}
            execvp(line->commands[ccmds].filename,line->commands[ccmds].argv);
            fprintf(stderr,"Error\n");
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

int main(void) {
	char buf[1024];
	tline *line;
    jobs = buildList();

    signal(SIGINT,SIG_IGN);
    signal(SIGQUIT,SIG_IGN);

    signal(SIGCHLD,background_management);

	printf("==> ");	
	while (fgets(buf, 1024, stdin)) 
    {
		if (buf[0]!='\n'){
			
			line = tokenize(buf);
			if (line==NULL){
				continue;
			} 

			if (strcmp(line->commands[0].argv[0],"cd") == 0)
				cd(line);
            else if (strcmp(line->commands[0].argv[0],"jobs") == 0)            
                check_jobs(jobs);
            else if ((strcmp(line->commands[0].argv[0],"fg") == 0) && line->commands[0].argv[1] != NULL) 
                fg_pid(atoi(line->commands[0].argv[1]));
            else if ((strcmp(line->commands[0].argv[0],"fg") == 0) && line->commands[0].argv[1] == NULL)
                fg();
			else
				execute(line,buf);
		}
        printf("==> ");	
	}
	return 0;
}