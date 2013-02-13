#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/poll.h>


#define VALID 0
#define ABORTED 1
#define FAILED 2
#define SKIPPED 3

struct testset {
	char * filename;
	int state;
	int passed;
	int failed;
	int missing;
	int todo;
	int skip;
	int count;
	int current;
	FILE *output;
	struct testset * next; 
};

struct syntax {
	char * string;
	int (*handler)(int testno,char* description, char * directive ,struct testset *ts);
};

int cb_ok(int testno,char* description, char * directive ,struct testset *ts);
int cb_plan(int testno,char* description, char * directive ,struct testset *ts);
int cb_notok(int testno,char* description, char * directive ,struct testset *ts);
int cb_bailout(int testno,char* description, char * directive ,struct testset *ts);
int cb_skipline(int testno,char* description, char * directive ,struct testset *ts);


struct syntax callbacks[] = {
	{"not ok"       ,&cb_notok      },
	{"ok"			,&cb_ok	 		},
	{"Bail out!"    ,&cb_bailout    },
	{"#"			,&cb_skipline   },
	{"1.."			,&cb_plan		},
	{NULL			,NULL      		}
};


char * ltrim(char * string){
	while(isspace(*string)){
		string++;
	}
	return string;
}


void rtrim(char * string){
	int len;

	len=strlen(string)-1;
	while(isspace(*(string+len))&&(len!=0)){
		len--;
	}
	*(string+len+1)=0;
	
}


int valid_testno(int testno,struct testset *ts) {

	if((testno == -1) || (testno == ts->current+1)) {
	//its a good test number or no test number given
		return ts->current+1;
	}else if(testno > ts->current+1) {
		//missed some tests
		fprintf(stderr,"test numbers dont tie up, expect %d got %d, probably missed %d tests\n",ts->current+1,testno,testno-ts->current-1);
		ts->missing+=(testno-1-ts->current);
		ts->state=FAILED;
		//ts->current=testno;
		return testno;
	} else {
		ts->state=FAILED;
		//back tracked test numbers
		fprintf(stderr,"test numbers dont tie up, was expecting %d found %d\n",ts->current,testno);
		return testno;
	}
}


int cb_notok(int testno,char* description, char * directive ,struct testset *ts){

	if(strncmp(directive,"TODO",4)==0){
		cb_ok(testno, description, directive, ts);
		return 0;
	} else if(strncmp(directive,"SKIP",4)==0){
		ts->skip++;	
	}
	ts->state=FAILED;

	ts->current=valid_testno(testno,ts);
	ts->failed++;

	return 0;
}



int cb_ok(int testno,char* description, char * directive ,struct testset *ts){

	if(strncmp(directive,"TODO",4)==0){
		ts->todo++;		
	} else if(strncmp(directive,"SKIP",4)==0){
		ts->skip++;		
	}
	ts->current=valid_testno(testno,ts);

	ts->passed++;
	return 0;
}


int cb_bailout(int testno,char* description, char * directive ,struct testset *ts){
	ts->state=ABORTED;
	return 0;
}


int cb_skipline(int testno,char* description, char * directive ,struct testset *ts){
	return 0;
}


int cb_plan(int testno,char* description, char * directive ,struct testset *ts){

	if(testno < 0){
		fprintf(stderr,"invalid plan found, marking test set as failed\n");
		ts->state=FAILED;
		return 0;
	}

	if(ts->count !=-1){
		ts->state=FAILED;
		return 0;
	}
	if(testno > 0){
		ts->count = testno;
	} else {
		ts->count=0;
		ts->state=SKIPPED;
	}

	return 0;
}


void * x_malloc(size_t size) {
	void *p;

	p = malloc(size);
	if (p == NULL){
		fprintf(stderr,"failed to malloc %lu bytes", (unsigned long) size);
		exit(100);
	}
	return p;
}


