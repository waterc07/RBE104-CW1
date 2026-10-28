#ifndef PARCEL_AUDIO_H
#define PARCEL_AUDIO_H
enum { SFX_STEP, SFX_CAPTURE, SFX_PICKUP, SFX_DELIVERY, SFX_VICTORY, SFX_COUNT };
void initAudio(void);
void playEffect(int effect);
void stopEffects(void);
void closeAudio(void);
#endif
