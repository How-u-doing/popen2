#include "popen_simplified.h"
#include <thread>
#include <cassert>

#ifndef POPEN_SIMPLIFIED
#   define POPEN  popen
#   define PCLOSE pclose
#else
#   define POPEN  popen_simplified
#   define PCLOSE pclose_simplified
#endif

void write_data(const char* data)
{
    FILE* output = POPEN("wc", "w");
    fprintf(output, "%s", data);
    int exit_status = PCLOSE(output);
    assert(exit_status == 0);
}

void read_data(char* data)
{
    FILE* input  = POPEN("echo foobar", "r");
    fscanf(input, "%s", data);
    int exit_status = PCLOSE(input);
    assert(exit_status == 0);
}

int main()
{
    char buf[10]{};
    std::thread t1(write_data, "one two three");
    std::thread t2(read_data, buf);
    t1.join();
    t2.join();
    fprintf(stdout, "all threads finished\n");

    fprintf(stdout, "data read:\n");
    fprintf(stdout, "%s\n", buf);
    return 0;
}
