/* This is the only file you should update and submit. */

/* Fill in your Name and GNumber in the following two comment fields
 * Name: Michael Teshome
 * GNumber: 01028913
 */

#include "logging.h"
#include "shell.h"

/* Feel free to define additional functions and update existing lines */

/* main */
/* The entry of your shell program */

int backgroundJob(char *path, char *cmds[], int n, char *og_command);
int foregroundJob(char *path, char *cmds [], int n);
void evaluateCmd(char *path, char *cmds [], int n, char *og_command);
void bg_handler(int sig);
int findJob(pid_t bg);

void remove_bgJob(pid_t bg);
void printJobs();
void kill_job(int jb, char *sig);
int containsJob(pid_t jb);
void fg_job(char *fg, char *job_Id);
void fgToBg(int job_Id);

void bg_job(char *fg, char *job_Id);
void bgCont(int job_Id); 

void controlZ(int sig);
void controlC(int sig);

typedef struct processes {
	pid_t bg;
	char *jobCommand;
	char *state;
	int jobId;
}bg_process;

bg_process * bg_list;
int bg_list_size = 1;
int numJobs = 0;
pid_t runningForeground;
char *fgCommand;

int main() 
{
    //Control C: Handler
    struct sigaction new, old;
    new.sa_handler = controlC;
    sigaction(SIGINT, &new, &old);
    
    //Control Z: Handler
    struct sigaction new2, old2;
    new2.sa_handler = controlZ;
    sigaction(SIGTSTP, &new2, &old2);
    //

    fgCommand = malloc(MAXLINE * sizeof(char));
    bg_list = malloc(1 * sizeof(bg_process));
    char cmdline[MAXLINE]; /* Command line */

    while (1) {
	/* Print prompt */
  	log_prompt();

	/* Read a line */
	// note: fgets will keep the ending '\'
	if (fgets(cmdline, MAXLINE, stdin)==NULL)
	{
	   	if (errno == EINTR)
			continue;
	    	exit(-1); 
	}

	if (feof(stdin)) {
	    	exit(0);
	}

	/* Parse command line and evaluate */
	strcpy(fgCommand, cmdline);
	char cmdLineCopy[MAXLINE];
	char * strings[50];

	int i;
	for(i = 0; i < MAXLINE; i++){
		
		if(cmdline[i] == '\n'){
			cmdLineCopy[i] = '\0';
		}
		else{
			cmdLineCopy[i] = cmdline[i];
		}
		
	} 
	
	const char split[5] = " ";
	char *splitStrings = strtok(cmdLineCopy, split);

	int count = 0;
	while(splitStrings != NULL){
		
		strings[count] = splitStrings;
		splitStrings = strtok(NULL, split);
		count += 1;
	}

	int commandCount = count;
	strings[commandCount] = NULL;

	

	evaluateCmd(strings[0], strings, commandCount, cmdline);
	
    } 
}
/* end main */ 

void evaluateCmd(char *path, char * cmds [], int n, char *og_command){
	
	char *command = cmds[0];
	
	if(strcmp(command, "help") == 0){
		log_help();
	}
	
	else if(strcmp(command, "quit") == 0){
		log_quit();
		exit(0);
	}

	else if(strcmp(cmds[n-1], "&") == 0){
		backgroundJob(path, cmds, n, og_command);
	}
	else if(strcmp(command, "jobs") == 0){
		printJobs();
	}
	else if(strcmp(command, "kill") == 0){
		int killProcess = atoi(cmds[2]);
		kill_job(killProcess, cmds[1]); 	
	}
	else if(strcmp(command, "fg") == 0){
		fg_job(cmds[0], cmds[1]);
	}
	else if(strcmp(command, "bg") == 0){
		bg_job(cmds[0], cmds[1]);
	}
	else{
		foregroundJob(path, cmds, n+1);
	}
}

int foregroundJob(char *path, char *cmds[], int n){
	char *args [n];
	int i = 0;
	for(i = 0; i < n; i++){
		args[i] = cmds[i];
	}

	pid_t process = fork();
	runningForeground = process;
	
	int childStatus;
	waitpid(process, &childStatus, 0);
	
	
	setpgid(0, 0);

	if(process == 0){
		int executed = execv(args[0], args);
		if(executed < 0){
			log_command_error(path);		
			exit(0);
		}
	}	

	if(WIFEXITED(childStatus)){
		log_job_fg_term(process, args[0]);
	}

	return childStatus;
}

