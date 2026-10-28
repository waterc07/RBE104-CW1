import math, wave, struct
from pathlib import Path
rate=22050
bpm=112
beat=60/bpm
duration=32*beat
buf=[0.0]*int(rate*duration)
def note(midi,start,length,volume,kind):
    freq=440*2**((midi-69)/12)
    for j in range(int(length*rate)):
        i=int(start*rate)+j
        if i>=len(buf): break
        t=j/rate
        env=min(1,t/.012)*min(1,(length-t)/.06)
        phase=2*math.pi*freq*t
        sound=math.sin(phase)
        if kind=='lead': sound=(sound+.25*math.sin(3*phase)+.12*math.sin(5*phase))*.7
        buf[i]+=sound*env*volume
chords=[(57,60,64),(53,57,60),(60,64,67),(55,59,62)]*2
for bar,chord in enumerate(chords):
    for k in range(8):
        note(chord[k%3]+12, (bar*4+k/2)*beat,.34*beat,.075,'lead')
    for k in range(4):
        note(chord[0]-12,(bar*4+k)*beat,.65*beat,.12,'bass')
    for k in (0,2):
        start=int((bar*4+k)*beat*rate)
        for j in range(int(.12*rate)):
            t=j/rate
            buf[start+j]+=.1*math.sin(2*math.pi*(80*t-100*t*t))*math.exp(-30*t)
melody=[76,79,81,79,76,72,74,76,77,76,72,69,72,76,74,72,
        76,79,84,83,81,79,76,74,74,71,74,79,76,74,71,69]
for k,m in enumerate(melody):note(m,k*beat,.65*beat,.09,'lead')
path=Path(__file__).resolve().parents[1] / 'assets' / 'night_shift.wav'
with wave.open(str(path),'wb') as f:
    f.setparams((1,2,rate,0,'NONE','not compressed'))
    f.writeframes(b''.join(struct.pack('<h',int(max(-1,min(1,x)) * 25000)) for x in buf))
print(path, len(buf)/rate)
