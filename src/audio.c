#include <stdio.h>
#include <SDL.h>
#include "audio.h"
static SDL_AudioDeviceID device;
static Uint8 *data[SFX_COUNT + 1];
static Uint32 lengths[SFX_COUNT + 1], musicPos, effectPos;
static int currentEffect = -1;
static int readSample(const Uint8 *p)
{
    /* WAV 用小端序保存一个有符号 16 位采样。先拼成 0..65535，
     * 再把高半区转换为负数，避免依赖 char 的符号和主机字节序。 */
    int value = p[0] | (p[1] << 8);
    if (value >= 32768) {
        value -= 65536;
    }
    return value;
}
static void mixAudio(void *unused, Uint8 *stream, int length)
{
    int i, value;
    (void)unused;
    SDL_memset(stream, 0, (size_t)length);
    /* SDL 在音频线程中调用这里。每两个字节处理一个采样，
     * 背景音乐循环，事件音效只播放一次；没有声音时保留静音。 */
    for (i = 0; i + 1 < length; i += 2) {
        value = 0;
        if (data[SFX_COUNT] && lengths[SFX_COUNT] >= 2) {
            value = readSample(data[SFX_COUNT] + musicPos) * 55 / 100;
            musicPos += 2;
            if (musicPos >= lengths[SFX_COUNT]) {
                musicPos = 0;
            }
        }
        if (currentEffect >= 0) {
            value += readSample(data[currentEffect] + effectPos);
            effectPos += 2;
            if (effectPos >= lengths[currentEffect]) {
                currentEffect = -1;
            }
        }
        /* 两路声音相加可能超过 PCM16 范围，截断后再拆成两个字节。 */
        if (value > 32767) {
            value = 32767;
        }
        if (value < -32768) {
            value = -32768;
        }
        stream[i] = (Uint8)(value & 255);
        stream[i + 1] = (Uint8)((unsigned int)value >> 8);
    }
}
void initAudio(void)
{
    /* 前五项与 SFX_* 枚举对应，最后一项留给循环背景音乐。
     * 文件路径以 EXE 所在目录为基准，不依赖启动时的工作目录。 */
    const char *names[] = {"step.wav",     "capture.wav", "pickup.wav",
                           "delivery.wav", "victory.wav", "night_shift.wav"};
    SDL_AudioSpec desired, actual, loaded;
    char path[2048];
    char *base = SDL_GetBasePath();
    int i;
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "Audio unavailable: %s\n", SDL_GetError());
        SDL_free(base);
        return;
    }
    SDL_zero(desired);
    desired.freq = 22050;
    desired.format = AUDIO_S16LSB;
    desired.channels = 1;
    desired.samples = 512;
    desired.callback = mixAudio;
    /* 不允许设备自动换格式，下面的混音按 22050 Hz、单声道 PCM16 编写。 */
    device = SDL_OpenAudioDevice(NULL, 0, &desired, &actual, 0);
    if (!device) {
        fprintf(stderr, "Audio unavailable: %s\n", SDL_GetError());
        SDL_free(base);
        return;
    }
    /* 设备刚打开时处于暂停状态，资源加载完成后才启动回调。
     * 缺失或格式不符的声音单独跳过，仍允许进入游戏。 */
    for (i = 0; i <= SFX_COUNT; i++) {
        snprintf(path, sizeof path, "%sassets/%s", base ? base : "", names[i]);
        if (!SDL_LoadWAV(path, &loaded, &data[i], &lengths[i])) {
            fprintf(stderr, "Missing sound: %s\n", path);
            continue;
        }
        if (loaded.freq != desired.freq || loaded.format != desired.format ||
            loaded.channels != 1 || lengths[i] < 2 || lengths[i] % 2) {
            fprintf(stderr, "Unsupported sound format: %s\n", path);
            SDL_FreeWAV(data[i]);
            data[i] = NULL;
            lengths[i] = 0;
        }
    }
    SDL_free(base);
    SDL_PauseAudioDevice(device, 0);
}
void playEffect(int effect)
{
    if (!device || effect < 0 || effect >= SFX_COUNT || !data[effect]) {
        return;
    }
    /* 主线程和音频回调共用播放位置，修改前锁住设备。
     * 脚步声不能打断正在播放的事件音效，其他事件替换当前音效。 */
    SDL_LockAudioDevice(device);
    if (effect != SFX_STEP || currentEffect < 0) {
        currentEffect = effect;
        effectPos = 0;
    }
    SDL_UnlockAudioDevice(device);
}
void stopEffects(void)
{
    if (!device) {
        return;
    }
    SDL_LockAudioDevice(device);
    currentEffect = -1;
    effectPos = 0;
    SDL_UnlockAudioDevice(device);
}
void closeAudio(void)
{
    int i;
    /* 先关闭设备并等待回调结束，再释放 WAV，避免回调读取已释放的内存。 */
    if (device) {
        SDL_CloseAudioDevice(device);
    }
    device = 0;
    currentEffect = -1;
    musicPos = effectPos = 0;
    for (i = 0; i <= SFX_COUNT; i++) {
        SDL_FreeWAV(data[i]);
        data[i] = NULL;
        lengths[i] = 0;
    }
}
