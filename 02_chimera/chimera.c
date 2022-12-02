#define _GNU_SOURCE
#include <sched.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <syscall.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

/* For our clone experiments, we working on a very low level and
 * fiddle around with threading. However, this leads to a problem with
 * the libc, which must perform some user-space operations to setup a
 * new thread. For example, in a clone()ed thread, we cannot simply
 * use printf(). Therefore, we provide you with a simple function to
 * output a string and a number. The writing to stdout (fd=1) is done
 * with plain system calls.
 *
 * Example: syscall_write("foobar = ", 23);
 */
int syscall_write(char *msg, int number) {
    write(1, msg, strlen(msg));
    if (number != 0) {
        char buffer[sizeof(number) * 3];
        char *p = &buffer[sizeof(number) * 3];
        int len = 1;
        *(--p) = '\n';
        if (number < 0) {
            write(1, "-", 1);
            number *= -1;
        }
        while (number > 0) {
            *(--p) =  (number % 10) + '0';
            number /= 10;
            len ++;
        }
        write(1, p, len);
    } else {
        write(1, "0\n", 2);
    }


    return 0;
}

// For the new task, we always require an stack area. To make our life
// easier, we just statically allocate an global variable of PAGE_SIZE.
char stack[4096];

// To demonstrate whether child and parent are within the same
// namespace, we count up a global variable. If they are within the
// same address space, we will see modification to this counter in
// both namespaces
volatile int counter = 0;

int child_entry(void* arg) {
    // We just give a little bit of information to the user. 
    syscall_write(": Hello from child_entry", 0);
    syscall_write(": getppid() = ", getppid()); // What is our parent PID
    syscall_write(": getpid()  = ", getpid());  // What is our thread group/process id
    syscall_write(": gettid()  = ", gettid());  // The ID of this thread!
    syscall_write(": getuid()  = ", getuid());  // What is the user id of this thread.


    // We increment the global counter in one second intervals. If we
    // are in our own address space, this will have no influence on
    // the parent!
    while (counter < 4) {
        counter++;
        sleep(1);
    }

    return 0;
}

enum clone_modes {
  Fork,
  Chimera,
  Thread,
  User  
};

// Method that is called in the clone and attempts to write to the
// shared counter If it is in fork mode, this does not do anything.
// If it is in a chimera or thread mode, it will modify the memory
// and actually exists the main loop.
int clone_main(void *arg) {
  syscall_write("\nThread id: ", gettid());
  syscall_write("\nProcess id: ", getpid());
  
  while (counter < 10) {
	counter++;
	syscall_write("\n> write from fork. Counter = ", counter);
	sleep(2);	
  }
  
  exit(0);
}

// Simple method that checks if shared memory is unreachable and
// verifies that the mapping has been created.
int user_main(void *arg) {
  sleep(2);
  counter += 2;
  syscall_write("getuid() = ", getuid());
  exit(0);
}

typedef int (*f) (void*);

void my_clone(int mode) {
  int flags = 0;
  f func = clone_main;

  /* Emulate fork */
  if (mode == Fork) {
	flags |= CLONE_FILES;
	flags |= CLONE_CHILD_SETTID;
	flags |= CLONE_CHILD_CLEARTID;
	flags |= SIGCHLD;
  }
  /* The whole kitchen sink, copies everything but the thread */
  else if (mode == Chimera) {
	flags |= CLONE_CHILD_SETTID;
	flags |= CLONE_CHILD_CLEARTID;	
	flags |= CLONE_VM;
	flags |= CLONE_FILES;
	flags |= CLONE_FS;
	flags |= CLONE_IO;
	flags |= SIGCHLD;
	flags |= CLONE_PARENT;
	flags |= CLONE_PTRACE;
	flags |= CLONE_SIGHAND;
  }
  /* Only copies the memory space and the thread group */
  else if (mode == Thread) {
	flags |= CLONE_THREAD;
	flags |= CLONE_SIGHAND;
	flags |= CLONE_VM;
  }
  /* Creates a normal fork, but with a user namespace */
  else if (mode == User) {
	flags |= CLONE_CHILD_SETTID;
	flags |= CLONE_CHILD_CLEARTID;
	flags |= SIGCHLD;
	flags |= CLONE_NEWUSER;
	func = user_main;
  }

  /* We need to pid to find our user spaces. We also move our stack to
	 the end so we can grow it backwards.  */
  int pid = clone(func, stack + 4096, flags, NULL);

  /* Very graceful solution */
  if (mode == User) {
	/* Open the file governing the user map of the process */
	char filename[1024];
	snprintf(filename, 1024, "/proc/%d/uid_map", pid);
	write(0, filename, strlen(filename));

	// Create the mapping from our user id to root
	char buf[1024];
	snprintf(buf, 1024, "0\t%d\t1", getuid());
	buf[1023] = '\0'; // Ensure that we actually exit
	int buf_size = strlen(buf);
	
	// Open the file, write 
	int f = open(filename, O_WRONLY);
	write(f, buf, strlen(buf));
	write(0, buf, buf_size);
	close(f);
  }
  
}



int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: %s MODE\n", argv[0]);
        printf("MODE:\n");
        printf("  - fork    -- emulate fork with clone\n");
        printf("  - chimera -- create process/thread chimera\n");
        printf("  - thread  -- create a new thread in a process\n");
        printf("  - user    -- create a new process and alter its UID namespace\n");
        return -1;
    }

    syscall_write("> Hello from main! - ", 0);
    syscall_write("> getppid() = ", getppid());
    syscall_write("> getpid()  = ", getpid());
    syscall_write("> gettid()  = ", gettid());
    syscall_write("> getuid()  = ", getuid());

    int flags = 0;
    void *arg = NULL;
    if (!strcmp(argv[1], "fork")) {
	  flags |= Fork;
    } else if (!strcmp(argv[1], "chimera")) {
	  flags |= Chimera;
	} else if (!strcmp(argv[1], "thread")) {
	  flags |= Thread;
	} else if (!strcmp(argv[1], "user")) {
	  flags |= User;
	} else {
        // TODO: Implement multiple clone modes.
        printf("Invalid clone() mode: %s\n", argv[1]);
        return -1;
    }
    
	my_clone(flags);
	
    syscall_write("\n!!!!! Press C-c to terminate. !!!!!", 0);

	// Close the loop
	int try_counter = 0;
    while(counter < 4 && try_counter++ < 20) {
        syscall_write("counter = ", counter);
        sleep(1);
    }
}
