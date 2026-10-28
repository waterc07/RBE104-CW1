import math
import struct
import wave
from pathlib import Path

root = Path(__file__).resolve().parents[1] / "assets"
root.mkdir(parents=True, exist_ok=True)
rate = 22050


def save(name, duration, sound):
    # sound 接收以秒为单位的时间，返回该时刻的振幅。
    # 先收集采样，再按小端有符号 16 位整数写入，与 audio.c 的格式一致。
    samples = []
    for i in range(int(rate * duration)):
        t = i / rate
        # 开头 4 ms 淡入、结尾 25 ms 淡出，避免声音突然开始或结束。
        envelope = min(1, t / .004) * min(1, (duration - t) / .025)
        value = sound(t) * envelope
        value = max(-1, min(1, value))
        samples.append(int(value * 19000))

    with wave.open(str(root / name), "wb") as output:
        output.setparams((1, 2, rate, 0, "NONE", "not compressed"))
        output.writeframes(struct.pack("<" + "h" * len(samples), *samples))


def tone(t, frequency):
    # 叠加少量三倍频，让音色不只有单一正弦波。
    phase = 2 * math.pi * frequency * t
    return (math.sin(phase) + .22 * math.sin(3 * phase)) * .7


def stepSound(t):
    return .3 * tone(t, 190) * math.exp(-35 * t)


def captureSound(t):
    # 相位中的二次项让主音随时间下降，再叠加低频并逐渐减弱。
    falling = math.sin(2 * math.pi * (660 * t - 530 * t * t))
    low = .16 * math.sin(2 * math.pi * 77 * t)
    return .52 * (falling + low) * math.exp(-3 * t)


def pickupSound(t):
    notes = [659.25, 830.61, 987.77]
    index = min(2, int(t / .08))
    return .42 * tone(t, notes[index])


def melody(t, notes, beat):
    # 先找当前音符，再以该音符开始处为零计算局部时间。
    # 每个音符单独淡入淡出，时间超出时仍使用最后一个有效下标。
    index = min(len(notes) - 1, int(t / beat))
    local = t - index * beat
    envelope = min(1, local / .005) * min(1, max(0, (beat - local) / .04))
    return .48 * tone(local, notes[index]) * envelope


def deliverySound(t):
    return melody(t, [523.25, 659.25, 1046.5], .16)


def victorySound(t):
    return melody(t, [523.25, 659.25, 783.99, 1046.5, 783.99, 1046.5, 1318.51], .24)


save("step.wav", .075, stepSound)
save("capture.wav", .48, captureSound)
save("pickup.wav", .24, pickupSound)
save("delivery.wav", .48, deliverySound)
save("victory.wav", 1.68, victorySound)
print("Generated five effects: 22050 Hz, mono PCM16.")
