#ifndef PARCEL_AUDIO_H
#define PARCEL_AUDIO_H
/* 枚举值也是音频数组的下标，SFX_COUNT 只表示音效数量，不是事件。 */
enum { SFX_STEP, SFX_CAPTURE, SFX_PICKUP, SFX_DELIVERY, SFX_VICTORY, SFX_COUNT };
/* 在 SDL 视频初始化之后调用；音频不可用时仅输出错误，游戏继续运行。 */
void initAudio(void);
/* 除脚步声外，新音效替换旧音效；脚步声只在没有事件音效时播放。 */
void playEffect(int effect);
/* 只停止事件音效，不重置循环背景音乐的位置。 */
void stopEffects(void);
/* 停止音频回调并释放已加载的 WAV。 */
void closeAudio(void);
#endif
