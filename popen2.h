#ifndef POPEN2_H
#define POPEN2_H

#include <stdio.h>
#include <sys/types.h>

// We might also want to use these in C, or provide an interface
// for high-level languages like python (though pybind may seem
// to be a better option, or is it? :).
#ifdef __cplusplus
extern "C" {
#endif

// kinda miss default values
struct subprocess_t {
    pid_t cpid;
    int exit_code;
    FILE* p_stdin;  // child process's stdin, writable
    FILE* p_stdout; // child process's stdout, readable
};

// though I think calling it `popen1` will be more appropriate :)
/*
 * This version enables the calling process to both read from and
 * write to the program's stdout and stdin, respectively.
 */
struct subprocess_t popen2(const char* program);

int pclose2(subprocess_t* p);

#ifdef __cplusplus
}
#endif

#endif // POPEN2_H
