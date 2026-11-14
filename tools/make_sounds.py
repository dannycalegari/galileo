#!/usr/bin/env python3
"""Makes the game's sound effects, data/sounds/*.wav, by synthesis: nothing recorded, nothing
downloaded. Each sound is a small function below; the script is the master copy of the files.

    python3 tools/make_sounds.py            all of them
    python3 tools/make_sounds.py bell coins only those

The ambient sounds are sets of short variants (birds_1.wav ...), played now and then at random;
the loops (the river, the hooves) are made a little longer than their length and folded, so
that they join without a click when played over and over.
"""
import math, os, random, struct, sys, wave

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "data", "sounds")
TAU = 2 * math.pi


ONE_SHOTS = ("bell_big", "bell_small", "coins", "anvil", "sword", "cannon")   # (the ambient variants fade themselves)


def write(name, samples, rate):
    os.makedirs(OUT, exist_ok=True)
    if name in ONE_SHOTS:   # a short fade at the very end, so that it never stops with a click
        m = int(rate * 0.05)
        for i in range(m): samples[-m + i] *= 0.5 + 0.5 * math.cos(math.pi * i / m)
    peak = max(1e-9, max(abs(x) for x in samples))
    k = 0.89 / peak if peak > 0.89 else 1.0
    with wave.open(os.path.join(OUT, name + ".wav"), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, x * k)) * 32767)) for x in samples))
    print(f"  {name}.wav  {len(samples) / rate:.1f} s")


def fold(buf, n, xf):
    """a loop of n samples from buf (n + xf long): the tail crossfaded onto the head"""
    out = buf[:n]
    for i in range(xf):
        t = i / xf
        out[i] = buf[n + i] * (1 - t) + out[i] * t
    return out


class Biquad:   # RBJ's cookbook: band-pass (constant peak), low-pass, high-pass
    def __init__(self, kind, f, q, rate):
        w = TAU * f / rate
        a = math.sin(w) / (2 * q)
        c = math.cos(w)
        if kind == "bp": b0, b1, b2 = a, 0, -a
        elif kind == "lp": b0, b1, b2 = (1 - c) / 2, 1 - c, (1 - c) / 2
        else: b0, b1, b2 = (1 + c) / 2, -(1 + c), (1 + c) / 2
        a0, a1, a2 = 1 + a, -2 * c, 1 - a
        self.b = (b0 / a0, b1 / a0, b2 / a0); self.a = (a1 / a0, a2 / a0)
        self.x1 = self.x2 = self.y1 = self.y2 = 0.0

    def __call__(self, x):
        b0, b1, b2 = self.b; a1, a2 = self.a
        y = b0 * x + b1 * self.x1 + b2 * self.x2 - a1 * self.y1 - a2 * self.y2
        self.x2, self.x1, self.y2, self.y1 = self.x1, x, self.y1, y
        return y


def partials(rate, secs, parts, strike=0.0, strike_f=2000):
    """a struck thing: sum of (freq, amp, decay seconds) sines, each a pair a little apart (it
    beats, as a bell does), plus a short noisy strike"""
    n = int(rate * secs)
    out = [0.0] * n
    rnd = random.Random(int(parts[0][0] * 100))
    for f, amp, dec in parts:
        ph1, ph2 = rnd.random() * TAU, rnd.random() * TAU
        df = 0.25 + 0.002 * f
        for i in range(n):
            t = i / rate
            e = amp * math.exp(-t * 6.9 / dec) * min(1.0, t / 0.002)
            out[i] += e * 0.5 * (math.sin(TAU * f * t + ph1) + math.sin(TAU * (f + df) * t + ph2))
    if strike:
        bp = Biquad("bp", strike_f, 1.2, rate)
        for i in range(int(rate * 0.03)):
            t = i / rate
            out[i] += strike * bp(rnd.uniform(-1, 1)) * math.exp(-t / 0.006)
    return out


# ---------------------------------------------------------------- the sounds

def bell(prime, name, secs=7.0):
    # a church bell's partials (the minor-third bell): hum, prime, tierce, quint, nominal ...
    P = prime
    parts = [(0.5 * P, 0.40, 7.0), (P, 0.30, 5.0), (1.2 * P, 0.34, 4.2), (1.5 * P, 0.12, 3.0), (2.0 * P, 0.50, 3.6),
             (2.5 * P, 0.14, 1.8), (3.0 * P, 0.18, 1.4), (4.0 * P, 0.12, 0.9), (5.33 * P, 0.07, 0.6), (6.7 * P, 0.04, 0.4)]
    write(name, partials(44100, secs, parts, strike=0.6, strike_f=3 * P), 44100)


