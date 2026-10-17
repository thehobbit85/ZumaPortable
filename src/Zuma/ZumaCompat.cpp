//
// ZumaCompat.cpp - implementations for the compatibility bridge.
//
// Ported from SexyAppFramework 1.3's Common.cpp (narrow-string path): the
// format-a-string-into-a-std::string helpers the decompiled game code calls.
//
#include "ZumaCompat.h"

namespace Sexy
{

std::string vformat(const char* fmt, va_list argPtr)
{
    // We draw the line at a 1MB string.
    const int maxSize = 1000000;

    // If the string is less than 161 characters, use the stack - saves the
    // malloc/free round trip on the common case.
    const int bufSize = 161;
    char stackBuffer[bufSize];

    int attemptedSize = bufSize - 1;
    va_list argCopy;

    va_copy(argCopy, argPtr);
    int numChars = vsnprintf(stackBuffer, attemptedSize, fmt, argCopy);
    va_end(argCopy);

    if ((numChars >= 0) && (numChars <= attemptedSize))
    {
        // Needed for the 160-character printf case.
        stackBuffer[numChars] = '\0';
        return std::string(stackBuffer);
    }

    // Now use the heap.
    char* heapBuffer = nullptr;

    while (((numChars == -1) || (numChars > attemptedSize)) &&
           (attemptedSize < maxSize))
    {
        attemptedSize *= 2;
        heapBuffer = (char*)realloc(heapBuffer, attemptedSize + 1);

        va_copy(argCopy, argPtr);
        numChars = vsnprintf(heapBuffer, attemptedSize, fmt, argCopy);
        va_end(argCopy);
    }

    if (heapBuffer == nullptr) // defensive: formatting failed outright
        return std::string();

    heapBuffer[numChars] = 0;
    std::string result(heapBuffer);
    free(heapBuffer);

    return result;
}

std::string StrFormat(const char* fmt, ...)
{
    va_list argList;
    va_start(argList, fmt);
    std::string result = vformat(fmt, argList);
    va_end(argList);

    return result;
}

} // namespace Sexy
