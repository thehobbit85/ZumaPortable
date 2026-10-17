//
// ZumaOggSfx.h - Ogg Vorbis sound-effect loading for this port.
//
// SDL-Mixer-X's Mix_LoadWAV_RW only understands sample formats (WAV/AIFF/VOC);
// compressed formats go through its music interface. Zuma Deluxe ships every
// sound effect as Ogg Vorbis, so they must be decoded to PCM before SDL_mixer
// will accept them as chunks.
//
// stb_vorbis already ships with SDL-Mixer-X (which renames its own copy to
// _mix_stb_vorbis_* to avoid exactly this kind of collision), so this file
// compiles the public copy and hands SDL_mixer a synthesised in-memory WAV.
//
#ifndef __ZUMA_OGG_SFX_H__
#define __ZUMA_OGG_SFX_H__

#include <cstddef>
#include <cstdint>

struct Mix_Chunk;

// Returns a mixer chunk for the given Ogg Vorbis stream, or nullptr on failure.
// The returned chunk is owned by the caller (Mix_FreeChunk).
Mix_Chunk* ZumaLoadOggChunkFromMemory(const uint8_t* theData, size_t theSize);

#endif // __ZUMA_OGG_SFX_H__
