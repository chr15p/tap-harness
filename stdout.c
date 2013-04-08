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