int backgroundJob(char *path, char *cmds[], int n, char *og_command){
	
	char *args [n];
	int i;
	for(i = 0; i < n; i++){
		args[i] = cmds[i];
	}

	args[n-1] = NULL;

	pid_t process = fork();
	
	bg_list[bg_list_size - 1].bg = process;
	bg_list[bg_list_size -1].jobCommand = malloc(sizeof(char) * 50);
	strcpy(bg_list[bg_list_size -1].jobCommand, og_command);
	bg_list[bg_list_size - 1].state = "Running";
	bg_list[bg_list_size -1].jobId = bg_list_size;
	numJobs += 1;
	bg_list_size += 1;
	bg_process *newbg_list = realloc(bg_list, sizeof(bg_process) * bg_list_size);
	bg_list = newbg_list;

	struct sigaction new, old;
	new.sa_handler = bg_handler;
	new.sa_flags = 0;
	sigaction(SIGCHLD, &new, &old);

	if(process == 0){ 	

		int executed = execv(args[0], args);
		
		if(executed < 0){
			log_command_error(path);		
			exit(0);
		}
		
				
	}
	else{
		log_start_bg(process, args[0]);
		
	}
	
	
	return 0;
}


void bg_handler(int sig){
	if(sig == SIGCHLD){
		int childStatus;

		pid_t waited = waitpid(-1, &childStatus, WNOHANG);
		

		if(containsJob(waited) == 1 && waited != 0){
			if(WIFEXITED(childStatus)){
				int bg_index = findJob(waited);
				log_job_bg_term(bg_list[bg_index].bg, bg_list[bg_index].jobCommand);
		
				//Remove Background Job from List:
				bg_list[bg_index].bg = NULL;
				bg_list[bg_index].state = NULL;
				bg_list[bg_index].jobCommand = NULL;
				numJobs -= 1;
			}
		}
		
		
	
	}
}

void printJobs(){
	log_job_number(numJobs);
	int i;
	for(i = 0; i < bg_list_size; i++){

		if(bg_list[i].bg != NULL){
			int jobID = i + 1;
			pid_t pid = bg_list[i].bg;
			char *state = bg_list[i].state;
			char *cmd = bg_list[i].jobCommand;
			log_job_details(jobID, pid, state, cmd);
		}
	
	}
}

int findJob(pid_t bg){
	int i;
	int foundJob = 0;
	for(i = 0; i < bg_list_size; i++){
		if(bg_list[i].bg == bg){
			foundJob = i;
		}
	}

	return foundJob;
}

void kill_job(int jb, char *sig){
	char sigs [3];
	sigs[0] = sig[1]; 
	sigs[1] = sig[2];
	sigs[2] = '\0';
	
	int sigN = atoi(sigs);

	pid_t job = jb;
	int containsJB = containsJob(job);

	if(containsJB == 0){
		log_kill_error(jb, sigN);
	}
	else{
		int jobIndex = findJob(job);
		pid_t killProcess = bg_list[jobIndex].bg;
		kill(killProcess, sigN);
		bg_list[jobIndex].state = "Stopped";
		log_job_bg_stopped(killProcess, bg_list[jobIndex].jobCommand);
	}
}

int containsJob(pid_t jb){
	int contains = 0;
	int i;
	for(i = 0; i < bg_list_size; i++){
		if(bg_list[i].bg == jb){
			contains = 1;
		}
	}

	return contains;
}

void fg_job(char *fg, char *job_Id){	

	if(job_Id == NULL){
		if(bg_list[0].bg == NULL){
			log_fg_notfound_error(0);
		}
		else{
			fgToBg(0);
		}
	}
	else if(numJobs == 0){
		log_no_bg_error();
	}
	else {
		int jID = atoi(job_Id) - 1;
		if(jID < 0 || jID >= bg_list_size){
			log_fg_notfound_error(jID);
		}
		else{
			fgToBg(jID);
		}
	}

}


