/*
 * * Copyright (C) 2013 Chris Procter
 * *
 * * This file is part of tap-harness
 * *
 * * This copyrighted material is made available to anyone wishing to use,
 * * modify, copy, or redistribute it subject to the terms and conditions
 * * of the GNU General Public License v.2.
 * *
 * * You should have received a copy of the GNU General Public License
 * * along with this program; if not, write to the Free Software Foundation,
 * * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 * */

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/poll.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>

#include "harness.h"

/*
#define OK 0
#define NOTOK 1
#define ABORTED 2
#define SKIPPED 3
#define TODO 4
#define MISSING 5

struct test {
	int number;
	int result;
	int directive;
	char * description;
	char * reason;
	struct test *next;
};

struct data {
	char * key;
	char * value;
	struct data *next;
};

struct testsuite {
	FILE * output;		//process file descriptor
	char * filename;
	int directive;
	char * reason;
	int plannedcount;
	int testcount;
	int result;
	struct test *tests;		//first test run
	struct test *current;		//test running atm / last test run (so its easy to append without traversing the entire ll
	struct list *metadata;
	struct testsuite *next;
};

struct list {
	struct data * base;
	struct data * head;
};

struct testgroup {
	struct testsuite * base;
	struct testsuite * head;
};

struct output {
	void (*base)(struct list *env,struct testgroup *tg);
	void (*head)(struct list *env,struct testgroup *tg);
};
*/

struct list environment;  //description of the machine ip, hardware type etc
		

/*************************************/

struct syntax {
	char * string;
	int (*handler)(int testno,char* description, char * directive, char * reason ,struct testsuite *ts);
};

int cb_ok(int testno,char* description, char * directive, char * reason ,struct testsuite *ts);
int cb_plan(int testno,char* description, char * directive, char * reason ,struct testsuite *ts);
int cb_notok(int testno,char* description, char * directive, char * reason ,struct testsuite *ts);
int cb_bailout(int testno,char* description, char * directive, char * reason ,struct testsuite *ts);
int cb_skipline(int testno,char* description, char * directive, char * reason ,struct testsuite *ts);

char * splitstring(char c, char* string); 


struct syntax callbacks[] = {
	{"not ok"       ,&cb_notok      },
	{"ok"			,&cb_ok	 		},
	{"Bail out!"    ,&cb_bailout    },
	{"#"			,&cb_skipline   },
	{"1.."			,&cb_plan		},
	{NULL			,NULL      		}
};


/*
struct _directives directives[] = {
	{"TODO"	}
	{"SKIP" }
	{"TAG" }
}*/

/***************************************
 *
 * *************************************/
char * ltrim(char * string){
	if(string==NULL){
		return NULL;
	}
	while(isspace(*string)){
		string++;
	}
	return string;
}


/***************************************
 *
 * *************************************/
void rtrim(char * string){
	int len;

	if(string==NULL){
		return;
	}
	len=strlen(string)-1;
	while(isspace(*(string+len))&&(len!=0)){
		len--;
	}
	*(string+len+1)=0;
	
	return;
}


/***************************************
 *
 * *************************************/
void * x_malloc(size_t size) {
	void *p;

	p = malloc(size);
	if (p == NULL){
		fprintf(stderr,"failed to malloc %lu bytes", (unsigned long) size);
		exit(100);
	}
	return p;
}


/***************************************
 *
 * *************************************/
char * copystring(char * string){
	char *tmp;

	if(string==NULL){
		return NULL;
	}
	//printf("copying +%s+\n",string);
	tmp=x_malloc(strlen(string)+1);
	strncpy(tmp,string,strlen(string)+1);

	return tmp;
}


/***************************************
 *
 * *************************************/
void addtest(struct testsuite* ts){
	struct test *t;
	
	t=(struct test*)x_malloc(sizeof(struct test));
	t->result=-1;
	t->directive=-1;
	t->description=NULL;
	t->reason=NULL;
	t->next=NULL;

	if(ts->current != NULL){
		t->number = ts->current->number +1;
		ts->current->next=t;
		ts->testcount++;
	}else{
		ts->tests=t;
		t->number = 1; 
		ts->testcount=1;
	}

	ts->current = t;

	//printf("adding test number %d count=%d\n",ts->tests->number,ts->testcount);
}


