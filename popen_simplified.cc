#include "popen_simplified.h"
#include <list>
#include <algorithm>
#include <utility>
#include <cerrno>
#include <sys/wait.h>
#include <unistd.h>

// The book-keeping `opened_processes` is unnecessary if somehow we can
// rewrite the file stream `FILE` struct. For example, we can just add
// a `pid_t popened_cpid` field (with default value -1) in it, and then
// we can simply `waitpid` for `fp->popened_cpid` in `pclose()`. :)
using proc_info = std::pair<FILE*, pid_t>;
static std::list<proc_info> opened_processes; // or use a symtab

FILE* popen_simplified(const char* program, const char* type) {
    /* We can assume type is either "r" or "w" */
    int pdes[2];
    if (pipe(pdes) < 0) { // Q1: What does pipe() do?
        return NULL;
    }
    pid_t pid = fork(); // Q2: How does fork() work?
    if (pid == -1) {    // A2, fork() failed
        close(pdes[0]);
        close(pdes[1]);
        return NULL;
    } else if (pid == 0) { // A2, child
        // Q3: Why do we need to close all FILE* here?
        for (auto info : opened_processes) {
            close(fileno(info.first));
        }
        if (*type == 'r') { // parent reads from pipe (child's stdout)
            // Child redirects stdout to pipe write end so that
            // parent can read from the read end of the pipe.
            close(pdes[0]); // A1, child closes unused read end
            if (pdes[1] != STDOUT_FILENO) {
                dup2(pdes[1], STDOUT_FILENO); // Q4: what does dup2() do?
                close(pdes[1]);               // A4
            }
        } else { // parent writes to the child's stdin
            close(pdes[1]); // A1, child closes unused write end
            if (pdes[0] != STDIN_FILENO) {
                dup2(pdes[0], STDIN_FILENO);
                close(pdes[0]); // A4
            }
        }
        char* argp[] = {(char*)"sh", (char*)"-c", (char*)program, NULL};
        execve("/bin/sh", argp, environ); // Q5: execve() vs system("sh -c {program}")
        _exit(127);
    }
    // A2, parent
    FILE* iop;
    if (*type == 'r') {
        iop = fdopen(pdes[0], type);
        close(pdes[1]); // A1, parent closes unused write end
    } else {
        iop = fdopen(pdes[1], type);
        close(pdes[0]); // A1, parent closes ununsed read end
    }
    opened_processes.emplace_back(iop, pid);
    return iop;
}

int pclose_simplified(FILE* iop) { // Q6: what happen if we simply fclose(iop)?
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

/* Most of the answers can be found on the linux man pages.
 * Just RTFMP (F: fine) :^)
 */

/*
 * Q1: What does pipe() do?
 * A1: pipe() creates a pipe, a *unidirectional* data channel which can be
 *     used for interprocess communincation. It takes 2 file descriptors
 *     `int pipefd[2]`, and makes `pipefd[0]` the read end and `pipefd[1]`
 *     the write end (mnemonics: 0 -> STDIN_FILENO  -> pipefd[0],
 *                               1 -> STDOUT_FILENO -> pipefd[1]).
 *     Data written to `pipefd[1]` can be read from `pipefd[0]`. Hence the
 *     name, pipe.
 *
 *     Notes: The read end of the pipe can’t be written, and the write end
 *     of the pipe can’t be read. Attempting to read/write to the wrong end
 *     of the pipe will result in a system call error (the read() or write()
 *     call will return -1).
 *
 *     (stolen from https://cs61.seas.harvard.edu/site/2021/ProcessControl/)
 *
 *     Also note that a read() from a pipe returns `EOF` iif *all* write ends
 *     of a pipe are closed. Thus, if we do something like child process
 *     writes to the pipe and parent process reads from the pipe, we need to
 *     close the unused write end from the parent process (closing the unused
 *     read end from the child is also encouraged :^). Otherwise, the parent
 *     will never receive an end of file (`EOF`) while reading.
 *
 *     For many other details please see `man pipe`.
 *
 * Q2: How does fork() work?
 * A2: (mmap, COW: shamelessly stolen from CSAPP_3e 9.8.2)
 *     When the fork function is called by the current process, the kernel
 *     creates various data structures for the new process and assigns it
 *     a unique PID. To create the virtual memory for the new process, it
 *     creates exact copies of the current process's `mm_struct`, area
 *     structs, and page tables (our concern here is that child process
 *     inherits the parent's opened files). It flags each page in both
 *     processes as read-only, and flags each area struct in both processes
 *     as private copy-on-write.
 *
 *     When the fork returns in the new process, the new process now has an
 *     exact copy of the virtual memory as it existed when the fork was
 *     called. When either of the processes performs any subsequent writes,
 *     the copy-on-write mechanism creates new pages, thus preserving the
 *     abstraction of a private address space for each process.
 *
 * Q3: Why do we need to close all `FILE*` there?
 * A3: They're just the side effects (undesired products) of fork(). Those
 *     file streams (`opened_processes`) are inherited from the parent,
 *     which are used for administration work (that is, keeping track of all
 *     `popen()`ed (pipe) files).
 *     Closing unrelated file descriptors can make the child process start
 *     using small file descriptors if it needs to open new files.
 *     Also, it's dangerous for the child process to hold resource handles
 *     to its peers :)
 *
 * Q4: What does dup2() do?
 * A4: It copies oldfd to newfd. For example, dup2(pipefd[1], 1) duplicates
 *     the pipe write end to stdout (newfd=1 will first be closed by dup2()
 *     if it's already opened). And now both pipefd[1] and stdout point to
 *     the write end of the pipe. If we want to redirect stdout to the pipe,
 *     we will also need to `close(pipefd[1])` so that only the stdout owns
 *     the write end of the pipe.
 *     Also note taht, if we want to redirect stderr to stdout (2>&1), all
 *     we need to do is dup2(1, 2) (no close(1)! Only opened files should
 *     be closed).
 *
 * Q5: execve() vs system("sh -c {program}")
 * A5: system() executes a shell command (fork() a child process, and then
 *     execl("/bin/sh", "sh", "-c", command, nullptr)), whereas execve()
 *     just executes the program (no forking involved).
 *     Also, system() returns after the command has been completed, while
 *     execve() never returns (unless on error to execve()).
 *
 * Q6: What happens if we simply fclose(iop)?
 * A6: Zombies' world :^)
 *     Besides, the book-keeping list will be broken (& by then it will
 *     contain closed streams and zombie processes. What's your name
 *     again -- opened_processes. Ah, boo you :'(
 *
 * Q7: Is our simplified version thread safe?
 * A7: No, it isn't. Neither does popen.c in bionic libc from the link
 *     https://android.googlesource.com/platform/bionic/+/3884bfe9661955543ce203c60f9225bbdf33f6bb/libc/unistd/popen.c
 *     (maybe because it was old, or just not designed to be thread safe
 *     for the sake of performance, like STL containers :)
 *     But note that `popen()` and `pclose()` in glibc are MT-safe.
 *     Run ./popen_simplified_test (which uses Google's thread sanitizer) to
 *     verify this.
 *
 *     Thread safety:
 *     https://docs.oracle.com/cd/E19683-01/806-6867/6jfpgdco5/index.html
 *
 *     Explanation:
 *     No readers-writer locks for the shared list `opened_process`.
 *     The shared list must be guarded by a lock during read/write,
 *     otherwise it will be corrupted (e.g. two or more writes happen
 *     at the same time or one thread is reading while at the mean
 *     time another thread is writing.)
 */