void fgToBg(int job_Id){
	pid_t converted = bg_list[job_Id].bg;
	kill(converted, 19);
	log_job_fg(converted, bg_list[job_Id].jobCommand);

	char *path = malloc(MAXLINE * sizeof(char));
	char cmdLineCopy[MAXLINE];
	char * strings[50];

	int i;
	for(i = 0; i < MAXLINE; i++){
		
		if(bg_list[job_Id].jobCommand[i] == '&'){
			cmdLineCopy[i] = '\0';
			path[i] = '\0';
			break;
		}
		else{
			cmdLineCopy[i] = bg_list[job_Id].jobCommand[i];
			path[i] = bg_list[job_Id].jobCommand[i];
		}
		
	} 
	
	const char split[5] = " ";
	char *splitStrings = strtok(cmdLineCopy, split);

	int count = 0;
	while(splitStrings != NULL){	
		strings[count] = splitStrings;
		
		splitStrings = strtok(NULL, split);
		count += 1;
	}

	int commandCount = count;
	strings[commandCount] = NULL;

	bg_list[job_Id].bg = NULL;
	bg_list[job_Id].state = NULL;
	bg_list[job_Id].jobCommand = NULL;
	numJobs -= 1;	

	int childStatus;
	kill(converted, 18);
	waitpid(converted, &childStatus, 0);

	if(converted == 0){
		int executed = execv(strings[0], strings);
		if(executed < 0){
				
			log_job_fg_fail(converted, path); 	
			exit(0);
		}
	}	

	if(WIFEXITED(childStatus)){
		log_job_fg_term(converted, strings[0]);
	}

	
}

void bg_job(char *fg, char *job_Id){
	if(job_Id == NULL){
		if(bg_list[0].bg == NULL){
			log_bg_notfound_error(0);
		}
		else{
			bgCont(0);
		}
	}
	else if(numJobs == 0){
		log_no_bg_error();
	}
	else {
		int jID = atoi(job_Id) - 1;
		if(jID < 0 || jID >= bg_list_size){
			log_bg_notfound_error(jID);
		}
		else{
			bgCont(jID);
		}
	}

}

void bgCont(int job_Id){
	bg_list[job_Id].state = "Running";
	log_job_bg(bg_list[job_Id].bg, bg_list[job_Id].jobCommand);
	log_job_bg_cont(bg_list[job_Id].bg, bg_list[job_Id].jobCommand);

	char *path = malloc(MAXLINE * sizeof(char));
	char cmdLineCopy[MAXLINE];
	char * strings[50];

	int i;
	for(i = 0; i < MAXLINE; i++){
		
		if(bg_list[job_Id].jobCommand[i] == '&'){
			cmdLineCopy[i] = '\0';
			path[i] = '\0';
			break;
		}
		else{
			cmdLineCopy[i] = bg_list[job_Id].jobCommand[i];
			path[i] = bg_list[job_Id].jobCommand[i];
		}
		
	} 
	
	const char split[5] = " ";
	char *splitStrings = strtok(cmdLineCopy, split);

	int count = 0;
	while(splitStrings != NULL){	
		strings[count] = splitStrings;
		
		splitStrings = strtok(NULL, split);
		count += 1;
	}

	int commandCount = count;
	strings[commandCount] = NULL;

	pid_t bgContinued = bg_list[job_Id].bg;
	kill(bgContinued, 18);


	if(bgContinued == 0){ 	

		int executed = execv(strings[0], strings);
		
		if(executed < 0){
			
			log_job_bg_fail(bgContinued, bg_list[job_Id].jobCommand);		
			exit(0);
		}
		
				
	}

}

void controlZ(int sig){
	if(sig == SIGTSTP){
		kill(runningForeground, 18);
		log_job_fg_stopped(runningForeground, fgCommand);

		bg_list[bg_list_size - 1].bg = runningForeground;
		bg_list[bg_list_size -1].jobCommand = malloc(sizeof(char) * 50);
		strcpy(bg_list[bg_list_size -1].jobCommand, fgCommand);
		bg_list[bg_list_size - 1].state = "Stopped";
		bg_list[bg_list_size -1].jobId = bg_list_size;
		numJobs += 1;
		bg_list_size += 1;
		bg_process *newbg_list = realloc(bg_list, sizeof(bg_process) * bg_list_size);
		bg_list = newbg_list;
		
	}
}

void controlC(int sig){
	if(sig == SIGINT){
		kill(runningForeground, 19);
		log_job_fg_term_sig(runningForeground, fgCommand);
	}
	
}