/***************************************
 *
 * *************************************/
int validatettestno(int testno,struct testsuite *ts){

	if((testno > -1) && (ts->testcount < testno)){
		///testno higher then expected
		while(ts->testcount < testno){
			addtest(ts);
			ts->current->result=MISSING;
		}
	}else if((testno > -1) && (ts->testcount > testno)){
		///tesetno lower then expected
		testno=ts->testcount;
		//ts->current->reason="invalid test number found";
		//ts->current->result=NOTOK;
	}

	if(testno==-1){
		testno=ts->testcount;
	}
	if((ts->plannedcount != -1)&&(testno > ts->plannedcount)) {
		///testno greater then the plan allows
		ts->result=NOTOK;
		ts->reason="more tests then expected in plan";
	}

	return testno;
}



/***************************************
 *
 * *************************************/
int cb_ok(int testno,char* description, char * directive, char * reason ,struct testsuite *ts){
	int number;

	addtest(ts);
	number=validatettestno(testno,ts);

	ts->current->result=OK;
	ts->current->number=number;

	if(strncmp(directive,"TODO",4)==0){
		ts->current->directive=TODO;
	} else if(strncmp(directive,"SKIP",4)==0){
		ts->current->directive=SKIPPED;
	}
	ts->current->description=copystring(description);
	//printf("testno=%d reason=%s\n",testno,reason);
	ts->current->reason=copystring(reason);

	return 0;
}


/***************************************
 *
 * *************************************/
int cb_notok(int testno,char* description, char * directive, char * reason ,struct testsuite *ts){
	int number;

	addtest(ts);
	number=validatettestno(testno,ts);
	ts->current->number=number;

	ts->current->result=NOTOK;

	if(strncmp(directive,"TODO",4)==0){
		ts->current->directive=TODO;;
		ts->current->result=OK;
	} else if(strncmp(directive,"SKIP",4)==0){
		ts->current->directive=SKIPPED;
	}

	ts->current->description=copystring(description);
	//printf("testno=%d reason=%s\n",testno,reason);
	ts->current->reason=copystring(reason);

	return 0;
}


/***************************************
 *
 * *************************************/
int cb_bailout(int testno,char* description, char * directive, char * reason ,struct testsuite *ts){

	ts->result=ABORTED;
	return 0;
}


/***************************************
 *
 * *************************************/
void adddata(char * string,struct list *ls){
	struct data *d;
	
	d=x_malloc(sizeof(struct data));

	d->value=copystring(splitstring('=',string));
	d->key=copystring(string);
	//printf("key=%s value=%s\n",d->key,d->value);
	d->next=NULL;

	if(ls->head != NULL){
		ls->head->next = d;
	}else{
		ls->base = d;
	}
	ls->head = d;

	return;	
}

/***************************************
 *
 * *************************************/
int cb_skipline(int testno,char* description, char * directive, char * reason, struct testsuite *ts){

	if(strncmp(directive,"TAG",3)==0){
		adddata(reason,ts->metadata);
	}else if(strncmp(directive,"ENV",3)==0){
		adddata(reason,&environment);
	}
	return 0;
}


/***************************************
 *
 * *************************************/
int cb_plan(int testno,char* description, char * directive, char * reason ,struct testsuite *ts){

	if(testno < 0){
		ts->result=NOTOK;
		ts->reason="invalid plan found";
		return 0;
	}

	if(ts->testcount !=-1){//we've alreay seeen a plan
		ts->result=NOTOK;
		ts->reason="multiple plans found";
		return 0;
	}

	if(testno > 0){
		ts->plannedcount = testno;
	} else {
		ts->plannedcount=0;
		ts->result=SKIPPED;
		ts->reason=reason;
	}

	return 0;
}



/***************************************
 *
 * *************************************/
char * splitstring(char c, char* string) {
	int offset=0;

	if(string == NULL) {
		return NULL;
	}

	string = ltrim(string);

	while((*(string+offset) != c) && (*(string+offset) != '\0')){
		offset++;
	}

	if(*(string+offset) == c) {
		*(string+offset) = 0;
		rtrim(string+offset+1);
		return ltrim(string+offset+1);
	}else{
		return (string+offset);
	}
}


