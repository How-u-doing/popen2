#include "popen2.h"
#include <cassert>

void write_data(subprocess_t& p, const char* data)
{
    // write data to p's stdin
    fprintf(p.p_stdin, "%s", data);
    fflush(p.p_stdin); // IMPORTANT, to make sure child receives the data
}

void read_data(const subprocess_t& p, char* data)
{
    // read data from p's stdout
    fscanf(p.p_stdout, "%s", data);
    //fflush(p.p_stdout);
}

int main()
{
    char buf[5][10]{};
    subprocess_t subprocess = popen2("cat");
    assert(subprocess.cpid != -1);

    // communicate with the subprocess
    // for the test subprocess `cat`, we need a '\n' in each write
    write_data(subprocess, "one\n");
    read_data(subprocess, buf[0]);

    write_data(subprocess, "two\n");
    write_data(subprocess, "three\n");
    read_data(subprocess, buf[1]);
    read_data(subprocess, buf[2]);

    write_data(subprocess, "four\n");
    read_data(subprocess, buf[3]);

    write_data(subprocess, "five\n");
    read_data(subprocess, buf[4]);

    pclose2(&subprocess);
    for (char * s : buf)
        fprintf(stdout, "%s\n", s);
    return 0;
}
