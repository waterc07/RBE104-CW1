/* 直接调用正式音频回调，不打开声音设备；只能验证数据，不能验证听感。 */
#include <assert.h>
#include <string.h>
#include "../src/audio.c"
int main(void)
{
    const char *names[] = {"step.wav",     "capture.wav", "pickup.wav",
                           "delivery.wav", "victory.wav", "night_shift.wav"};
    SDL_AudioSpec spec;
    Uint8 block[1024];
    int i, j, nonzero;
    char path[256];
    for (i = 0; i <= SFX_COUNT; i++) {
        snprintf(path, sizeof path, "assets/%s", names[i]);
        assert(SDL_LoadWAV(path, &spec, &data[i], &lengths[i]));
        assert(spec.freq == 22050 && spec.channels == 1 && spec.format == AUDIO_S16LSB);
    }
    /* 暂时关掉背景音乐，防止音乐掩盖某个音效没有输出的问题。
     * 一直混音到该音效播完，确认至少有一个非零字节，再恢复音乐。 */
    for (i = 0; i < SFX_COUNT; i++) {
        Uint8 *music = data[SFX_COUNT];
        data[SFX_COUNT] = NULL;
        currentEffect = i;
        effectPos = 0;
        nonzero = 0;
        while (currentEffect >= 0) {
            mixAudio(NULL, block, sizeof block);
            for (j = 0; j < (int)sizeof block; j++) {
                if (block[j]) {
                    nonzero = 1;
                }
            }
        }
        assert(nonzero);
        data[SFX_COUNT] = music;
    }
    /* 从音乐最后一个采样开始，检查同一个输出块内能回到音乐开头。
     * 同时播放事件音效，确认两条播放位置各自前进。 */
    musicPos = lengths[SFX_COUNT] - 2;
    currentEffect = 1;
    effectPos = 0;
    mixAudio(NULL, block, sizeof block);
    assert(musicPos == sizeof block - 2 && effectPos == sizeof block);
    closeAudio();
    puts(
        "PASS: all WAV formats, all five effect outputs, BGM loop boundary, concurrent mix and cleanup.");
    return 0;
}