/***************************************
 *
 * *************************************/
int parse_line(char * line, struct testsuite *ts) {
	int i=0;
	int retval;
	char *content;
	char * description=NULL;
	char * directive=NULL;
	char * reason=NULL;
	int testno;

	line=ltrim(line);
	rtrim(line);

	//callbacks[i].string testno description #  directive  reason
	while(callbacks[i].string != NULL){
		if(strncmp(line,callbacks[i].string,strlen(callbacks[i].string))==0) {
			content=line+strlen(callbacks[i].string);
			testno = strtol(content, (char **) &content, 10);
			if(content == (line+strlen(callbacks[i].string))) {
				testno = -1;
			}
			description = content;
			directive = splitstring('#',description);
			reason = splitstring(' ',directive);

			retval=(callbacks[i].handler)(testno, description, directive, reason, ts);
			if(retval == 1) {
				return 1;
			} else if(retval == 0) {
				return 0;
			} else {
				;
			}
		}
		i++;
	}
	return -1;
}


/***************************************
 *
 * *************************************/
pid_t test_start(const char *path, int *fd) {
    int fds[2], errfd;
    pid_t child;

    if (pipe(fds) == -1) {
		fprintf(stderr,"ABORTED can't create pipe\n");
		exit(1);
    }

    child = fork();
    if (child == (pid_t) -1) {
		fprintf(stderr,"ABORTED can't fork\n");
		exit(2);
    } else if (child == 0) {
		/* In child.  Set up our stdout and stderr. */
		errfd = open("/dev/null", O_WRONLY);
		if (errfd < 0)
		    exit(3);
		if (dup2(errfd, 2) == -1)
		    exit(4);
		close(fds[0]);
		if (dup2(fds[1], 1) == -1)
		    exit(5);

		/* Now, exec our process. */
		if (execl(path, path, (char *) 0) == -1)
		    exit(6);
   	} else {
		/* In parent.  Close the extra file descriptor. */
		close(fds[1]);
    }
    *fd = fds[0];
    return child;
}


void addtestsuite(char * filename,struct testgroup * tg){
	struct testsuite *ts;

	ts=x_malloc(sizeof(struct testsuite));

	ts->filename=filename;
	ts->directive=-1;
	ts->reason=NULL;
	ts->plannedcount=-1;
	ts->testcount=-1;
	ts->result=-1;
	ts->tests=NULL;
	ts->current=NULL;
	ts->metadata=x_malloc(sizeof(struct list));
	ts->metadata->base=NULL;
	ts->metadata->head=NULL;
	ts->next=NULL;

	if(tg->head != NULL){
		tg->head->next=ts;
	}else{
		tg->base=ts;
	}

	tg->head=ts;

}


int readresultsfile(char * logfile,struct testgroup *tg){
	FILE * log;
	char buffer[BUFSIZ];
	int count=0;
	char *filename;

	log=fopen(logfile,"r");
	while(fgets(buffer, sizeof(buffer), log)){
		//printf("read %s",buffer);
		if(strncmp("## FILE:",buffer,8)==0){
			//its a new file
			filename = copystring(splitstring(':',buffer));
			addtestsuite(filename,tg);
		}else{
			parse_line(buffer,tg->head);
		}
	}

	fclose(log);
	return count;
}


/***************************************
 *
 * *************************************/