def coins():
    rate, n = 44100, int(44100 * 0.7)
    out = [0.0] * n
    rnd = random.Random(7)
    for k in range(5):
        at = int(rate * (0.02 + rnd.random() * 0.4))
        s = rnd.uniform(0.93, 1.08)
        c = partials(rate, 0.25, [(3150 * s, 0.5, 0.12), (5120 * s, 0.35, 0.09), (7430 * s, 0.25, 0.07), (9830 * s, 0.15, 0.05)],
                     strike=0.3, strike_f=6000)
        g = rnd.uniform(0.5, 1.0)
        for i, x in enumerate(c):
            if at + i < n: out[at + i] += g * x
    write("coins", out, rate)


def anvil():
    rate = 44100
    out = partials(rate, 1.3, [(1150, 0.5, 0.9), (2810, 0.35, 0.6), (4330, 0.25, 0.4), (6510, 0.15, 0.25), (8220, 0.08, 0.15)],
                   strike=0.5, strike_f=3000)
    for i in range(int(rate * 0.06)):   # the thud of the hammer
        t = i / rate
        out[i] += 0.5 * math.sin(TAU * 130 * t) * math.exp(-t / 0.012)
    write("anvil", out, rate)


def sword():
    rate = 44100
    out = partials(rate, 0.8, [(2310, 0.4, 0.35), (3710, 0.35, 0.25), (5930, 0.25, 0.18), (8110, 0.15, 0.12)], strike=0.9, strike_f=4500)
    write("sword", out, rate)


def cannon():
    """a gun fired some way off: a crack, a deep boom, and the roll of it along the hills"""
    rate, secs = 22050, 3.2
    n = int(rate * secs)
    out = [0.0] * n
    rnd = random.Random(11)
    lp1, lp2, lp3 = Biquad("lp", 900, 0.7, rate), Biquad("lp", 160, 0.8, rate), Biquad("lp", 320, 0.6, rate)
    for i in range(n):
        t = i / rate
        x = rnd.uniform(-1, 1)
        crack = lp1(x) * math.exp(-t / 0.015) * 1.2                      # the report
        boom = lp2(x) * 7.0 * math.exp(-t / 0.35) * min(1.0, t / 0.004)  # the body of it
        thump = math.sin(TAU * (48 - 14 * t) * t) * 0.9 * math.exp(-t / 0.18)
        roll = lp3(x) * 1.6 * math.exp(-max(0.0, t - 0.25) / 1.1) * min(1.0, t / 0.3) * (0.7 + 0.3 * math.sin(TAU * 1.7 * t))   # the echo
        out[i] = crack + boom + thump + roll
    write("cannon", out, rate)


def musket_shot(rnd, rate, dist):
    """one matchlock going off some way off: the crack, the body of the powder, and its echo; DIST 0 near .. 1 far
    (the far one duller and with more of its echo)"""
    n = int(rate * 1.3)
    out = [0.0] * n
    hp = Biquad("hp", 1500, 0.7, rate)
    lp = Biquad("lp", 1400 - 900 * dist, 0.7, rate)
    lpe = Biquad("lp", 500, 0.6, rate)
    g = 1.0 - 0.45 * dist
    for i in range(n):
        t = i / rate
        x = rnd.uniform(-1, 1)
        crack = hp(x) * 2.2 * (1 - dist) * math.exp(-t / 0.004)
        body = lp(x) * 3.5 * math.exp(-t / (0.05 + 0.04 * dist)) * min(1.0, t / 0.001)
        echo = lpe(x) * (0.5 + 0.6 * dist) * math.exp(-max(0.0, t - 0.08) / 0.35) * min(1.0, t / 0.08)
        out[i] = g * (crack + body + echo)
    return out


def musket(k, rate=22050):
    """a single musket shot, one of several (some near, some far)"""
    rnd = random.Random(300 + k)
    return fade_ends(pad(musket_shot(rnd, rate, rnd.uniform(0.1, 0.8)), rate, 0.01, 0.1), rate, 0.01)


def volley(k, rate=22050):
    """a rank firing: eight to fourteen muskets in a second or so, not together (the ragged volley of men whose
    matches catch one after another), and the roll of it"""
    rnd = random.Random(400 + k)
    n = int(rate * 3.2)
    out = [0.0] * n
    shots = rnd.randint(8, 14)
    for j in range(shots):
        at = int(rate * (0.02 + abs(rnd.gauss(0, 0.28)) * (1 if j else 0)))
        shot = musket_shot(random.Random(rnd.random()), rate, rnd.uniform(0.15, 0.7))
        a = rnd.uniform(0.45, 1.0)
        for i, x in enumerate(shot):
            if at + i < n: out[at + i] += a * x
    return fade_ends(out, rate, 0.02)


