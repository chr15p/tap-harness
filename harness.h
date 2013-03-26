#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/poll.h>
//#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <signal.h>


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

