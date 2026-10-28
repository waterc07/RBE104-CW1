/* SDL2 callback mixes looping BGM and prioritized effects inside the game. */
#include <stdio.h>
#include <SDL.h>
#include "audio.h"
static SDL_AudioDeviceID device;
static Uint8 *data[SFX_COUNT+1];
static Uint32 lengths[SFX_COUNT+1], musicPos, effectPos;
static int currentEffect=-1;
static int readSample(const Uint8 *p)
{
    int value=p[0] | (p[1]<<8);
    return value>=32768?value-65536:value;
}
static void mixAudio(void *unused,Uint8 *stream,int length)
{
    int i,value;
    (void)unused;
    SDL_memset(stream,0,(size_t)length);
    for(i=0;i+1<length;i+=2) {
        value=0;
        if(data[SFX_COUNT] && lengths[SFX_COUNT]>=2) {
            value=readSample(data[SFX_COUNT]+musicPos)*55/100;
            musicPos+=2;
            if(musicPos>=lengths[SFX_COUNT]) musicPos=0;
        }
        if(currentEffect>=0) {
            value+=readSample(data[currentEffect]+effectPos);
            effectPos+=2;
            if(effectPos>=lengths[currentEffect]) currentEffect=-1;
        }
        if(value>32767) value=32767;
        if(value<-32768) value=-32768;
        stream[i]=(Uint8)(value&255);
        stream[i+1]=(Uint8)((unsigned int)value>>8);
    }
}
void initAudio(void)
{
    const char *names[]={"step.wav","capture.wav","pickup.wav","delivery.wav","victory.wav","night_shift.wav"};
    SDL_AudioSpec desired,actual,loaded;
    char path[2048];
    char *base=SDL_GetBasePath();
    int i;
    if(SDL_InitSubSystem(SDL_INIT_AUDIO)!=0) {
        fprintf(stderr,"Audio unavailable: %s\n",SDL_GetError());
        SDL_free(base);return;
    }
    SDL_zero(desired);
    desired.freq=22050;desired.format=AUDIO_S16LSB;
    desired.channels=1;desired.samples=512;desired.callback=mixAudio;
    device=SDL_OpenAudioDevice(NULL,0,&desired,&actual,0);
    if(!device) {fprintf(stderr,"Audio unavailable: %s\n",SDL_GetError());SDL_free(base);return;}
    for(i=0;i<=SFX_COUNT;i++) {
        snprintf(path,sizeof path,"%sassets/%s",base?base:"",names[i]);
        if(!SDL_LoadWAV(path,&loaded,&data[i],&lengths[i])) {
            fprintf(stderr,"Missing sound: %s\n",path);continue;
        }
        if(loaded.freq!=desired.freq || loaded.format!=desired.format || loaded.channels!=1 || lengths[i]<2 || lengths[i]%2) {
            fprintf(stderr,"Unsupported sound format: %s\n",path);
            SDL_FreeWAV(data[i]);data[i]=NULL;lengths[i]=0;
        }
    }
    SDL_free(base);
    SDL_PauseAudioDevice(device,0);
}
void playEffect(int effect)
{
    if(!device || effect<0 || effect>=SFX_COUNT || !data[effect]) return;
    SDL_LockAudioDevice(device);
    if(effect!=SFX_STEP || currentEffect<0) {currentEffect=effect;effectPos=0;}
    SDL_UnlockAudioDevice(device);
}
void stopEffects(void)
{
    if(!device) return;
    SDL_LockAudioDevice(device);currentEffect=-1;effectPos=0;SDL_UnlockAudioDevice(device);
}
void closeAudio(void)
{
    int i;
    if(device) SDL_CloseAudioDevice(device);
    device=0;currentEffect=-1;musicPos=effectPos=0;
    for(i=0;i<=SFX_COUNT;i++) {SDL_FreeWAV(data[i]);data[i]=NULL;lengths[i]=0;}
}
