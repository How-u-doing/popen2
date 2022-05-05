# popen2

We would like to generalize the POSIX `popen` such that
we can both read from and write to the subprocess's stdout and stdin, respectively.

## Understanding `popen`

Excerpted from the [Linux manual page](https://man7.org/linux/man-pages/man3/popen.3.html):

```cpp
#include <stdio.h>

FILE *popen(const char *command, const char *type);
int pclose(FILE *stream);
```
> The popen() function opens a process by creating a pipe, forking,
       and invoking the shell.  Since a pipe is by definition
       unidirectional, the type argument may specify only reading or
       writing, not both; the resulting stream is correspondingly read-
       only or write-only.

Below is a reference implementation of `popen` (and `pclose`), simplified from [here](https://android.googlesource.com/platform/bionic/+/3884bfe9661955543ce203c60f9225bbdf33f6bb/libc/unistd/popen.c):

```cpp
using proc_info = std::pair<FILE*, pid_t>;
static std::list<proc_info> opened_processes;

FILE* popen(const char* program, const char* type) {
    /* We can assume type is either "r" or "w" */
    int pdes[2];
    if (pipe(pdes) < 0) { // Q1: What does pipe() do?
        return NULL;
    }
    pid_t pid = fork(); // Q2: How does fork() work?
    if (pid == -1) {    // A2
        close(pdes[0]);
        close(pdes[1]);
        return NULL;
    } else if (pid == 0) { // A2
        // Q3: Why do we need to close all FILE* here?
        for (auto info : opened_processes) {
            close(fileno(info.first));
        }
        if (*type == 'r') {
            close(pdes[0]); // A1
            if (pdes[1] != STDOUT_FILENO) {
                dup2(pdes[1], STDOUT_FILENO); // Q4: what does dup2() do?
                close(pdes[1]);               // A4
            }
        } else {
            close(pdes[1]); // A1
            if (pdes[0] != STDIN_FILENO) {
                dup2(pdes[0], STDIN_FILENO);
                close(pdes[0]); // A4
            }
        }
        char* argp[] = {(char*)"sh", (char*)"-c", (char*)program, NULL};
        execve("/bin/sh", argp, environ); // Q5: execve() vs system("sh -c {program}")
        _exit(127);
    }
    // A2
    FILE* iop;
    if (*type == 'r') {
        iop = fdopen(pdes[0], type);
        close(pdes[1]); // A1
    } else {
        iop = fdopen(pdes[1], type);
        close(pdes[0]); // A1
    }
    opened_processes.push_back({iop, pid});
    return iop;
}

int pclose(FILE* iop) { // Q6: what happen if we simply fclose(iop)?
    auto it = std::find_if(opened_processes.begin(), opened_processes.end(),
                           [&](const proc_info& info) { return info.first == iop; });
    if (it == opened_processes.end()) {
        return -1;
    }
    fclose(iop);
    pid_t pid = it->second;
    int pstat;
    do {
        pid = waitpid(pid, &pstat, 0);
    } while (pid == -1 && errno == EINTR);
    opened_processes.erase(it);
    return (pid == -1 ? -1 : pstat);
}
```

Is our simplified version thread-safe?

## Implementing `popen2`

With our understanding of `popen`, briefly explain how to implement `popen2`.

```cpp
struct subprocess_t {
    FILE* p_stdin;  // child process' stdin, writable
    FILE* p_stdout; // child process' stdout, readable
    int returncode; // child process' exit code
};

subprocess_t popen2(const char* program);
int pclose2(subprocess_t& p);
```

Food for thoughts:

- Do we need to modify the book-keeping `proc_info` type?
- How many `pipe()` calls are needed in `popen2()`?
- How to capture the subprocess's exit code?
