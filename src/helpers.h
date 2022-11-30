#ifndef HELPERS_H_
#define HELPERS_H_

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// Dummy use for currently unused objects to allow compilation while treating warnings as errors
#define UNUSED(x) (void)(x)

bool starts_with(const char *str, const char *prefix)
{
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

void* VmbAlignedAlloc(size_t alignment, size_t size)
{
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    return aligned_alloc(alignment, size);
#endif
}

void VmbAlignedFree(void* buffer)
{
#ifdef _WIN32
    _aligned_free(buffer);
#else
    free(buffer);
#endif
}

#endif // HELPERS_H_