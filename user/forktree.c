// Fork a binary tree of processes and display their structure.

#include <lib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>

#define DEPTH 10

void forktree(const char *cur);

void
forkchild(const char *cur, char branch)
{
	char nxt[DEPTH+1];
	int i;
	if (strlen(cur) >= DEPTH)
		return;

	snprintf(nxt, DEPTH+1, "%s%c", cur, branch);
	if ((i = fork()) == 0) {
		forktree(nxt);
		exit();
	} else if (i < 0)
		printf("forktree fork error! pid :%d err:%d\n",getpid(),i);
}

void
forktree(const char *cur)
{
	printf("%08d: I am '%s'\n", getpid(), cur);

	forkchild(cur, '0');
	forkchild(cur, '1');
}

int
main(int argc, char **argv)
{
	//open("/dev/tty",O_RDWR,0);
	forktree("");
	//printf("forktree over!%d\n",getpid());
	return 0;
}

