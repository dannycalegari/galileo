/* miniaudio_impl.c: the one place miniaudio and stb_vorbis are compiled (see src/ext/README.md). stb_vorbis's
   declarations first, so that miniaudio decodes Ogg Vorbis with it; then its code. */
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#include "miniaudio.h"
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