void runtests(time_t endtime, int concurrent, int testcount, struct testgroup *tg){
	struct pollfd *pollfd;
	pid_t *pidarray;
	char buffer[BUFSIZ];
	int i=0;
	int fd;
	int nr_events;
	int remaining;
	int nexttest;
	struct testsuite *tscurrent;
	struct testsuite **worklist;

	if((concurrent==0) || (concurrent > testcount)){
		concurrent=testcount;
	}

	pidarray = x_malloc( sizeof(pid_t) * concurrent);
	pollfd = x_malloc( sizeof(struct pollfd) * concurrent);
	worklist = x_malloc( sizeof(void *) * concurrent);

	tscurrent=tg->base;
	while((tscurrent != NULL) && (i < concurrent)){
		//printf("starting %s\n",testarray[i].filename);
		pidarray[i] = test_start(tscurrent->filename,&fd);
		tscurrent->output = fdopen(fd, "r");
		worklist[i] = tscurrent;
		pollfd[i].fd = fd;
		pollfd[i].events = POLLIN;
		i++;
		tscurrent = tscurrent->next;
	}

	remaining = testcount; // number of tests either running or left to run
	nexttest = concurrent; //index of next test to run

	while((remaining > 0) && ( endtime > time(NULL) )){
		nr_events = poll(pollfd,concurrent ,-1);
		if(nr_events ==-1){
			perror("poll call failed");
			exit(99);
		}

		for(i=0;i<concurrent;i++){
			if(pollfd[i].revents & POLLIN){
				while( (worklist[i]->result != SKIPPED)
						  && (worklist[i]->result != ABORTED)
						  && fgets(buffer, sizeof(buffer), worklist[i]->output) ){
					if(buffer == NULL){
						printf("eof for %s\n",worklist[i]->filename);
					}
					//printf("buffer=%s",buffer);
					parse_line(buffer,worklist[i]);
				}
			}			
			if(pollfd[i].revents & POLLHUP){
				fclose( worklist[i]->output );

				if(tscurrent != NULL ){
					worklist[i] = tscurrent;
					pidarray[i] = test_start(worklist[i]->filename,&fd); 	//otherwise start the next process and feed it into the grinder^W poll
					worklist[i]->output = fdopen(fd, "r");
					pollfd[i].fd = fd;
					tscurrent = tscurrent->next;
				} else {
					pidarray[i] = -1;
					pollfd[i].fd = -1; //we've run out of tests so just mark this pollfd structure as not to be watched
				}
				remaining--;
			}
		}	
	}

	for(i=0;i<concurrent;i++){
		if(pidarray[i] != -1 ){
			//printf("killing %d\n",pidarray[i]);
			kill(pidarray[i],SIGABRT);
		}
	}
	free(pidarray);
	free(pollfd);
	free(worklist);
}


/***************************************
 *
 * *************************************/
void write_to_log(struct list *environment,struct testgroup *tg,void *data){
	FILE *log;
	struct data * env;
	struct testsuite *ts;
	struct test * curtest;
	struct data * curmeta;


	log=fopen((char*)data,"w");
	env=environment->base;
	while(env !=NULL){
		fprintf(log,"## ENV %s=%s\n",env->key,env->value);
		env=env->next;
	}

	ts=tg->base;
	while(ts != NULL){
		fprintf(log,"## FILE: %s\n",ts->filename);
		fprintf(log,"0..%d\n",ts->plannedcount);
		curmeta=ts->metadata->base;
		while(curmeta!=NULL){
			fprintf(log,"## TAG %s=%s\n",curmeta->key,curmeta->value);
			curmeta=curmeta->next;
		}

		curtest=ts->tests;
		while(curtest!=NULL){
			switch(curtest->result){
				case OK:
					fprintf(log,"ok ");
					break;
				case NOTOK:
					fprintf(log,"not ok ");
					break;
				default:
					fprintf(log,"unknown ");
					break;
			}
			fprintf(log,"%d %s ",curtest->number,curtest->description);
			switch(curtest->directive){
				case SKIPPED:
					fprintf(log,"# SKIP ");
					break;
				case TODO:
					fprintf(log,"# TODO ");
					break;
				default:
					fprintf(log,"# ");
					break;
			}
			fprintf(log," %s\n",curtest->reason);
			curtest=curtest->next;
		}
		ts=ts->next;
	}
	fclose(log);
}

/***************************************
 *
 * *************************************/
void write_to_stdout(struct list *environment,struct testgroup *tg,void *data){
	struct testsuite *ts;
	struct data * curmeta;
	struct test * curtest;

	ts = tg->base;
	while(ts != NULL){
		printf("Test: %s\n",ts->filename);
		curmeta=ts->metadata->base;
		printf("Metadata:\n");
		while(curmeta!=NULL){
			printf(" %s=+%s+\n",curmeta->key,curmeta->value);
			curmeta=curmeta->next;
		}

		curtest=ts->tests;
		while(curtest!=NULL){
			printf(" %d=  %d (%s)  --%s\n",curtest->number,curtest->result,curtest->reason,curtest->description);
			curtest=curtest->next;
		}

		ts = ts->next;
	}
}

