//
// ZumaCompat.h - compatibility bridge between the Zuma Deluxe 1.0.0
// decompilation (SexyAppFramework 1.3 era) and the modernized SDL2/OpenGL-ES
// SexyAppFramework this port is built on (from PvZ-Portable).
//
// Injected from Zuma_Prefix.pch so every game translation unit sees it, which
// keeps the decompiled sources byte-identical to upstream wherever possible.
//
#ifndef __ZUMA_COMPAT_H__
#define __ZUMA_COMPAT_H__

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#ifndef _WIN32
#include <strings.h>
#endif

namespace Sexy
{

// ---------------------------------------------------------------------------
// Fonts: framework 1.3 exposed the abstract font base as Sexy::Font. The modern
// framework calls it _Font (ImageFont/SysFont still derive from it). Alias it
// so the decompiled call sites read the same as upstream.
// ---------------------------------------------------------------------------
class _Font;
typedef _Font Font;

// ---------------------------------------------------------------------------
// Strings: 1.3's SexyString (std::string on the narrow, non-_USE_WIDE_STRING
// path this port uses) was dropped in favour of std::string everywhere.
// ---------------------------------------------------------------------------
typedef std::string SexyString;
typedef SexyString::value_type SexyChar;

// ---------------------------------------------------------------------------
// Windows-style scalar aliases used throughout the decompiled sources. They come
// from <windows.h> on Windows; provide them for the other targets (Android in
// particular) so the game code compiles unchanged.
// ---------------------------------------------------------------------------
#ifndef _WIN32
typedef uint32_t DWORD;
typedef uint16_t WORD;
typedef uint8_t  BYTE;
typedef uint32_t UINT;
typedef int32_t  LONG;
typedef uint32_t ULONG;
#endif

// ---------------------------------------------------------------------------
// Variadic string formatting, used by the game for high-score and dialog text.
// ---------------------------------------------------------------------------
std::string vformat(const char* fmt, va_list argPtr);
std::string StrFormat(const char* fmt, ...);

} // namespace Sexy

// ---------------------------------------------------------------------------
// Unqualified min/max: framework 1.3's Common.h made these visible to game code
// (via <algorithm> plus the old headers' using-declarations). Restored for the
// decompiled call sites.
// ---------------------------------------------------------------------------
using std::min;
using std::max;

// ---------------------------------------------------------------------------
// Legacy helpers the decompiled code used from SexyAppFramework 1.3 / Win32.
// These must sit at global scope: the game calls them unqualified, and pulling
// standard headers in below "namespace Sexy" would nest them incorrectly.
// ---------------------------------------------------------------------------
#ifdef _WIN32
	#define sexystricmp _stricmp
#else
	#define sexystricmp strcasecmp
	// The decompiled sources also call the bare Win32 spelling.
	#define stricmp strcasecmp
	// itoa is non-standard (Win32 only); supply the equivalent.
	inline char* itoa(int theValue, char* theBuffer, int theRadix)
	{
		if (theRadix == 10)
		{
			std::snprintf(theBuffer, 34, "%d", theValue);
			return theBuffer;
		}

		char* aOut = theBuffer;
		unsigned int aValue = (theValue < 0) ? (unsigned int)(-(long long)theValue)
											  : (unsigned int)theValue;
		char aDigits[36];
		int aCount = 0;

		do
		{
			int aDigit = (int)(aValue % (unsigned int)theRadix);
			aDigits[aCount++] = (char)(aDigit < 10 ? '0' + aDigit : 'a' + aDigit - 10);
			aValue /= (unsigned int)theRadix;
		} while (aValue != 0);

		if (theValue < 0)
			*aOut++ = '-';

		while (aCount > 0)
			*aOut++ = aDigits[--aCount];

		*aOut = '\0';
		return theBuffer;
	}
	// GetTickCount() is a Win32 API; supply the equivalent millisecond clock so
	// the game code reads identically on every target.
	inline unsigned long GetTickCount()
	{
		using namespace std::chrono;
		return (unsigned long) duration_cast<milliseconds>(
			steady_clock::now().time_since_epoch()).count();
	}
#endif

// ---------------------------------------------------------------------------
// Byte-order helpers from framework 1.3, used by the save-game/level code.
// All supported targets (x86_64, arm64) are little-endian, so these are
// identities; the big-endian spellings are provided for completeness.
// ---------------------------------------------------------------------------
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
	#define LONG_NATIVE_TO_LITTLEE(x) (uint32_t)(((uint32_t)(x) >> 24) | (((uint32_t)(x) >> 8) & 0xFF00) | (((uint32_t)(x) << 8) & 0xFF0000) | ((uint32_t)(x) << 24))
	#define LONG_LITTLEE_TO_NATIVE(x) LONG_NATIVE_TO_LITTLEE(x)
	#define WORD_NATIVE_TO_LITTLEE(x) (uint16_t)((((uint16_t)(x) >> 8) & 0xFF) | ((uint16_t)(x) << 8))
	#define WORD_LITTLEE_TO_NATIVE(x) WORD_NATIVE_TO_LITTLEE(x)
#else
	#define LONG_NATIVE_TO_LITTLEE(x) (x)
	#define LONG_LITTLEE_TO_NATIVE(x) (x)
	#define WORD_NATIVE_TO_LITTLEE(x) (x)
	#define WORD_LITTLEE_TO_NATIVE(x) (x)
#endif

// ---------------------------------------------------------------------------
// Framework 1.3 handed the game a concrete Image* from GetImage(); the modern
// framework returns GLImage*. Pull in the definition so the implicit
// GLImage* -> Image*/MemoryImage* conversions at the game's call sites work.
// ---------------------------------------------------------------------------
#include <SexyAppFramework/graphics/GLImage.h>

#ifndef _S
#define _S(x) x
#endif

#endif // __ZUMA_COMPAT_H__
