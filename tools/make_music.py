import math
import random
import struct
import wave
from pathlib import Path

rate = 22050
bpm = 124
beat = 60 / bpm

# 八个小节共三十二拍。所有声部累加到同一个浮点缓冲区，最后统一混音。
samples = [0.0] * round(rate * 32 * beat)
# 固定种子让鼓的噪声成分在每次运行中完全一致。
rng = random.Random(104)


def note(midi, start, length, volume, kind):
    # MIDI 69 是 440 Hz，每升高十二个编号，频率翻倍。
    frequency = 440 * 2 ** ((midi - 69) / 12)
    offset = round(start * rate)
    for j in range(round(length * rate)):
        index = offset + j
        if index >= len(samples):
            break
        t = j / rate
        envelope = min(1, t / .008) * min(1, (length - t) / .04)
        phase = 2 * math.pi * frequency * t
        if kind == "bass":
            # 低音只叠二次谐波，保持厚实而不刺耳。
            sound = (math.sin(phase) + .22 * math.sin(2 * phase)) * .82
        else:
            # 短促的奇次谐波音色，作为和弦与旋律的断奏声部。
            sound = (math.sin(phase) + .24 * math.sin(3 * phase)
                     + .08 * math.sin(5 * phase)) * .76
        samples[index] += sound * envelope * volume


def drum(start, kind, volume):
    # 噪声经过一阶低通；军鼓再叠 165 Hz 正弦，底鼓用上滑相位。
    length = .16 if kind == "kick" else .11 if kind == "snare" else .045
    offset = round(start * rate)
    filtered = 0.0
    for j in range(round(length * rate)):
        index = offset + j
        if index >= len(samples):
            break
        t = j / rate
        attack = min(1, t / .002)
        filtered = .65 * filtered + .35 * rng.uniform(-1, 1)
        if kind == "kick":
            phase = 2 * math.pi * (54 * t + 3.4 * (1 - math.exp(-35 * t)))
            sound = math.sin(phase) * math.exp(-25 * t)
        elif kind == "snare":
            sound = (.72 * filtered + .28 * math.sin(2 * math.pi * 165 * t)) * math.exp(-32 * t)
        else:
            sound = filtered * math.exp(-65 * t)
        samples[index] += volume * attack * sound


# Am - F - Dm - E 两轮，保留小调色彩，结尾前仍有追逐的紧张感。
chords = [(57, 60, 64), (53, 57, 60), (50, 53, 57), (52, 56, 59)] * 2
pattern = [0, 2, 1, 2, 0, 1, 2, 1]
for bar, chord in enumerate(chords):
    # 每半拍弹一个和弦音；低音在反拍加七度推进，踩镲落在同一时间点。
    for k in range(8):
        start = (bar * 4 + k / 2) * beat
        note(chord[pattern[k]], start, .30 * beat, .065, "pulse")
        note(chord[0] - 12 + (7 if k in (3, 7) else 0),
             start, .36 * beat, .145 if k % 2 == 0 else .095, "bass")
        drum(start, "hat", .045 if k % 2 == 0 else .035)
    # 底鼓在正拍与反拍之间交替，军鼓固定落在第二、四拍。
    for k in (0, 1.5, 2, 3.5):
        drum((bar * 4 + k) * beat, "kick", .17)
    for k in (1, 3):
        drum((bar * 4 + k) * beat, "snare", .115)

# 旋律沿用上行下行动机，整体比旧版低一个八度，最后四音落到低音 A 收束。
melody = [64, 67, 69, 67, 64, 60, 62, 64,
          65, 64, 62, 57, 64, 62, 59, 56,
          64, 67, 72, 71, 69, 67, 64, 62,
          62, 65, 64, 60, 59, 56, 52, 57]
for k, midi in enumerate(melody):
    note(midi, k * beat, (.72 if k == 31 else .62) * beat, .105, "lead")

# 两端各淡入淡出 6 ms，消掉缓冲区边界上的爆音；再按峰值留出混音余量。
edge = round(.006 * rate)
for i in range(edge):
    samples[i] *= i / edge
    samples[-1 - i] *= i / edge
peak = max(abs(value) for value in samples)
gain = min(1.0, .72 / peak)

# 混音完成后限制振幅，再转换成 audio.c 要求的小端单声道 PCM16。
pcm = [round(value * gain * 32767) for value in samples]
path = Path(__file__).resolve().parents[1] / "assets" / "night_shift.wav"
with wave.open(str(path), "wb") as output:
    output.setparams((1, 2, rate, 0, "NONE", "not compressed"))
    output.writeframes(struct.pack("<" + "h" * len(pcm), *pcm))
print(path, f"{len(samples) / rate:.2f}s", f"{bpm} BPM")