int parse_line(char * line, struct testset *ts) {
	int i=0;
	int retval;
	char *content;
	char * description=NULL;
	char * directive=NULL;
	int testno;
	int offset=0;

	line=ltrim(line);
	rtrim(line);

	while(callbacks[i].string != NULL){
		if(strncmp(line,callbacks[i].string,strlen(callbacks[i].string))==0) {
			content=line+strlen(callbacks[i].string);
			testno = strtol(content, (char **) &content, 10);
			if(content == (line+strlen(callbacks[i].string))) {
				testno=-1;
			}
			description=ltrim(content);
			while((*(description+offset) != '#') && (*(description+offset) !='\0')){
				offset++;
			}
			//everything after a # is potentially a directive
			//replace the # with a \0 to terminate description
			*(description+offset)=0;
			directive=ltrim(description+offset+1);
			//strip off trailing spaces
			rtrim(directive);
			rtrim(description);

			retval=(callbacks[i].handler)(testno,description,directive,ts);
			if(retval==1){
				return 1;
			}else if(retval==0){
				return 0;
			}else{
				;
			}
		}
		i++;
	}
	return -1;
}


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



int main(int argc, char *argv[]) {
	int option;
	char * logfile=NULL;
	int i;
	//int *output;
	//FILE **output;
	struct testset *testarray;
	pid_t pid;
	char buffer[BUFSIZ];
	float pct;
	int maxlen=0;
	char format[10];
	int testcount=0;
	int fd;
	int nr_events;
	struct pollfd *pollfd;
	int j;

	while ((option = getopt(argc, argv, "hl:r:")) != EOF) {
		switch (option) {
		case 'h':
			exit(0);
			break;
		case 'l':
			logfile = optarg;
			break;
		default:
			exit(1);
		}
	}

	testcount=argc-optind;
	testarray=x_malloc(sizeof(struct testset) *testcount);
	//output=x_malloc(sizeof(FILE)*testcount);
	
	for(i=0;i<testcount;i++){
		printf("setting up %s\n",argv[i+optind]);
		testarray[i].state=VALID;
		testarray[i].passed=0;
		testarray[i].failed=0;
		testarray[i].missing=0;
		testarray[i].todo=0;
		testarray[i].skip=0;
		testarray[i].count=-1;
		testarray[i].current=0;
		testarray[i].next=NULL;
		testarray[i].output=NULL;
		testarray[i].filename=argv[i+optind];
		if(strlen(testarray[i].filename)>0){
			maxlen=strlen(testarray[i].filename);
		}
	}

	//events=x_malloc(sizeof(struct epoll_event)*64);
	pollfd=x_malloc(sizeof(struct pollfd)*testcount);

	for(i=0;i<testcount;i++){
		pid=test_start(testarray[i].filename,&fd);
		testarray[i].output = fdopen(fd, "r");
		printf("%s: setting %d to %d\n",testarray[i].filename,i,fd);

		pollfd[i].fd=fd;
		pollfd[i].events=POLLIN;
	}


	for(j=0;j<15;j++){
		nr_events=poll(pollfd,testcount ,-1);
		if(nr_events ==-1){
			perror("poll call failed");
			exit(99);
		}

		for(i=0;i<testcount;i++){
			printf("%s i=%d %d %x\n",testarray[i].filename,i,nr_events,pollfd[i].revents); 
			if(pollfd[i].revents & POLLIN){
				fgets(buffer, sizeof(buffer), testarray[i].output);
				if(buffer == NULL){
					printf("eof for %s\n",testarray[i].filename);
				}
				printf("buffer=%s\n",buffer);
			}			
			if(pollfd[i].revents & POLLHUP){
				pollfd[i].fd=-1;
			}
		}	
	}
	//**********************
	//write out the results.
	//**********************
	//printf("%-20s\tPassed/Failed/Missed (%%)\tSkipped/Todo\tResult\n","testset");
	sprintf(format,"%%-%ds",maxlen+2);
	printf(format,"Testset");
	printf("%-6s/%6s/%6s %8s %-7s %-4s %s\n","Passed","Failed","Missed"," (%%) ","skipped","Todo","Result");
	for(i=0;i<testcount;i++){
		//printf("\n%s:\n",currset->filename);
		//printf("PASS: %d FAIL: %d NOT FOUND: %d\n",testarray[i].passed,testarray[i].failed,testarray[i].missing);
		//printf("todo: %d skipped: %d \n",testarray[i].todo,testarray[i].skip);
		printf(format,testarray[i].filename);
		pct=((float) testarray[i].passed/(testarray[i].passed+testarray[i].failed+testarray[i].missing))*100.0;
		printf("%5d /%5d /%6d ",testarray[i].passed,testarray[i].failed,testarray[i].missing);
		printf("%8.1f ",pct);
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
	exit(0);
}
