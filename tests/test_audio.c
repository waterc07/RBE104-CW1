/* Offline verification of the exact callback used by the SDL device. */
#include <assert.h>
#include <string.h>
#include "../src/audio.c"
int main(void)
{
    const char *names[]={"step.wav","capture.wav","pickup.wav","delivery.wav","victory.wav","night_shift.wav"};
    SDL_AudioSpec spec;
    Uint8 block[1024];
    int i,j,nonzero;
    char path[256];
    for(i=0;i<=SFX_COUNT;i++) {
        snprintf(path,sizeof path,"assets/%s",names[i]);
        assert(SDL_LoadWAV(path,&spec,&data[i],&lengths[i]));
        assert(spec.freq==22050 && spec.channels==1 && spec.format==AUDIO_S16LSB);
    }
    for(i=0;i<SFX_COUNT;i++) {
        Uint8 *music=data[SFX_COUNT];data[SFX_COUNT]=NULL;
        currentEffect=i;effectPos=0;nonzero=0;
        while(currentEffect>=0) {
            mixAudio(NULL,block,sizeof block);
            for(j=0;j<(int)sizeof block;j++) if(block[j]) nonzero=1;
        }
        assert(nonzero);data[SFX_COUNT]=music;
    }
    musicPos=lengths[SFX_COUNT]-2;
    currentEffect=1;effectPos=0;
    mixAudio(NULL,block,sizeof block);
    assert(musicPos==sizeof block-2 && effectPos==sizeof block);
    closeAudio();
    puts("PASS: all WAV formats, all five effect outputs, BGM loop boundary, concurrent mix and cleanup.");
    return 0;
}
