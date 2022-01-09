#ifndef _SYSCALL_H
#define _SYSCALL_H
#include <types.h>
/* system call numbers */
enum {
	SYS_CPUTS = 0,
	SYS_EXIT,
	SYS_EXOFORK,
	SYS_GETPID,
	SYS_BRK,
	SYS_READ,
	SYS_OPEN,
	SYS_CREATE,
	SYS_CLOSE,
	SYS_MKDIR,
	SYS_WRITE,
	SYS_DUP,
	SYS_DUP2,
	SYS_PIPE,
	SYS_WAIT,
	SYS_EXEC,
	SYS_FDTYPE,
	NSYSCALLS
};

#endif /* !_SYSCALL_H */
