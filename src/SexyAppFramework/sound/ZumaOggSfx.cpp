//
// ZumaOggSfx.cpp - Ogg Vorbis -> Mix_Chunk decoding. See ZumaOggSfx.h.
//
#include "ZumaOggSfx.h"

#include <vector>

#include <SDL.h>
#include <SDL_mixer.h>

#include "Common.h"

// Compile the (public) stb_vorbis implementation in this translation unit.
// SDL-Mixer-X's internal copy is renamed to _mix_stb_vorbis_*, so the two
// coexist in the same binary.
#include "SDL-Mixer-X/src/codecs/stb_vorbis/stb_vorbis.h"

namespace
{

void WriteLE32(uint8_t* thePtr, uint32_t theValue)
{
	thePtr[0] = uint8_t(theValue & 0xFF);
	thePtr[1] = uint8_t((theValue >> 8) & 0xFF);
	thePtr[2] = uint8_t((theValue >> 16) & 0xFF);
	thePtr[3] = uint8_t((theValue >> 24) & 0xFF);
}

void WriteLE16(uint8_t* thePtr, uint16_t theValue)
{
	thePtr[0] = uint8_t(theValue & 0xFF);
	thePtr[1] = uint8_t((theValue >> 8) & 0xFF);
}

// Synthesise a canonical 16-bit PCM WAV in memory so SDL_mixer's loader (which
// does understand WAV, and converts it to the mixer's format) can build a chunk.
uint8_t* BuildWav(const std::vector<short>& thePcm, int theChannels, int theSampleRate,
				  uint32_t& theOutSize)
{
	const uint32_t aPcmBytes = uint32_t(thePcm.size() * sizeof(short));
	const uint32_t aRate = uint32_t(theSampleRate);
	const uint16_t aChannels = uint16_t(theChannels);
	const uint16_t aBits = 16;
	const uint16_t aBlockAlign = uint16_t(aChannels * aBits / 8);
	const uint32_t aByteRate = aRate * aBlockAlign;

	theOutSize = 44 + aPcmBytes;
	uint8_t* aWav = (uint8_t*)SDL_malloc(theOutSize);
	if (aWav == nullptr)
		return nullptr;

	uint8_t* p = aWav;
	SDL_memcpy(p, "RIFF", 4); p += 4;
	WriteLE32(p, 36 + aPcmBytes); p += 4;
	SDL_memcpy(p, "WAVE", 4); p += 4;

	SDL_memcpy(p, "fmt ", 4); p += 4;
	WriteLE32(p, 16); p += 4;			// fmt chunk size
	WriteLE16(p, 1); p += 2;			// PCM
	WriteLE16(p, aChannels); p += 2;
	WriteLE32(p, aRate); p += 4;
	WriteLE32(p, aByteRate); p += 4;
	WriteLE16(p, aBlockAlign); p += 2;
	WriteLE16(p, aBits); p += 2;

	SDL_memcpy(p, "data", 4); p += 4;
	WriteLE32(p, aPcmBytes); p += 4;

	SDL_memcpy(p, thePcm.data(), aPcmBytes);

	return aWav;
}

} // namespace

Mix_Chunk* ZumaLoadOggChunkFromMemory(const uint8_t* theData, size_t theSize)
{
	if (theData == nullptr || theSize == 0)
	{
		Sexy::LogInfoLn("[zuma-port diag] ogg: null/empty data");
		return nullptr;
	}

	int anError = 0;
	stb_vorbis* aVorbis = stb_vorbis_open_memory((const unsigned char*)theData,
												 (int)theSize, &anError, nullptr);
	if (aVorbis == nullptr)
	{
		Sexy::LogInfoLn("[zuma-port diag] ogg: stb_vorbis_open_memory failed, err={}", anError);
		return nullptr;
	}

	const stb_vorbis_info aInfo = stb_vorbis_get_info(aVorbis);
	const int aChannels = aInfo.channels;
	const int aRate = int(aInfo.sample_rate);

	// Whole stream, interleaved, native rate/channels.
	unsigned int aFrames = stb_vorbis_stream_length_in_samples(aVorbis);
	if (aFrames == 0)
	{
		Sexy::LogInfoLn("[zuma-port diag] ogg: stream_length_in_samples returned 0");
		stb_vorbis_close(aVorbis);
		return nullptr;
	}

	std::vector<short> aPcm(size_t(aFrames) * size_t(aChannels));
	int aGot = stb_vorbis_get_samples_short_interleaved(aVorbis, aChannels,
													   aPcm.data(), int(aPcm.size()));
	stb_vorbis_close(aVorbis);

	if (aGot <= 0)
		return nullptr;

	aPcm.resize(size_t(aGot) * size_t(aChannels));

	uint32_t aWavSize = 0;
	uint8_t* aWav = BuildWav(aPcm, aChannels, aRate, aWavSize);
	if (aWav == nullptr)
	{
		Sexy::LogInfoLn("[zuma-port diag] ogg: BuildWav failed");
		return nullptr;
	}

	// Mix_LoadWAV_RW copies (and format-converts) the samples, so the staging
	// buffer can go away as soon as it returns.
	Mix_Chunk* aChunk = Mix_LoadWAV_RW(SDL_RWFromMem(aWav, int(aWavSize)), 1);
	SDL_free(aWav);

	if (aChunk == nullptr)
		Sexy::LogInfoLn("[zuma-port diag] ogg: decoded {} frames {}ch {}Hz but Mix_LoadWAV_RW failed: {}",
						aGot, aChannels, aRate, Mix_GetError());

	return aChunk;
}
