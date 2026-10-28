import math, struct, wave
from pathlib import Path
root=Path(__file__).resolve().parents[1] / 'assets';root.mkdir(parents=True,exist_ok=True)
rate=22050
def save(name,duration,fn):
    data=[]
    for i in range(int(rate*duration)):
        t=i/rate
        env=min(1,t/.004)*min(1,(duration-t)/.025)
        data.append(int(max(-1,min(1,fn(t)*env)) * 19000))
    with wave.open(str(root/name),'wb') as w:
        w.setparams((1,2,rate,0,'NONE','not compressed'))
        w.writeframes(struct.pack('<'+'h'*len(data),*data))
def tone(t,f):
    p=2*math.pi*f*t
    return (math.sin(p)+.22*math.sin(3*p))*.7
save('step.wav',.075,lambda t:.3*tone(t,190)*math.exp(-35*t))
save('capture.wav',.48,lambda t:.52*(math.sin(2*math.pi*(660*t-530*t*t))+.16*math.sin(2*math.pi*77*t))*math.exp(-3*t))
notes=[659.25,830.61,987.77]
save('pickup.wav',.24,lambda t:.42*tone(t,notes[min(2,int(t/.08))]))
# Short rising confirmation, followed by a longer arcade victory melody.
def melody(t, notes, beat):
    index=min(len(notes)-1,int(t/beat))
    local=t-index*beat
    envelope=min(1,local/.005)*min(1,max(0,(beat-local)/.04))
    return .48*tone(local,notes[index])*envelope
save('delivery.wav',.48,lambda t:melody(t,[523.25,659.25,1046.5],.16))
save('victory.wav',1.68,lambda t:melody(t,[523.25,659.25,783.99,1046.5,783.99,1046.5,1318.51],.24))
print('Generated five effects: 22050 Hz, mono PCM16.')
