#ifndef HELPERS_H_
#define HELPERS_H_

#include <stdbool.h>
#include <string.h>

// Dummy use for currently unused objects to allow compilation while treating warnings as errors
#define UNUSED(x) (void)(x)

bool starts_with(const char *str, const char *prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

#endif // HELPERS_H_