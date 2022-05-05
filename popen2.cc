#include "popen2.h"
#include <list>
#include <algorithm>
#include <cerrno>
#include <sys/wait.h>
#include <unistd.h>

void init_subprocess(subprocess_t* p)
{
    p->cpid = -1;
    p->exit_code = 0;
    p->p_stdin = nullptr;
    p->p_stdout = nullptr;
}

// thread unsafe :)
static std::list<subprocess_t> opened_processes;

subprocess_t popen2(const char* program)
{
    subprocess_t subprocess;
    // although already initialized (in C++), but maybe not what we want
    init_subprocess(&subprocess);
    // We will need 2 pipes in order to communicate between parent and child.
    // If only using 1 pipe, both child and parent will hold two ends of the
    // pipe, and now if the parent is going to write to the pipe, data may
    // get read from parent immediately, without going to the child.
    int pipe_pc[2];  // write: P -> C  /  read: C <- P
    int pipe_cp[2];  // write: C -> P  /  read: P <- C
    if (pipe(pipe_pc) < 0) {
        return subprocess;
    }
    if (pipe(pipe_cp) < 0) {
        close(pipe_pc[0]);
        close(pipe_pc[1]);
        return subprocess;
    }

    pid_t pid = fork();
    if (pid == -1) { // failed
        close(pipe_pc[0]);
        close(pipe_pc[1]);
        close(pipe_cp[0]);
        close(pipe_cp[1]);
        return subprocess;
    }
    else if (pid == 0) { // child
        // close unrelated files inherited from parent
        for (const auto& p : opened_processes) {
            close(fileno(p.p_stdin));
            close(fileno(p.p_stdout));
        }

        close(pipe_cp[0]); // close unused read  end in pipe C -> P
        close(pipe_pc[1]); // close unused write end in pipe P -> C

        // redirect stdout to write end of the pipe
        if (pipe_cp[1] != STDOUT_FILENO) {
            dup2(pipe_cp[1], STDOUT_FILENO);
            close(pipe_cp[1]);
        }
        // redirect stdin to read end of the pipe
        if (pipe_pc[0] != STDIN_FILENO) {
            dup2(pipe_pc[0], STDIN_FILENO);
            close(pipe_pc[0]);
        }

        // launch the shell to run the program
        const char* argp[] = {"sh", "-c", program, nullptr};
        execve("/bin/sh", (char**) argp, environ);
        // shouldn't be reached here
        _exit(127);
    }
    close(pipe_pc[0]); // close unused read  end in pipe P -> C
    close(pipe_cp[1]); // close unused write end in pipe C -> P
    // parent (client) reads from the subprocess's stdout and
    //                 writes to the subprocess's stdin
    subprocess.p_stdout = fdopen(pipe_cp[0], "r");
    subprocess.p_stdin  = fdopen(pipe_pc[1], "w");
    subprocess.cpid = pid;
    opened_processes.push_back(subprocess);
    return subprocess;
}

int pclose2(subprocess_t* p)
{
    auto it = std::find_if(opened_processes.begin(), opened_processes.end(),
                           [&](const subprocess_t& process) {
                               return process.cpid == p->cpid;
                           });
    if (it == opened_processes.end()) {
        return -1;
    }

    fclose(p->p_stdin);
    fclose(p->p_stdout);

    pid_t pid = p->cpid;
    int pstat;
    do {
        pid = waitpid(pid, &pstat, 0);
    } while (pid == -1 && errno == EINTR);

    // obtain the subprocess's exit code
    if (WIFEXITED(pstat)) {
        p->exit_code = WEXITSTATUS(pstat);
    }
    else if (WIFSIGNALED(pstat)) {
        p->exit_code = WTERMSIG(pstat);
    }

    opened_processes.erase(it);
    return (pid == -1 ? -1 : pstat);
}
