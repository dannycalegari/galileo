# src/ext: other people's code, as published

- `miniaudio.h` miniaudio v0.11.26 (2026-10-31) by David Reid, https://github.com/mackron/miniaudio (the file of the
  release tag 0.11.26): sound playback on macOS, Linux and Windows. Public domain or MIT No Attribution, at the
  user's choice (the licence is at the end of the file).
- `stb_vorbis.c` stb_vorbis v1.22 by Sean Barrett and others, from the same release of miniaudio (its
  `extras/stb_vorbis.c`): the Ogg Vorbis decoder miniaudio uses for the music. Public domain or MIT (at the end of
  the file).
- `miniaudio_impl.c` ours: compiles the two into one object (src/audio.cc and src/sound.cc use them).

Both files are unchanged from their sources.
