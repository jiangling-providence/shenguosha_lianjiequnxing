#include "audio.h"
#include <SDL2/SDL_mixer.h>

Mix_Music *bgm_normal = NULL;
Mix_Music *bgm_jingliu = NULL;

void audio_init(void)
{
    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048);
    bgm_normal = Mix_LoadMUS("res/audio/bgm_normal.mp3");
    bgm_jingliu = Mix_LoadMUS("res/audio/bgm_jingliu.mp3");
}

void play_bgm0(void)
{
    if (!bgm_normal) return;
    Mix_HaltMusic();
    Mix_PlayMusic(bgm_normal, -1);
}

void play_bgm_jingliu(void)
{
    if (!bgm_jingliu) return;
    Mix_HaltMusic();
    Mix_PlayMusic(bgm_jingliu, -1);
}

void audio_close(void)
{
    Mix_FreeMusic(bgm_normal);
    Mix_FreeMusic(bgm_jingliu);
    Mix_HaltMusic();
    Mix_CloseAudio();
}