/***************************************
 *
 * *************************************/
int main(int argc, char *argv[]) {
	char * logfile=NULL;
	int readfileflag=0;
	struct testgroup *tg=NULL;
	struct testsuite *ts;
	int i;
	int option;
	int testcount=0;
	int concurrent=10;
	//struct test * curtest;
	//struct data * curmeta;
	struct data * env;
	time_t endtime=0;
	//FILE *log;
	int resultcount[6];
	struct test *t;

	while ((option = getopt(argc, argv, "hl:c:t:r")) != EOF) {
		switch (option) {
		case 'h':
			exit(0);
			break;
		case 'l':
			logfile = optarg;
			break;
		case 'r':
			readfileflag = 1;
			break;
		case 'c':
			concurrent = strtol(optarg,NULL,10);
			break;
		case 't':
			endtime = time(NULL)+strtol(optarg,NULL,10);
			break;
		default:
			exit(1);
		}
	}

	tg = x_malloc(sizeof(struct testgroup));

	if(readfileflag == 0){
		testcount = argc-optind;
		if(endtime == 0){
			endtime = time(NULL) + 31536000; //if no timeout is given run for a year
		}
		for(i = 0; i < testcount; i++){
			addtestsuite(argv[i+optind],tg);
		}
		runtests(endtime, concurrent, testcount, tg);
	}else{
		for(i = 0; i < argc-optind; i++){
			testcount = readresultsfile(argv[i+optind],tg);
		}
	}
	//**********************
	//write out the results.
	//**********************
	printf("Tests: ");
	ts = tg->base;
	while(ts != NULL){
		printf("%s, ",ts->filename);
		ts = ts->next;
	}
	printf("\nEnvironment:\n");
	env=environment.base;
	while(env != NULL){
		printf(" %s=%s\n",env->key,env->value);
		env = env->next;
	}

	printf("Results:\n");
	write_to_stdout(&environment,tg,NULL);
	if(logfile){
		write_to_log(&environment,tg,logfile);
	}


	printf("%15s %7s %7s %7s %7s %7s %7s %9s\n","Suite","Passed","Failed","Aborted","Skipped","Todo","Missing","Result");
	ts = tg->base;
	while(ts != NULL){
		for(i=0;i<6;i++){
			resultcount[i]=0;
		}
		printf("%15s ",ts->filename);
		t = ts->tests;
		while(t != NULL){
			resultcount[t->result]++;
			t=t->next;
		}
		for(i=0;i<6;i++){
			printf("%7d ",resultcount[i]);
		}
		switch(ts->result){
			case ABORTED:
				printf("%9s","Aborted");
				break;
			case SKIPPED:
				printf("%9s","Skipped");
				break;
			case NOTOK:
				printf("%9s","Failed");
				break;
			default:
				printf("%9s","Success");
				break;
		}
		printf("\n");
		ts = ts->next;
	}

	/*
	sprintf(format,"%%-%ds",maxlen+2);
	printf(format,"Testset");
	printf("%-6s/%6s/%6s %8s %-7s %-4s %s\n","Passed","Failed","Missed"," (%%) ","skipped","Todo","Result");
	for(i=0 ; i<testcount ; i++){
		printf(format,testarray[i].filename);
		if((testarray[i].passed+testarray[i].failed+testarray[i].missing)!=0){
			pct=((float) testarray[i].passed/(testarray[i].passed+testarray[i].failed+testarray[i].missing))*100.0;
		}else{
			pct=0;
		}
		printf("%5d /%5d /%6d ",testarray[i].passed,testarray[i].failed,testarray[i].missing);
		printf("%8.0f ",floor(pct));
		printf("%7d %4d ",testarray[i].skip,testarray[i].todo);
		switch(testarray[i].state){
			case ABORTED:
				printf("file aborted\n");
				break;
			case SKIPPED:
				printf("file skipped\n");
				break;
			case FAILED:
				printf("failed\n");
				break;
			default:
				printf("success\n");
				break;
		}
	}
	*/
	//free(pollfd);
	//free(mapping);
	free(tg);
	exit(0);
}
