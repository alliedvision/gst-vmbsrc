#ifndef HELPERS_H_
#define HELPERS_H_

#include <stdbool.h>
#include <string.h>

bool starts_with(const char *str, const char *prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

#endif // HELPERS_H_