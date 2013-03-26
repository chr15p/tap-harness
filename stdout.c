#include "harness.h"

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

