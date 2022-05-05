#ifndef POPEN_SIMPLIFIED_H
#define POPEN_SIMPLIFIED_H

#include <stdio.h>

// We might also want to use these in C, or provide an interface
// for high-level languages like python (though pybind may seem
// to be a better option, or is it? :).
#ifdef __cplusplus
extern "C" {
#endif

FILE* popen_simplified(const char* program, const char* type);

int pclose_simplified(FILE* iop);

#ifdef __cplusplus
}
#endif

#endif // POPEN_SIMPLIFIED_H
