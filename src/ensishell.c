/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include "variante.h"
#include "readcmd.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>

// struct de jobs
struct JOB {
	char** name;
	pid_t pid;
	int status;
};

static int nombreJobs = 0;
static struct JOB* allJobs;

int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */
	printf("Not implemented yet: can not execute %s\n", line);

	/* Remove this line when using parsecmd as it will free it */
	free(line);

	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}

/*
Appel d'une commande

@numPipe: 0 ou 1 selon que la commmanbde soit appelée en premier ou en 2
*/
void runcmd(char **cmd, int background, char* input, char* output, int pipeOutput[2], int pipeInput[2]){
	char *cmdExec = cmd[0];
	char *job = "jobs";
	int child_status;

	if (strcmp(cmdExec, job) == 0){
		// Appel de la commande jobs.
		for (int i = 0; i < nombreJobs; i++) {
			printf("[%i]        ", i+1);
			if (allJobs[i].status == 1){
				printf("[Stopped]       ");
			} else {
				printf("[Processing]         ");
			}
			for (int j = 0; allJobs[i].name[j] != 0; j++){
				printf("%s ", allJobs[i].name[j]);
			}
			printf("\n");
		}
	} else {
		pid_t f = fork();

		if (f == -1){
			fprintf(stderr, "Erreur dans le fork");
			exit(1);
		} else if (f == 0){ // Nouveau processus
			int inputFile = -1;
			int outputFile = -1;
			// Gestion des entrees sorties
			if (output){
				// Si un fichier sortie est proposé.
				outputFile = open(output, O_CREAT|O_WRONLY, 0777);
				dup2(outputFile, STDOUT_FILENO);
			}
			if (inputFile){
				// Si un fichier d'entrée est proposé
				inputFile = open(input, O_RDONLY);
				// Redirection vers l'entrée.
				dup2(inputFile, STDIN_FILENO);
			}


			if (pipeOutput[0] != -1){
			 	dup2(pipeOutput[1], STDOUT_FILENO);
			}
			if (pipeInput[1] != -1){
			 	dup2(pipeInput[0], STDIN_FILENO);
			}

			execvp(cmdExec, cmd);

			// On ferme les fichiers
			if (inputFile != -1){
				close(inputFile);
			}
			if (outputFile != -1){
				close(outputFile);
			}

			exit(0);
		} else { // Processus parent attend la fin du premier processus.*
			if (nombreJobs < 15){
				int lengthCmd = 0;
				while(cmd[lengthCmd] != NULL){
					lengthCmd++;
				}
				char **copyParam = malloc(lengthCmd*sizeof(char*) + 1);
				for (int i=0; i < lengthCmd; i++){
					copyParam[i] = malloc(sizeof(cmd[i]) + 1);
					memcpy(copyParam[i], cmd[i], sizeof(cmd[i])/sizeof(char) + 1);
				}
				allJobs[nombreJobs] = (struct JOB){.name = copyParam, .pid = f, .status = 0};
				nombreJobs ++;
			}

			if (background == 0){
				int currentProcess = nombreJobs - 1;
				// On attend que le job soit fini pour mettre son status à 1.
				waitpid(allJobs[nombreJobs-1].pid, &child_status, 0);
				while (!WIFEXITED(child_status) && !WIFSIGNALED(child_status)){
					waitpid(allJobs[nombreJobs-1].pid, &child_status, 0);
				}
				allJobs[currentProcess].status = 1;
			}
		}
	}
}


int main() {
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	allJobs = malloc(16 * sizeof(struct JOB));

	while (1) {
		struct cmdline *l;
		char *line=0;
		int i, j;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {

			terminate(0);
		}



		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);


		int background = 0;
		if (l->bg){
			background = 1;
			printf("background (&)\n");
		}

		int tailleCommandes = 0;
		/* Display each command of the pipe */
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
            for (j=0; cmd[j]!=0; j++) {
                printf("'%s' ", cmd[j]);
            }
			tailleCommandes ++;
			printf("\n");
		}

		int pipeInput[2] = {-1, -1};
		int pipeOutput[2] = {-1, -1};

		int nbInstructions = 0;
		/* Execution des commandes */
		for (i=0; l->seq[i]!=0; i++) {
			nbInstructions++;
		}

		for (i=0; i < nbInstructions; i++){
			pipeInput[0] = pipeOutput[0];
			pipeInput[1] = pipeOutput[1];
			if(i != nbInstructions-1){
				if(pipe(pipeOutput) != 0){
					fprintf(stderr, "Erreur dans la creation du pipe à l'étape %i\n", i);
					exit(1);
				}
			}
			else{
				pipeOutput[0] = -1;
				pipeOutput[1] = -1;
			}
			char **cmd = l->seq[i];
			runcmd(cmd, background, l->in, l->out, pipeOutput, pipeInput);
			//there seems to be no issue closing non existing pipes
			// so the next 2 line work regardless of pipe initialization
			close(pipeOutput[1]);
			close(pipeInput[0]);
		}

		if (tailleCommandes != 1){
			dup2(1, pipeInOut[0]);
		}

		// On close le pipe
		close(pipeInOut[0]);
		close(pipeInOut[1]);
	}
	return EXIT_SUCCESS;
}