def clash(k, rate=44100):
    """steel on steel: a sword against a sword or a breastplate, each a little different"""
    rnd = random.Random(500 + k)
    f = rnd.uniform(0.85, 1.2)
    out = partials(rate, 0.6, [(2100 * f, 0.4, 0.25), (3400 * f, 0.35, 0.18), (5200 * f, 0.25, 0.12), (7600 * f, 0.15, 0.08)],
                   strike=1.0, strike_f=4000 * f)
    return fade_ends(out, rate, 0.02)


def knock(k, rate=22050):
    """wood on wood: a pike's shaft against another, or the butt of a musket swung as a club; a dull blow"""
    rnd = random.Random(600 + k)
    n = int(rate * 0.35)
    out = [0.0] * n
    bp = Biquad("bp", rnd.uniform(450, 850), 2.5, rate)
    bp2 = Biquad("bp", rnd.uniform(1600, 2400), 3.0, rate)
    for i in range(n):
        t = i / rate
        x = rnd.uniform(-1, 1)
        out[i] = (bp(x) * 4.0 * math.exp(-t / 0.03) + bp2(x) * 0.8 * math.exp(-t / 0.008)
                  + 0.6 * math.sin(TAU * 140 * t) * math.exp(-t / 0.02))
    return fade_ends(out, rate, 0.01)


def one_shot_variants(name, make, n, rate):
    for k in range(n): write(f"{name}_{k + 1}", make(k), rate)


def hooves():
    """a horse walking on a road: four beats a stride, a stride a second; two strides"""
    rate, secs = 22050, 2.0
    n = int(rate * secs)
    out = [0.0] * n
    rnd = random.Random(3)
    for k in range(8):
        at = int(rate * (k * 0.25 + rnd.uniform(-0.01, 0.01)))
        f = 380 if k % 2 == 0 else 300
        bp = Biquad("bp", f, 3.0, rate)
        bp2 = Biquad("bp", 1800, 2.0, rate)
        g = 0.8 + 0.2 * rnd.random()
        for i in range(int(rate * 0.09)):
            t = i / rate
            x = rnd.uniform(-1, 1)
            v = g * (bp(x) * 3.0 * math.exp(-t / 0.018) + bp2(x) * 0.6 * math.exp(-t / 0.004))
            if at + i < n: out[at + i] += v
    write("hooves", out, rate)


# ---------------------------------------------------------------- the ambient sounds
# Each is a set of short variants (NAME_1.wav ...), which the game plays one at a time, now and
# then, at random (sounds.txt "ambient"): a bird's phrase, a cricket's few chirps, a snatch of
# talk, one wave, one gust. Only the river is a continuous bed, smooth and faint.

def pad(out, rate, head=0.05, tail=0.3):
    return [0.0] * int(rate * head) + out + [0.0] * int(rate * tail)


