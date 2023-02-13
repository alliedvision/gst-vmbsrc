#ifndef VIMBAX_HELPERS_H_
#define VIMBAX_HELPERS_H_

#include <VmbC/VmbCommonTypes.h>

const char *ErrorCodeToMessage(VmbError_t eError);

VmbInt64_t RoundToNearestValidValue(VmbInt64_t value, VmbInt64_t min, VmbInt64_t max, VmbInt64_t increment);

#endif // VIMBAX_HELPERS_H_
