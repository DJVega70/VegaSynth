# Makes a fake "Salamander-style" folder of sine-wave notes (A0..C8 in minor thirds, 16 velocity layers) for tests/piano_test.cpp.
# Usage: python3 tools/make_fake_piano.py fakepiano
import numpy as np, wave, os, sys
d = os.path.join(sys.argv[1] if len(sys.argv) > 1 else 'fakepiano', '44.1khz16bit'); os.makedirs(d, exist_ok=True)
names = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
for m in range(21, 109, 3):
    f = 440 * 2 ** ((m - 69) / 12); t = np.arange(int(44100 * 0.4)) / 44100
    for L in range(1, 17):
        x = (0.04 + 0.055 * L) * np.sin(2 * np.pi * f * t) * np.exp(-t / 0.3)
        w = wave.open(f'{d}/{names[m % 12]}{m // 12 - 1}v{L}.wav', 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100); w.writeframes((x * 32767).astype('<i2').tobytes()); w.close()
for n in ['rel1', 'rel2', 'pedalD1', 'pedalU1', 'harmD3']:
    w = wave.open(f'{d}/{n}.wav', 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100); w.writeframes(np.zeros(4000, '<i2').tobytes()); w.close()