def fade_ends(out, rate, secs=0.04):
    m = min(len(out) // 2, int(rate * secs))
    for i in range(m):
        g = 0.5 - 0.5 * math.cos(math.pi * i / m)
        out[i] *= g; out[-1 - i] *= g
    return out


def crickets_variant(k, rate=22050):
    rnd = random.Random(100 + k)
    secs = rnd.uniform(1.5, 4.0)
    out = [0.0] * int(rate * secs)
    for c in range(1 if rnd.random() < 0.6 else 2):
        f = rnd.uniform(4100, 4900)
        pulses = rnd.choice([2, 3, 3, 4])
        g = rnd.uniform(0.5, 1.0) * (1 if c == 0 else 0.5)
        t0 = rnd.uniform(0.0, 0.4)
        while t0 < secs - 0.3:
            for p in range(pulses):
                st = int(rate * (t0 + p * 0.024))
                for i in range(int(rate * 0.014)):
                    if st + i >= len(out): break
                    t = i / rate
                    out[st + i] += g * math.sin(math.pi * t / 0.014) ** 2 * math.sin(TAU * f * (st + i) / rate)
            t0 += rnd.uniform(0.35, 1.1)   # irregular: crickets are not clocks
    return fade_ends(pad(out, rate), rate)


def birds_variant(k, rate=22050):
    rnd = random.Random(200 + k)
    out = [0.0] * int(rate * 3.5)
    def note(st, dur, f0, f1, amp, vib=0.0):
        ph = 0.0
        for i in range(int(rate * dur)):
            if st + i >= len(out): break
            t = i / (rate * dur)
            f = f0 + (f1 - f0) * t + vib * math.sin(TAU * 30 * i / rate)
            ph += TAU * f / rate
            out[st + i] += amp * math.sin(math.pi * t) ** 2 * math.sin(ph)
    st = int(rate * 0.05)
    kind = k % 5
    if kind in (0, 3):   # a warbler: a run of quick notes
        for j in range(rnd.randint(5, 12)):
            d = rnd.uniform(0.035, 0.07)
            f = rnd.uniform(2600, 5400)
            note(st, d, f, f * rnd.uniform(0.85, 1.2), rnd.uniform(0.5, 0.9), vib=rnd.choice([0, 0, 150]))
            st += int(rate * (d + rnd.uniform(0.01, 0.05)))
    elif kind in (1, 4):   # a whistle of two or three notes
        for j in range(rnd.randint(2, 3)):
            d = rnd.uniform(0.15, 0.3)
            f = rnd.uniform(1800, 3000)
            note(st, d, f, f * rnd.uniform(1.05, 1.35), rnd.uniform(0.5, 0.8))
            st += int(rate * (d + rnd.uniform(0.06, 0.15)))
    else:   # a cuckoo, far off
        for f in (660, 540):
            note(st, 0.26, f, f * 0.98, 0.5)
            st += int(rate * 0.42)
    out = out[:min(len(out), st + int(rate * 0.1))]
    if rnd.random() < 0.5:   # some far off: softer, duller
        lp = Biquad("lp", 2500, 0.7, rate)
        out = [0.6 * lp(x) for x in out]
    return fade_ends(pad(out, rate), rate)


def waves_variant(k, rate=22050):
    """one wave: it rises, breaks with a hiss, and draws back over the shingle"""
    rnd = random.Random(300 + k)
    rise, back = rnd.uniform(0.9, 1.6), rnd.uniform(3.0, 5.5)
    n = int(rate * (rise + back + 0.5))
    out = [0.0] * n
    hp = Biquad("hp", 2200, 0.7, rate)
    b = 0.0
    lp = Biquad("lp", 400, 0.7, rate)
    for i in range(n):
        t = i / rate
        if i % 64 == 0:   # the roar opens as the wave breaks, and closes as it draws back
            c = 300 + 1200 * math.exp(-((t - rise) / 0.6) ** 2)
            lp = Biquad("lp", c, 0.7, rate) if i == 0 else lp
            w = TAU * c / rate; al = math.sin(w) / 1.4; co = math.cos(w); a0 = 1 + al
            lp.b = ((1 - co) / 2 / a0, (1 - co) / a0, (1 - co) / 2 / a0); lp.a = (-2 * co / a0, (1 - al) / a0)
        x = rnd.uniform(-1, 1)
        b = 0.97 * b + 0.03 * x
        env = (t / rise) ** 2 if t < rise else math.exp(-(t - rise) / (back * 0.45))
        crest = math.exp(-((t - rise - 0.15) / 0.35) ** 2)
        out[i] = env * lp(b * 8) + 0.35 * crest * hp(x)
    return fade_ends(out, rate, 0.3)


def wind_variant(k, rate=22050):
    """a gust: it gathers, blows, and dies away"""
    rnd = random.Random(400 + k)
    up, down = rnd.uniform(1.0, 2.5), rnd.uniform(2.0, 4.5)
    n = int(rate * (up + down))
    out = [0.0] * n
    lp = Biquad("lp", 300, 0.8, rate)
    for i in range(n):
        t = i / rate
        if i % 64 == 0:
            env = math.sin(0.5 * math.pi * t / up) if t < up else math.cos(0.5 * math.pi * (t - up) / down)
            c = 250 + 550 * env
            w = TAU * c / rate; al = math.sin(w) / 1.6; co = math.cos(w); a0 = 1 + al
            lp.b = ((1 - co) / 2 / a0, (1 - co) / a0, (1 - co) / 2 / a0); lp.a = (-2 * co / a0, (1 - al) / a0)
        e = math.sin(0.5 * math.pi * t / up) ** 2 if t < up else math.cos(0.5 * math.pi * (t - up) / down) ** 2
        out[i] = e * lp(rnd.uniform(-1, 1))
    return fade_ends(out, rate, 0.2)


VOWELS = [(730, 1090), (530, 1840), (270, 2290), (570, 840), (300, 870), (660, 1700), (440, 1020)]


def town_variant(k, rate=22050):
    """people talking a little way off: one or two voices, a phrase each, in syllables with
    vowels and a rise and fall of pitch, heard through the air of a street"""
    rnd = random.Random(500 + k)
    out = []
    for v in range(1 if rnd.random() < 0.5 else 2):
        f0 = rnd.uniform(95, 140) if rnd.random() < 0.55 else rnd.uniform(180, 240)
        nsyl = rnd.randint(4, 11)
        b1 = Biquad("bp", 500, 4.0, rate); b2 = Biquad("bp", 1500, 6.0, rate)
        ph = 0.0
        phrase = []
        for j in range(nsyl):
            d = rnd.uniform(0.09, 0.26)
            F1, F2 = rnd.choice(VOWELS)
            for f, bq, q in ((F1, b1, 4.0), (F2, b2, 6.0)):
                w = TAU * f / rate; al = math.sin(w) / (2 * q); a0 = 1 + al
                bq.b = (al / a0, 0, -al / a0); bq.a = (-2 * math.cos(w) / a0, (1 - al) / a0)
            ns = int(rate * d)
            pitch = f0 * (1.12 - 0.25 * j / nsyl) * rnd.uniform(0.95, 1.05)
            cons = rnd.random() < 0.5
            for i in range(ns):
                t = i / ns
                ph += pitch * (1 + 0.01 * rnd.uniform(-1, 1)) / rate
                buzz = 2 * (ph % 1.0) - 1
                e = math.sin(math.pi * t) ** 0.7
                x = e * (b1(buzz) + 0.6 * b2(buzz))
                if cons and t < 0.15: x += 0.3 * rnd.uniform(-1, 1) * (1 - t / 0.15)
                phrase.append(x)
            phrase += [0.0] * int(rate * rnd.uniform(0.01, 0.07))
        out += phrase + [0.0] * int(rate * rnd.uniform(0.25, 0.6))
    # at a distance: duller, with the street's echo
    lp = Biquad("lp", 1600, 0.7, rate)
    out = [lp(x) for x in out] + [0.0] * int(rate * 0.3)
    echo = out[:]
    for dly, g in ((0.023, 0.35), (0.041, 0.25), (0.067, 0.15)):
        m = int(rate * dly)
        for i in range(m, len(out)): echo[i] += g * out[i - m]
    return fade_ends(pad(echo, rate), rate)


def variants(name, make, n):
    rate = 22050
    for k in range(n): write(f"{name}_{k + 1}", make(k), rate)


def river_shape(N, rate, rnd):
    """water going by: a smooth, faint bed, no faster change than a slow swell"""
    out = [0.0] * N
    bp = Biquad("bp", 700, 0.6, rate)
    lp = Biquad("lp", 1800, 0.7, rate)
    for i in range(N):
        t = i / rate
        x = rnd.uniform(-1, 1)
        m = 0.8 + 0.2 * math.sin(TAU * 0.05 * t)
        out[i] = m * lp(bp(x))
    return out


def noise_loop(name, secs, shape, seed=1, rate=22050):
    n, xf = int(rate * secs), int(rate * 1.5)
    rnd = random.Random(seed)
    out = shape(n + xf, rate, rnd)
    write(name, fold(out, n, xf), rate)


SOUNDS = {
    "bell": lambda: (bell(196, "bell_big"), bell(392, "bell_small", 5.0)),
    "coins": coins, "anvil": anvil, "sword": sword, "hooves": hooves, "cannon": cannon,
    # the storming of the Alte Veste: ragged musketry and the soldiers' blows
    "musket": lambda: one_shot_variants("musket", musket, 6, 22050),
    "volley": lambda: one_shot_variants("volley", volley, 4, 22050),
    "clash": lambda: one_shot_variants("clash", clash, 4, 44100),
    "knock": lambda: one_shot_variants("knock", knock, 4, 22050),
    "crickets": lambda: variants("crickets", crickets_variant, 8),
    "birds": lambda: variants("birds", birds_variant, 12),
    "waves": lambda: variants("waves", waves_variant, 7),
    "wind": lambda: variants("wind", wind_variant, 7),
    "town": lambda: variants("town", town_variant, 12),
    "river": lambda: noise_loop("river", 16.0, river_shape, 3),
}

if __name__ == "__main__":
    want = sys.argv[1:] or list(SOUNDS)
    for k in want: SOUNDS[k]()
