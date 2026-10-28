import math
import wave
import struct
from pathlib import Path

rate = 22050
bpm = 112
beat = 60 / bpm

# 八个小节，每小节四拍。所有声部累加到同一个浮点采样数组中。
duration = 32 * beat
samples = [0.0] * int(rate * duration)


def note(midi, start, length, volume, kind):
    # MIDI 69 是 440 Hz，每升高十二个编号，频率翻倍。
    frequency = 440 * 2 ** ((midi - 69) / 12)
    for j in range(int(length * rate)):
        index = int(start * rate) + j
        if index >= len(samples):
            break
        t = j / rate
        envelope = min(1, t / .012) * min(1, (length - t) / .06)
        phase = 2 * math.pi * frequency * t
        sound = math.sin(phase)
        if kind == "lead":
            sound = (sound + .25 * math.sin(3 * phase) + .12 * math.sin(5 * phase)) * .7
        samples[index] += sound * envelope * volume


chords = [(57, 60, 64), (53, 57, 60), (60, 64, 67), (55, 59, 62)] * 2
for bar, chord in enumerate(chords):
    # 每半拍轮流播放一个和弦音，低音每拍播放一次。
    for k in range(8):
        note(chord[k % 3] + 12, (bar * 4 + k / 2) * beat, .34 * beat, .075, "lead")
    for k in range(4):
        note(chord[0] - 12, (bar * 4 + k) * beat, .65 * beat, .12, "bass")
    # 第一、三拍加入短促的下降音，作为节奏声部。
    for k in (0, 2):
        start = int((bar * 4 + k) * beat * rate)
        for j in range(int(.12 * rate)):
            t = j / rate
            samples[start + j] += .1 * math.sin(2 * math.pi * (80 * t - 100 * t * t)) * math.exp(-30 * t)

melody = [76, 79, 81, 79, 76, 72, 74, 76, 77, 76, 72, 69, 72, 76, 74, 72,
          76, 79, 84, 83, 81, 79, 76, 74, 74, 71, 74, 79, 76, 74, 71, 69]
for k, midi in enumerate(melody):
    note(midi, k * beat, .65 * beat, .09, "lead")

# 混音完成后限制振幅，再转换成 audio.c 要求的小端单声道 PCM16。
data = bytearray()
for value in samples:
    value = max(-1, min(1, value))
    data.extend(struct.pack("<h", int(value * 25000)))

path = Path(__file__).resolve().parents[1] / "assets" / "night_shift.wav"
with wave.open(str(path), "wb") as output:
    output.setparams((1, 2, rate, 0, "NONE", "not compressed"))
    output.writeframes(data)
print(path, len(samples) / rate)
