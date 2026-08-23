#!/usr/bin/env node
//
// Authors the ExprTK presets of the score user library
// (https://github.com/ossia/score-user-library, "default" package).
//
//   node tests/unit/tools/author-expression-presets.js \
//        ~/Documents/ossia/score/packages/default/Presets
//
// It overwrites the presets it knows about, so it is also the record of how
// they are written. After running it, regenerate the test corpus:
//
//   node tests/unit/tools/gen-expression-preset-corpus.js \
//        ~/Documents/ossia/score/packages/default/Presets \
//        tests/unit/ExpressionPresetCorpus.hpp
//
// ExprTK gotchas the expressions below have to respect:
//  * `sec`, `step`, `map`, `norm`, `lerp`, `random`, `noise` and the other
//    registered functions are reserved: `var sec := ...` does not compile.
//  * implicit multiplication does not apply after a function call or a vector
//    element, so `m3[0] (1 - k)` silently evaluates to just `m3[0]`.
//  * `clamp` takes (low, value, high).
//  * multi-argument functions are not element-wise over a vector: `clamp(-1, x, 1)`
//    on an audio bus broadcasts one value instead of clamping each channel.
const fs = require('fs');
const path = require('path');

const ROOT = process.argv[2];
if (!ROOT) { console.error('usage: mkpresets.js <presets-root>'); process.exit(1); }

// process folder -> { uuid, expr: <port index>, params: [indices for a,b,c], size: idx }
const PROC = {
  'Arraygen':                    { uuid: 'cf3df02f-a563-4e92-a739-b321d3a84252', expr: 0, size: 1 },
  'Arraymap':                    { uuid: '1fe9c806-b601-4ee0-9fbb-0ab817c4dd87', expr: 1 },
  'Micromap':                    { uuid: '25c64b87-a44a-4fed-9f60-0a48906fd3ec', expr: 1 },
  'Expression Value Filter':     { uuid: 'ae84e8b6-74ff-4259-aeeb-305d95cdfcab', expr: 1, params: [2, 3, 4] },
  'Expression Value Generator':  { uuid: 'd757bd0d-c0a1-4aec-bf72-945b722ab85b', expr: 0, params: [1, 2, 3] },
  'Expression Audio Filter':     { uuid: '13e1f4b0-1c2c-40e6-93ad-dfc91aac5335', expr: 1, params: [2, 3, 4] },
  'Expression Audio Generator':  { uuid: 'eae294b3-afeb-4fba-bbe4-337998d3748a', expr: 0, params: [1, 2, 3] },
};

const f = v => Number.isInteger(v) ? v.toFixed(1) : String(v);

function write(proc, name, expr, opts = {}) {
  const p = PROC[proc];
  if (!p) throw new Error('unknown process ' + proc);
  const items = [`[${p.expr},{"String":${JSON.stringify(expr)}}]`];
  if (p.size !== undefined) items.push(`[${p.size},{"Int":${opts.size ?? 12}}]`);
  if (p.params) {
    const abc = opts.abc ?? [0.5, 0.5, 0.5];
    p.params.forEach((idx, k) => items.push(`[${idx},{"Float":${f(abc[k])}}]`));
  }
  const cat = category(proc, name);
  const json = `{"Key":{"Uuid":"${p.uuid}","Effect":""},"Name":${JSON.stringify(name)}`
             + `,"Category":${JSON.stringify(cat)}`
             + `,"Preset":[${items.join(',')}]}`;
  const dir = path.join(ROOT, proc);
  fs.mkdirSync(dir, { recursive: true });
  fs.writeFileSync(path.join(dir, (opts.file ?? name) + '.scp'), json);
  return json;
}

// The menu category every preset is filed under. score nests these into
// submenus, '/' separated; a preset without one lands in the root of the menu,
// which with a couple of hundred presets is unusable.
//
// This table is the whole taxonomy: `write` refuses to author a preset that is
// not in it, and the pass at the end of this file stamps the category onto
// presets the script does not author itself.
const CATEGORIES = {
  'Arraygen': {
    'Circle': 'Shapes', 'Line': 'Shapes', 'Spiral': 'Shapes', 'Star': 'Shapes',
    'Grid': 'Shapes', 'Superformula': 'Shapes', 'Lissajous cloud': 'Shapes',
    'Rose 1': 'Shapes', 'Rose 2': 'Shapes', 'Rose 3': 'Shapes',
    'Epicycloid': 'Shapes', 'Epicycloid 2': 'Shapes', 'Hypo 1': 'Shapes',

    'Fibonacci sphere': '3D', 'Helix': '3D', 'Torus knot': '3D',

    'Speaker ring': 'Spatial', 'Arc': 'Spatial', 'Dome': 'Spatial',

    'Fibonacci disc': 'Sampling', 'Sample circle': 'Sampling',
    'Drifting cloud': 'Sampling', 'Perlin field': 'Sampling',
    'Random walk': 'Sampling',

    'Rotating ring': 'Animation', 'Chase': 'Animation', 'Comet': 'Animation',
    'Ripple': 'Animation', 'Bouncing row': 'Animation',
    'Chladni grid': 'Animation',

    'Rainbow': 'Colour',

    'Sine wavetable': 'Tables', 'Saw wavetable': 'Tables', 'Ramp': 'Tables',
    'Chromatic scale': 'Tables', 'Harmonic series': 'Tables',
  },

  'Arraymap': {
    'MIDI to unit': 'Levels', 'dB to gain': 'Levels', 'Gain to dB': 'Levels',
    'Gamma': 'Levels', 'Invert': 'Levels', 'Rectify': 'Levels',
    'Unit to DMX': 'Levels', 'DMX to unit': 'Levels',

    'Peak hold': 'Dynamics', 'Soft compress': 'Dynamics',
    'Threshold': 'Dynamics', 'Deadzone': 'Dynamics',
    'Deadzone (rescaled)': 'Dynamics', 'Smooth': 'Dynamics',
    'Slew limit': 'Dynamics',

    'Derivative': 'Motion', 'Motion': 'Motion',

    'Travelling wave': 'Animation', 'Staggered LFO': 'Animation',
    'Jitter': 'Animation', 'Fade by index': 'Animation',
    'Chill boi': 'Animation', 'Shaky boi': 'Animation',
    'Super shaky boi': 'Animation',

    'Quantize': 'Shaping',

    'To heat colour': 'Colour', 'sRGB to linear': 'Colour',
    'Linear to sRGB': 'Colour',

    'To ring': 'Geometry',
  },

  'Micromap': {
    'Abs': 'Basics', 'Ceil': 'Basics', 'Floor': 'Basics', 'Round': 'Basics',
    'Square': 'Basics', 'Cube': 'Basics', 'Square root': 'Basics',
    'Invert': 'Basics', 'Clamp 0-1': 'Basics', 'divide by 16': 'Basics',

    'Bipolar to unipolar': 'Ranges', 'Unipolar to bipolar': 'Ranges',
    'Wrap 0-1': 'Ranges', 'Fold -1 to 1': 'Ranges', 'Deadzone': 'Ranges',

    'Smoothstep': 'Curves', 'Perceptual fader': 'Curves',

    'Deg2Rad': 'Angles', 'Rad2Deg': 'Angles',

    'X coord': 'Arrays', 'Y coord': 'Arrays', 'Z coord': 'Arrays',
    'Sum': 'Arrays', 'Average': 'Arrays', 'Maximum': 'Arrays',
    'Minimum': 'Arrays', 'RMS': 'Arrays', 'Vector length': 'Arrays',
    'Array centroid': 'Arrays', 'Array peak index': 'Arrays',
    'Array energy': 'Arrays',

    'Smooth': 'Smoothing', 'Slew limit': 'Smoothing', 'Peak hold': 'Smoothing',
    'Derivative': 'Smoothing', 'Direction': 'Smoothing',
    'Latch non-zero': 'Smoothing', 'Speed per second': 'Smoothing',

    'Gain to dB': 'Audio analysis', 'dB to gain': 'Audio analysis',
    'Envelope to meter': 'Audio analysis', 'dbmeter_to_05': 'Audio analysis',
    'Frequency to 0-1 (log)': 'Audio analysis',

    'Frequency to MIDI pitch': 'Pitch', 'Semitones to ratio': 'Pitch',
    'Frequency to pitch class': 'Pitch', 'Frequency to octave': 'Pitch',

    'MIDI pitch to frequency': 'MIDI', 'MIDI pitch to seconds': 'MIDI',
    'midi to vst': 'MIDI', 'MIDI velocity to gain': 'MIDI',
    'MIDI CC to bipolar': 'MIDI', 'MIDI note to pitch class': 'MIDI',
    'MIDI note to octave': 'MIDI', 'MIDI channel': 'MIDI',
    'MIDI message type': 'MIDI', 'MIDI 14-bit': 'MIDI',

    'BPM to seconds': 'Time', 'BPM to Hz': 'Time',
    'BPM to milliseconds': 'Time', 'Milliseconds to Hz': 'Time',

    'Distance from centre': 'Tracking', 'Angle from centre': 'Tracking',
    'Distance between points': 'Tracking',

    'Luminance': 'Colour', 'sRGB to linear': 'Colour',
    'Linear to sRGB': 'Colour', 'Unit to 8-bit': 'Colour',
    '8-bit to unit': 'Colour',
  },

  'Expression Value Filter': {
    'One-pole smoothing': 'Smoothing', 'Slew limiter': 'Smoothing',
    'Damped spring': 'Smoothing', 'Hysteresis': 'Smoothing',
    'Exponential Average': 'Smoothing',

    'Schmitt trigger': 'Triggers', 'Toggle on press': 'Triggers',
    'Increment-on-press': 'Triggers', 'Increment-on-release': 'Triggers',
    'Step counter': 'Triggers', 'Random on trigger': 'Triggers',
    'Direction change': 'Triggers', 'Gate': 'Triggers',
    'Onset to bang': 'Triggers', 'Beat to pulse': 'Triggers',

    'Sample and Hold': 'Sample and hold', 'Sample and Hold A': 'Sample and hold',
    'Hold A': 'Sample and hold', 'Freeze': 'Sample and hold',

    'Smoothstep': 'Shaping', 'Exponential response': 'Shaping',
    'Remap to a-b': 'Shaping', 'Quantize': 'Shaping', 'Deadzone': 'Shaping',

    'Envelope follower': 'Dynamics', 'Peak with decay': 'Dynamics',

    'Quantize to major scale': 'Musical',
    'Quantize to minor scale': 'Musical',
    'Quantize to pentatonic': 'Musical',

    'Noisify': 'Random',
  },

  'Expression Value Generator': {
    'Sine LFO': 'LFO', 'Triangle LFO': 'LFO', 'Square LFO': 'LFO',
    'Saw LFO': 'LFO', 'Pulse train': 'LFO', 'Breathing': 'LFO',

    'Ramp': 'Envelopes', 'Ping-pong': 'Envelopes',
    'Exponential decay': 'Envelopes', 'Envelope': 'Envelopes',
    'Fade in-out': 'Envelopes', 'Staircase': 'Envelopes',

    'Dice': 'Random', 'Fast Noise': 'Random', 'Soft Noise': 'Random',
    'Perlin Noise': 'Random', 'Sample and hold noise': 'Random',
    'Drunk walk': 'Random', 'Smooth noise XY': 'Random',

    'Circle XY': 'Motion', 'Lissajous XY': 'Motion', 'Spiral XY': 'Motion',
    'Lorenz XYZ': 'Motion', 'De Jong XY': 'Motion', 'Logistic': 'Motion',
    'Weierstrass': 'Motion', 'Vertical scroll': 'Motion', 'XY': 'Motion',
    'XYZ': 'Motion',

    'Hue sweep RGBA': 'Colour', 'Random Color': 'Colour',
    'Color Noise': 'Colour',
  },

  'Expression Audio Filter': {
    'Soft clip': 'Distortion', 'Hard clip': 'Distortion',
    'Tube drive': 'Distortion', 'Wave folder': 'Distortion',
    'Aggressive shaping': 'Distortion', 'Cheby 2': 'Distortion',
    'Cheby 3': 'Distortion', 'Cheby 4': 'Distortion',
    'Shape A': 'Distortion', 'Shape B': 'Distortion',
    'Cubic Disto': 'Distortion', 'Harsh': 'Distortion',
    'Sin Disto': 'Distortion', 'Tan Disto': 'Distortion', 'Erf': 'Distortion',

    'Bit crusher': 'Lo-fi', 'Sample rate crusher': 'Lo-fi',
    'Discretize': 'Lo-fi',

    'One-pole lowpass': 'Filters', 'One-pole highpass': 'Filters',
    'DC blocker': 'Filters', 'Crude Lowpass': 'Filters',

    'Noise gate': 'Dynamics',

    'Tremolo': 'Modulation', 'Ring modulator': 'Modulation',
    'Auto-pan': 'Modulation',

    'Gain': 'Utility', 'Invert phase': 'Utility', 'Rectify': 'Utility',
    'Half rectify': 'Utility', 'Mono': 'Utility', 'Stereo width': 'Utility',
    'Mid-side encode': 'Utility', 'Mid-side decode': 'Utility',
    'Swap channels': 'Utility',
  },

  'Expression Audio Generator': {
    'Sine': 'Oscillators', 'Saw': 'Oscillators', 'Triangle': 'Oscillators',
    'Pulse': 'Oscillators', 'Square': 'Oscillators',
    'Low-Frequency Square': 'Oscillators', 'Detuned saws': 'Oscillators',
    'Binaural beat': 'Oscillators', 'Wobbly': 'Oscillators',

    'White noise': 'Noise', 'Stereo noise': 'Noise',

    'FM pair': 'Synthesis', 'Ring mod pair': 'Synthesis', 'Sweep': 'Synthesis',

    'Kick': 'Drums', 'Snare': 'Drums', 'Hi-hat': 'Drums',
  },
};

function category(proc, name) {
  const c = CATEGORIES[proc] && CATEGORIES[proc][name];
  if (!c)
    throw new Error(`no category for ${proc} / ${name} - add it to CATEGORIES`);
  return c;
}

// "seconds since the start of the score" - t is expressed in flicks.
const SEC = 'var tsec := t / 705600000;\n';

/* ===================================================================== */
/* Repairs to existing presets                                            */
/* ===================================================================== */

// i/(n-1) was 0 for the first element, making b == 0 and (a-b)/b == 0/0.
// (1+i)/n is always > 0 and also survives n == 1.
write('Arraygen', 'Hypo 1',
`var u := (1 + i) / n;
var a := 0.5 u;
var b := -0.5 u;
var p := 0.000000001 t + (1+i);
var vx := (a - b) cos(p) + b cos (p (a-b) / b);
var vy := (a - b) sin(p) + b sin(p (a-b) / b);
return [vx, vy]`, { size: 278 });

// x ^ (b x) is NaN for every negative sample. Shape the magnitude and put the
// sign back, which keeps the odd symmetry a waveshaper needs.
write('Expression Audio Filter', 'Aggressive shaping',
`var d := 0.2 + 3 b;
var m := abs(x) ^ d;
out := (0.3 + 0.7 c) sgn(x) sin((0.5 + 30 a) m);`, { abc: [0.7, 1.0, 0.25] });

// sqrt of a negative input is NaN, and a mapping is fed whatever is patched
// into it.
write('Micromap', 'Square root', `sqrt(max(0, x))`);

// Two bugs: clamp() takes (low, value, high) so the original clamped `l` rather
// than `x` and never limited the top end, and a == b divided by zero.
write('Expression Value Filter', 'Smoothstep',
`// a and b are the two ends of the ramp.
var l := min(a, b);
var r := max(a, b);
var v := clamp(0, (x - l) / max(0.000001, r - l), 1);
v v (3 - 2 v)`, { abc: [0.0, 1.0, 0.5] });

// a == 0 gave 0 / 0 on every sample.
write('Expression Audio Filter', 'Discretize',
`// a: number of levels.
var s := 1 + 60 a;
out := round(s x) / s`, { abc: [0.5, 0.0, 0.0] });

// The Chebyshev shapers fed (100 a x) straight into a polynomial: with a normal
// signal that is a gain of up to 100 before being raised to the 4th power, so
// they put out five- to nine-digit samples. Chebyshev polynomials are only
// bounded on -1..1, so limit the drive into that range first.
const cheby = body =>
`// Chebyshev waveshaper. a: drive, c: output level.
var v := tanh((0.2 + 8 a) x);
out := (0.2 + 0.8 c) (${body})`;

write('Expression Audio Filter', 'Cheby 2', cheby('2 v v - 1'),
      { abc: [0.168, 1.0, 0.25] });
write('Expression Audio Filter', 'Cheby 3', cheby('v (4 v v - 3)'),
      { abc: [0.735, 1.0, 0.25] });
write('Expression Audio Filter', 'Cheby 4', cheby('8 v v v v - 8 v v + 1'),
      { abc: [0.118, 1.0, 0.25] });
write('Expression Audio Filter', 'Shape B', cheby('1.5 v - 0.5 v v v'),
      { abc: [0.8, 1.0, 0.25] });

// These five wrote out[1] unconditionally, so they could not be compiled at all
// against a mono bus - and did nothing on a bus with more than two channels.
// Looping over x[] makes them work on any channel count.
const per_channel = body =>
`for(var i := 0; i < x[]; i += 1) {
  ${body}
}`;

write('Expression Audio Filter', 'Crude Lowpass',
      per_channel('out[i] := clamp(-1, (1 - 2 a) x[i] + 2 a px[i], 1);'),
      { abc: [0.25, 0.5, 0.5] });
write('Expression Audio Filter', 'Cubic Disto',
      per_channel('out[i] := clamp(-1, a x[i] x[i] x[i], 1);'),
      { abc: [0.633, 0.5, 0.5] });
write('Expression Audio Filter', 'Harsh',
      per_channel('out[i] := clamp(-1, tan(x[i] log(1 + 200 a)), 1);'),
      { abc: [0.85, 0.5, 0.5], file: 'Harsh Disto' });
write('Expression Audio Filter', 'Sin Disto',
      per_channel('out[i] := clamp(-1, sin(x[i] a), 1);'),
      { abc: [0.533, 0.5, 0.5] });
write('Expression Audio Filter', 'Tan Disto',
      per_channel('out[i] := clamp(-1, tan(x[i] a), 1);'),
      { abc: [1.0, 0.5, 0.5] });

/* ===================================================================== */
/* Arraygen - point clouds, LED strips, spatial layouts                   */
/* ===================================================================== */

write('Arraygen', 'Circle',
`var a := 2 pi i / n;
return [cos(a), sin(a)]`, { size: 64 });

write('Arraygen', 'Rotating ring',
`${SEC}var a := 2 pi i / n + 0.5 tsec;
return [cos(a), sin(a)]`, { size: 64 });

write('Arraygen', 'Line',
`var u := i / max(1, n - 1);
return [2 u - 1, 0]`, { size: 32 });

write('Arraygen', 'Spiral',
`var u := i / max(1, n - 1);
var a := 8 pi u;
return [u cos(a), u sin(a)]`, { size: 256 });

write('Arraygen', 'Fibonacci disc',
`// Golden-angle phyllotaxis: the most even way to spread n points on a disc.
var a := i * 2.39996322972865332;
var r := sqrt((i + 0.5) / n);
return [r cos(a), r sin(a)]`, { size: 512 });

write('Arraygen', 'Fibonacci sphere',
`// Evenly distributed points on the unit sphere.
var y := 1 - 2 (i + 0.5) / n;
var r := sqrt(max(0, 1 - y y));
var a := i * 2.39996322972865332;
return [r cos(a), y, r sin(a)]`, { size: 512 });

write('Arraygen', 'Helix',
`var u := i / max(1, n - 1);
var a := 6 pi u;
return [cos(a), 2 u - 1, sin(a)]`, { size: 256 });

write('Arraygen', 'Grid',
`var side := max(1, ceil(sqrt(n)));
var cx := i % side;
var cy := trunc(i / side);
var d := max(1, side - 1);
return [2 cx / d - 1, 2 cy / d - 1]`, { size: 256 });

write('Arraygen', 'Star',
`var a := 2 pi i / n;
var r := if(i % 2 == 0, 1, 0.45);
return [r cos(a), r sin(a)]`, { size: 24 });

write('Arraygen', 'Superformula',
`var a := 2 pi i / n;
var m := 7;
var e1 := 0.3;
var e2 := 1.7;
var base := max(0.000001, abs(cos(m a / 4)) ^ e2 + abs(sin(m a / 4)) ^ e2);
var r := base ^ (-1 / (3 e1));
return [r cos(a), r sin(a)]`, { size: 512 });

write('Arraygen', 'Torus knot',
`var u := 2 pi i / n;
var P := 2;
var Q := 3;
var r := 2 + cos(Q u);
return [0.3 r cos(P u), 0.3 r sin(P u), 0.3 sin(Q u)]`, { size: 512 });

write('Arraygen', 'Lissajous cloud',
`${SEC}var u := 2 pi i / n;
return [sin(3 u + 0.3 tsec), sin(4 u)]`, { size: 256 });

write('Arraygen', 'Rainbow',
`// HSV hue sweep along the array: one RGB triplet per LED.
${SEC}var h := 6 frac(i / n + 0.1 tsec);
return [
  clamp(0, abs(h - 3) - 1, 1),
  clamp(0, 2 - abs(h - 2), 1),
  clamp(0, 2 - abs(h - 4), 1)
]`, { size: 60 });

write('Arraygen', 'Chase',
`// A gaussian bump running along the strip.
${SEC}var u := i / max(1, n - 1);
var head := frac(0.3 tsec);
var d := u - head;
exp(-0.5 (d / 0.08) ^ 2)`, { size: 60 });

write('Arraygen', 'Comet',
`// Bright head, exponential tail, wraps around the strip.
${SEC}var u := i / max(1, n - 1);
var head := frac(0.4 tsec);
exp(-8 frac(head - u))`, { size: 60 });

write('Arraygen', 'Ripple',
`${SEC}var u := i / max(1, n - 1);
return [2 u - 1, 0.5 sin(10 u - 4 tsec)]`, { size: 128 });

write('Arraygen', 'Bouncing row',
`${SEC}var u := i / max(1, n - 1);
var ph := frac(0.4 tsec + u);
return [2 u - 1, abs(sin(pi ph))]`, { size: 32 });

write('Arraygen', 'Chladni grid',
`// Standing-wave figure sampled on a square grid: one amplitude per cell.
var side := max(1, ceil(sqrt(n)));
var d := max(1, side - 1);
var cx := (i % side) / d;
var cy := trunc(i / side) / d;
var M := 3;
var K := 5;
cos(M pi cx) cos(K pi cy) - cos(K pi cx) cos(M pi cy)`, { size: 256 });

write('Arraygen', 'Perlin field',
`${SEC}noise(0.15 i + 0.5 tsec, 3, 0.5)`, { size: 64 });

write('Arraygen', 'Drifting cloud',
`${SEC}return [
  2 noise(10 i + 0.3 tsec, 2, 0.5) - 1,
  2 noise(10 i + 1000 + 0.3 tsec, 2, 0.5) - 1
]`, { size: 128 });

write('Arraygen', 'Random walk',
`// Each element keeps its own value between ticks (po) and wanders.
clamp(0, po + random(-0.03, 0.03), 1)`, { size: 32 });

write('Arraygen', 'Sine wavetable',
`sin(2 pi i / n)`, { size: 256 });

write('Arraygen', 'Saw wavetable',
`2 i / n - 1`, { size: 256 });

write('Arraygen', 'Ramp',
`i / max(1, n - 1)`, { size: 32 });

write('Arraygen', 'Chromatic scale',
`// One frequency per element, centred on A440.
440 * 2 ^ ((i - trunc(n / 2)) / 12)`, { size: 24 });

write('Arraygen', 'Harmonic series',
`1 / (1 + i)`, { size: 16 });

/* ===================================================================== */
/* Arraymap - per-element processing of an incoming array                 */
/* ===================================================================== */

write('Arraymap', 'Invert', `1 - x`);
write('Arraymap', 'Rectify', `abs(x)`);
write('Arraymap', 'MIDI to unit', `x / 127`);
write('Arraymap', 'Quantize', `round(8 x) / 8`);
write('Arraymap', 'Gamma', `// Perceptual brightness curve for LEDs.\nsgn(x) abs(x) ^ 2.2`);
write('Arraymap', 'dB to gain', `10 ^ (x / 20)`);
write('Arraymap', 'Gain to dB', `20 log10(max(0.000001, abs(x)))`);
write('Arraymap', 'Smooth', `// One-pole per element: po is that element's own previous output.\npo + 0.15 (x - po)`);
write('Arraymap', 'Slew limit', `po + clamp(-0.05, x - po, 0.05)`);
write('Arraymap', 'Peak hold', `// Per-band VU meter with a decaying peak.\nmax(abs(x), 0.95 po)`);
write('Arraymap', 'Derivative', `x - px`);
write('Arraymap', 'Motion', `abs(x - px)`);
write('Arraymap', 'Deadzone', `if(abs(x) < 0.05, 0, x)`);
write('Arraymap', 'Threshold', `if(x > 0.5, 1, 0)`);
write('Arraymap', 'Jitter', `x + random(-0.02, 0.02)`);
write('Arraymap', 'Soft compress', `sgn(x) abs(x) / (1 + abs(x))`);
write('Arraymap', 'Fade by index', `x (1 - i / max(1, n - 1))`);
write('Arraymap', 'Travelling wave',
`${SEC}x (0.5 + 0.5 sin(6 i / n - 4 tsec))`);
write('Arraymap', 'Staggered LFO',
`${SEC}x + 0.2 sin(2 pi (0.5 tsec + i / n))`);
write('Arraymap', 'To ring',
`// Turn a 1-D array into a radial 2-D shape: value drives the radius.
var a := 2 pi i / n;
return [x cos(a), x sin(a)]`);
write('Arraymap', 'To heat colour',
`// Value to red-yellow-white, one colour per element.
var u := clamp(0, x, 1);
return [clamp(0, 3 u, 1), clamp(0, 3 u - 1, 1), clamp(0, 3 u - 2, 1)]`);

/* ===================================================================== */
/* Micromap - single-value conversions                                    */
/* ===================================================================== */

write('Micromap', 'Clamp 0-1', `clamp(0, x, 1)`);
write('Micromap', 'Invert', `1 - x`);
write('Micromap', 'Bipolar to unipolar', `0.5 (x + 1)`);
write('Micromap', 'Unipolar to bipolar', `2 x - 1`);
write('Micromap', 'Wrap 0-1', `frac(x)`);
write('Micromap', 'Fold -1 to 1', `// Triangle fold: values outside -1..1 reflect back in.\n2 asin(sin(pi x / 2)) / pi`);
write('Micromap', 'Smoothstep', `var u := clamp(0, x, 1);\nu u (3 - 2 u)`);
write('Micromap', 'Perceptual fader', `var u := clamp(0, x, 1);\n(exp(3 u) - 1) / (exp(3) - 1)`);
write('Micromap', 'Frequency to MIDI pitch', `69 + 12 log2(max(0.000001, x) / 440)`);
write('Micromap', 'Semitones to ratio', `2 ^ (x / 12)`);
write('Micromap', 'BPM to seconds', `60 / max(0.000001, x)`);
write('Micromap', 'BPM to Hz', `x / 60`);
write('Micromap', 'Milliseconds to Hz', `1000 / max(0.000001, x)`);
write('Micromap', 'Smooth', `po + 0.1 (x - po)`);
write('Micromap', 'Slew limit', `po + clamp(-0.02, x - po, 0.02)`);
write('Micromap', 'Peak hold', `max(abs(x), 0.99 po)`);
write('Micromap', 'Direction', `sgn(x - px)`);
write('Micromap', 'Speed per second',
`// Rate of change in units per second - dt is in flicks.
(x - px) * 705600000 / max(1, dt)`);
write('Micromap', 'Latch non-zero', `if(x != 0, x, po)`);
write('Micromap', 'Vector length',
`var s := 0;
for(var k := 0; k < xv[]; k += 1) { s += xv[k] xv[k]; };
sqrt(s)`);
write('Micromap', 'Sum', `sum(xv)`);
write('Micromap', 'Average', `avg(xv)`);
write('Micromap', 'Maximum', `max(xv)`);
write('Micromap', 'Minimum', `min(xv)`);
write('Micromap', 'RMS',
`var s := 0;
for(var k := 0; k < xv[]; k += 1) { s += xv[k] xv[k]; };
sqrt(s / max(1, xv[]))`);

/* ===================================================================== */
/* Expression Value Filter                                                */
/* ===================================================================== */

write('Expression Value Filter', 'One-pole smoothing',
`// a: smoothing amount (small = slow).
m1 := m1 + (0.001 + a) (x - m1)`, { abc: [0.1, 0.5, 0.5] });

write('Expression Value Filter', 'Slew limiter',
`// a: maximum change per incoming value.
var r := 0.001 + a;
m1 := m1 + clamp(-r, x - m1, r)`, { abc: [0.05, 0.5, 0.5] });

write('Expression Value Filter', 'Schmitt trigger',
`// a: falling threshold, b: rising threshold.
m1 := if(x > b, 1, if(x < a, 0, m1))`, { abc: [0.4, 0.6, 0.5] });

write('Expression Value Filter', 'Toggle on press',
`m1 := if(x > 0.5 and px <= 0.5, 1 - m1, m1)`);

write('Expression Value Filter', 'Envelope follower',
`// a: attack, b: release.
var at := 0.001 + a;
var rl := 0.001 + b;
var v := abs(x);
m1 := m1 + if(v > m1, at, rl) * (v - m1)`, { abc: [0.5, 0.02, 0.5] });

write('Expression Value Filter', 'Peak with decay',
`// a: how fast the peak falls back.
m1 := max(abs(x), m1 (1 - 0.001 - 0.2 a))`, { abc: [0.1, 0.5, 0.5] });

write('Expression Value Filter', 'Quantize',
`// a: number of steps.
var s := max(1, round(1 + 32 a));
round(x s) / s`, { abc: [0.25, 0.5, 0.5] });

write('Expression Value Filter', 'Step counter',
`// Counts rising edges, wrapping at a steps.
var m := max(1, round(1 + 15 a));
m1 := if(x > 0.5 and px <= 0.5, mod(m1 + 1, m), m1)`, { abc: [0.2, 0.5, 0.5] });

write('Expression Value Filter', 'Random on trigger',
`// New random value in a..b on every rising edge.
m1 := if(x > 0.5 and px <= 0.5, random(a, b), m1)`, { abc: [0.0, 1.0, 0.5] });

write('Expression Value Filter', 'Freeze',
`// c above 0.5 holds the last value.
m1 := if(c > 0.5, m1, x)`, { abc: [0.5, 0.5, 0.0] });

write('Expression Value Filter', 'Exponential response',
`// a: how much the curve bends.
var k := 0.1 + 8 a;
(exp(k clamp(0, x, 1)) - 1) / (exp(k) - 1)`, { abc: [0.4, 0.5, 0.5] });

write('Expression Value Filter', 'Remap to a-b',
`map(x, 0, 1, a, b)`, { abc: [0.0, 1.0, 0.5] });

write('Expression Value Filter', 'Damped spring',
`// a: stiffness, b: damping. Overshoots and settles on the input.
var k := 0.001 + 0.2 a;
var d := 0.5 + 0.49 b;
m2 := d (m2 + k (x - m1));
m1 := m1 + m2;
m1`, { abc: [0.3, 0.5, 0.5] });

write('Expression Value Filter', 'Hysteresis',
`// Ignores changes smaller than a.
m1 := if(abs(x - m1) > 0.001 + a, x, m1)`, { abc: [0.05, 0.5, 0.5] });

write('Expression Value Filter', 'Quantize to major scale',
`// 0..1 in, MIDI note out.
var sc[7] := {0, 2, 4, 5, 7, 9, 11};
var d := round(21 clamp(0, x, 1));
var o := trunc(d / 7);
36 + 12 o + sc[d % 7]`);

write('Expression Value Filter', 'Gate',
`// Passes the value only above threshold a.
if(abs(x) > a, x, 0)`, { abc: [0.2, 0.5, 0.5] });

/* ===================================================================== */
/* Expression Value Generator                                            */
/* ===================================================================== */

write('Expression Value Generator', 'Ramp', `pos`);
write('Expression Value Generator', 'Ping-pong', `1 - abs(2 pos - 1)`);

write('Expression Value Generator', 'Sine LFO',
`${SEC}var f := 0.05 + 5 a;
0.5 + 0.5 sin(2 pi f tsec)`, { abc: [0.1, 0.5, 0.5] });

write('Expression Value Generator', 'Triangle LFO',
`${SEC}var ph := frac((0.05 + 5 a) tsec);
1 - abs(2 ph - 1)`, { abc: [0.1, 0.5, 0.5] });

write('Expression Value Generator', 'Square LFO',
`// a: rate, b: pulse width.
${SEC}var ph := frac((0.05 + 5 a) tsec);
if(ph < 0.02 + 0.96 b, 1, 0)`, { abc: [0.1, 0.5, 0.5] });

write('Expression Value Generator', 'Saw LFO',
`${SEC}frac((0.05 + 5 a) tsec)`, { abc: [0.1, 0.5, 0.5] });

write('Expression Value Generator', 'Breathing',
`${SEC}var s := 0.5 + 0.5 sin(2 pi (0.05 + a) tsec);
s s (3 - 2 s)`, { abc: [0.1, 0.5, 0.5] });

write('Expression Value Generator', 'Pulse train',
`${SEC}var ph := frac((0.1 + 5 a) tsec);
if(ph < 0.05, 1, 0)`, { abc: [0.2, 0.5, 0.5] });

write('Expression Value Generator', 'Exponential decay',
`exp(-(0.1 + 20 a) pos)`, { abc: [0.2, 0.5, 0.5] });

write('Expression Value Generator', 'Envelope',
`// Attack / sustain / release across the interval. a: attack, b: release, c: level.
var A := 0.02 + 0.3 a;
var R := 0.02 + 0.5 b;
var s := 0.2 + 0.7 c;
var up := clamp(0, pos / A, 1);
var down := clamp(0, (1 - pos) / R, 1);
s min(up, down)`, { abc: [0.2, 0.2, 0.8] });

write('Expression Value Generator', 'Fade in-out',
`var f := 0.01 + 0.4 a;
clamp(0, min(pos / f, (1 - pos) / f), 1)`, { abc: [0.2, 0.5, 0.5] });

write('Expression Value Generator', 'Staircase',
`var s := max(1, round(1 + 16 a));
trunc(pos s) / s`, { abc: [0.4, 0.5, 0.5] });

write('Expression Value Generator', 'Sample and hold noise',
`// a: how often a new value is drawn.
${SEC}var k := trunc(tsec (0.05 + 10 a));
if(k != m1) { m1 := k; m2 := random(0, 1); };
m2`, { abc: [0.3, 0.5, 0.5] });

write('Expression Value Generator', 'Drunk walk',
`// a: step size.
m1 := clamp(0, m1 + (0.001 + 0.1 a) random(-1, 1), 1)`, { abc: [0.2, 0.5, 0.5] });

write('Expression Value Generator', 'Smooth noise XY',
`${SEC}var f := 0.05 + 2 a;
return [2 noise(f tsec, 3, 0.5) - 1, 2 noise(1000 + f tsec, 3, 0.5) - 1]`, { abc: [0.2, 0.5, 0.5] });

write('Expression Value Generator', 'Circle XY',
`${SEC}var w := 2 pi (0.02 + 2 a) tsec;
var r := 0.1 + b;
return [r cos(w), r sin(w)]`, { abc: [0.1, 0.9, 0.5] });

write('Expression Value Generator', 'Lissajous XY',
`${SEC}var f := 0.1 + 2 a;
return [sin(2 pi f tsec), sin(2 pi f (1 + 0.5 b) tsec + 1)]`, { abc: [0.2, 0.6, 0.5] });

write('Expression Value Generator', 'Spiral XY',
`var u := pos;
var w := 8 pi u;
return [u cos(w), u sin(w)]`);

write('Expression Value Generator', 'Hue sweep RGBA',
`// a: how many times the hue goes round, b: starting hue.
var h := 6 frac(pos (0.5 + 4 a) + b);
return [
  clamp(0, abs(h - 3) - 1, 1),
  clamp(0, 2 - abs(h - 2), 1),
  clamp(0, 2 - abs(h - 4), 1),
  1
]`, { abc: [0.25, 0.0, 0.5] });

write('Expression Value Generator', 'Lorenz XYZ',
`// The classic strange attractor. a: speed.
if(m1 == 0 and m2 == 0 and m3 == 0) { m1 := 0.1; m3 := 20; };
var h := 0.0005 + 0.008 a;
var dx := 10 (m2 - m1);
var dy := m1 (28 - m3) - m2;
var dz := m1 m2 - 2.6666666 m3;
m1 := clamp(-100, m1 + h dx, 100);
m2 := clamp(-100, m2 + h dy, 100);
m3 := clamp(-100, m3 + h dz, 100);
return [m1 / 30, m2 / 30, (m3 - 25) / 30]`, { abc: [0.4, 0.5, 0.5] });

write('Expression Value Generator', 'De Jong XY',
`// Bounded strange attractor - a, b, c reshape it.
var A := -2 + 4 a;
var B := -2 + 4 b;
var C := -2 + 4 c;
var nx := sin(A m2) - cos(B m1);
var ny := sin(C m1) - cos(2 m2);
m1 := nx;
m2 := ny;
return [0.5 m1, 0.5 m2]`, { abc: [0.15, 0.85, 0.3] });

/* ===================================================================== */
/* Expression Audio Generator                                            */
/* ===================================================================== */

write('Expression Audio Generator', 'Saw',
`// a: pitch, b: level.
var f := 20 + 2000 a;
m1[0] := frac(m1[0] + f / fs);
var s := b (2 m1[0] - 1);
out[0] := s;
out[1] := s;`, { abc: [0.1, 0.3, 0.5] });

write('Expression Audio Generator', 'Triangle',
`var f := 20 + 2000 a;
m1[0] := frac(m1[0] + f / fs);
var s := b (4 abs(m1[0] - 0.5) - 1);
out[0] := s;
out[1] := s;`, { abc: [0.1, 0.4, 0.5] });

write('Expression Audio Generator', 'Pulse',
`// a: pitch, b: level, c: pulse width.
var f := 20 + 2000 a;
m1[0] := frac(m1[0] + f / fs);
var s := if(m1[0] < 0.02 + 0.96 c, b, -b);
out[0] := s;
out[1] := s;`, { abc: [0.1, 0.3, 0.5] });

write('Expression Audio Generator', 'White noise',
`var s := b random(-1, 1);
out[0] := s;
out[1] := s;`, { abc: [0.5, 0.3, 0.5] });

write('Expression Audio Generator', 'Stereo noise',
`out[0] := b random(-1, 1);
out[1] := b random(-1, 1);`, { abc: [0.5, 0.3, 0.5] });

write('Expression Audio Generator', 'FM pair',
`// a: pitch, b: modulator ratio, c: modulation depth.
var fc := 40 + 800 a;
var ratio := 1 + round(7 b);
m1[0] := frac(m1[0] + fc / fs);
m1[1] := frac(m1[1] + fc ratio / fs);
var s := 0.4 sin(2 pi m1[0] + 8 c sin(2 pi m1[1]));
out[0] := s;
out[1] := s;`, { abc: [0.2, 0.15, 0.3] });

write('Expression Audio Generator', 'Ring mod pair',
`var f1 := 40 + 800 a;
var f2 := 40 + 800 b;
m1[0] := frac(m1[0] + f1 / fs);
m1[1] := frac(m1[1] + f2 / fs);
var s := 0.4 sin(2 pi m1[0]) sin(2 pi m1[1]);
out[0] := s;
out[1] := s;`, { abc: [0.2, 0.35, 0.5] });

write('Expression Audio Generator', 'Detuned saws',
`// b: detune amount.
var f := 40 + 400 a;
m1[0] := frac(m1[0] + f / fs);
m1[1] := frac(m1[1] + f (1 + 0.02 b) / fs);
var s := 0.2 ((2 m1[0] - 1) + (2 m1[1] - 1));
out[0] := s;
out[1] := s;`, { abc: [0.2, 0.4, 0.5] });

write('Expression Audio Generator', 'Binaural beat',
`// a: pitch, b: beat frequency between the two ears.
var f := 100 + 400 a;
m1[0] := frac(m1[0] + f / fs);
m1[1] := frac(m1[1] + (f + 0.5 + 20 b) / fs);
out[0] := 0.3 sin(2 pi m1[0]);
out[1] := 0.3 sin(2 pi m1[1]);`, { abc: [0.1, 0.3, 0.5] });

write('Expression Audio Generator', 'Sweep',
`// Repeating exponential chirp. a: low, b: high, c: sweep rate.
var f0 := 40 + 200 a;
var f1 := 400 + 4000 b;
m2[0] := frac(m2[0] + (0.05 + c) / fs);
m1[0] := frac(m1[0] + f0 (f1 / f0) ^ m2[0] / fs);
var s := 0.3 sin(2 pi m1[0]);
out[0] := s;
out[1] := s;`, { abc: [0.2, 0.4, 0.2] });

write('Expression Audio Generator', 'Kick',
`// a: tempo, b: decay, c: level.
if(m2[0] == 0 and m3[0] == 0) { m2[0] := 1; };
m2[0] += (0.2 + 4 a) / fs;
if(m2[0] >= 1) { m2[0] := m2[0] - 1; m3[0] := 1; };
m3[0] := m3[0] * (1 - (2 + 30 b) / fs);
m1[0] := frac(m1[0] + (40 + 400 m3[0] m3[0]) / fs);
var s := c m3[0] sin(2 pi m1[0]);
out[0] := s;
out[1] := s;`, { abc: [0.15, 0.3, 0.8] });

/* ===================================================================== */
/* Expression Audio Filter                                                */
/* ===================================================================== */

write('Expression Audio Filter', 'Soft clip',
`// a: drive. Stays inside -1..1 whatever you feed it.
var d := 1 + 30 a;
out := tanh(d x)`, { abc: [0.3, 0.5, 0.5] });

write('Expression Audio Filter', 'Hard clip',
`// a: drive, b: ceiling.
var d := 1 + 10 a;
var lim := 0.05 + 0.95 b;
for(var i := 0; i < x[]; i += 1) {
  var v := d x[i];
  out[i] := if(v > lim, lim, if(v < -lim, -lim, v));
}`, { abc: [0.3, 0.8, 0.5] });

write('Expression Audio Filter', 'Tube drive',
`// Asymmetric saturation. a: drive, b: bias (adds even harmonics).
var d := 1 + 20 a;
out := tanh(d x + 0.6 b) - tanh(0.6 b)`, { abc: [0.3, 0.3, 0.5] });

write('Expression Audio Filter', 'Wave folder',
`// a: fold amount.
var d := 1 + 8 a;
out := 0.63661977 asin(sin(1.57079633 d x))`, { abc: [0.3, 0.5, 0.5] });

write('Expression Audio Filter', 'Bit crusher',
`// a: bit depth (fewer levels to the right).
var lv := max(2, round(66 - 62 a));
for(var i := 0; i < x[]; i += 1) {
  out[i] := round(lv x[i]) / lv;
}`, { abc: [0.5, 0.5, 0.5] });

write('Expression Audio Filter', 'Sample rate crusher',
`// a: how many input samples to hold.
var hold := max(1, round(1 + 100 a));
m2[0] += 1;
if(m2[0] >= hold) {
  m2[0] := 0;
  for(var i := 0; i < x[]; i += 1) { m1[i] := x[i]; };
};
for(var i := 0; i < x[]; i += 1) { out[i] := m1[i]; }`, { abc: [0.2, 0.5, 0.5] });

write('Expression Audio Filter', 'One-pole lowpass',
`// a: cutoff (right = brighter).
var k := 0.0005 + 0.5 a;
for(var i := 0; i < x[]; i += 1) {
  m1[i] := m1[i] + k (x[i] - m1[i]);
  out[i] := m1[i];
}`, { abc: [0.1, 0.5, 0.5] });

write('Expression Audio Filter', 'One-pole highpass',
`var k := 0.0005 + 0.5 a;
for(var i := 0; i < x[]; i += 1) {
  m1[i] := m1[i] + k (x[i] - m1[i]);
  out[i] := x[i] - m1[i];
}`, { abc: [0.1, 0.5, 0.5] });

write('Expression Audio Filter', 'DC blocker',
`for(var i := 0; i < x[]; i += 1) {
  var y := x[i] - m1[i] + 0.995 m2[i];
  m1[i] := x[i];
  m2[i] := y;
  out[i] := y;
}`);

write('Expression Audio Filter', 'Gain',
`// -60 dB at the left, +12 dB at the right.
out := x * 10 ^ ((-60 + 72 a) / 20)`, { abc: [0.83, 0.5, 0.5] });

write('Expression Audio Filter', 'Invert phase', `out := -x`);
write('Expression Audio Filter', 'Rectify', `out := abs(x)`);
write('Expression Audio Filter', 'Half rectify', `out := 0.5 (x + abs(x))`);

write('Expression Audio Filter', 'Mono',
`var s := avg(x);
for(var i := 0; i < x[]; i += 1) { out[i] := s; }`);

write('Expression Audio Filter', 'Tremolo',
`// a: rate, b: depth.
m1[0] := frac(m1[0] + (0.1 + 20 a) / fs);
out := x (1 - b (0.5 - 0.5 cos(2 pi m1[0])))`, { abc: [0.2, 0.7, 0.5] });

write('Expression Audio Filter', 'Ring modulator',
`// a: modulator frequency, b: dry/wet.
m1[0] := frac(m1[0] + (10 + 2000 a) / fs);
out := x (1 - b + b sin(2 pi m1[0]))`, { abc: [0.1, 0.8, 0.5] });

write('Expression Audio Filter', 'Auto-pan',
`// a: rate, b: width. Even channels go one way, odd channels the other.
m1[0] := frac(m1[0] + (0.05 + 5 a) / fs);
var p := 0.5 + 0.5 b sin(2 pi m1[0]);
for(var i := 0; i < x[]; i += 1) {
  out[i] := x[i] * sqrt(if(i % 2 == 0, 1 - p, p));
}`, { abc: [0.1, 0.8, 0.5] });

write('Expression Audio Filter', 'Noise gate',
`// a: threshold.
var th := 0.001 + 0.3 a;
for(var i := 0; i < x[]; i += 1) {
  var v := abs(x[i]);
  m1[i] := m1[i] + if(v > m1[i], 0.01, 0.0005) * (v - m1[i]);
  out[i] := if(m1[i] > th, x[i], 0);
}`, { abc: [0.05, 0.5, 0.5] });

write('Expression Audio Filter', 'Stereo width',
`// a: width. 0 = mono, 1 = normal, right = wider.
var w := 3 a;
for(var i := 0; i < x[]; i += 1) { out[i] := x[i]; };
for(var i := 0; i + 1 < x[]; i += 2) {
  var mid := 0.5 (x[i] + x[i+1]);
  var side := 0.5 w (x[i] - x[i+1]);
  out[i] := mid + side;
  out[i+1] := mid - side;
}`, { abc: [0.333, 0.5, 0.5] });

/* ===================================================================== */
/* Utilities for the output of the rest of the tree                       */
/* ===================================================================== */

// --- Analysis/Envelope, Analysis/Spectrum, Analysis/Pitch ---------------
// Envelope Follower, RMS, Peak, Centroid, Rolloff and Pitch detector all emit
// bare floats: linear amplitudes and frequencies in Hz, neither of which maps
// onto a 0..1 control the way a listener hears them.

write('Micromap', 'Gain to dB', `20 log10(max(0.000001, abs(x)))`);
write('Micromap', 'dB to gain', `10 ^ (x / 20)`);

write('Micromap', 'Envelope to meter',
`// Linear amplitude to 0..1 over the last 60 dB, the way a meter reads.
clamp(0, 1 + log10(max(0.000001, abs(x))) / 3, 1)`);

write('Micromap', 'Frequency to 0-1 (log)',
`// 20 Hz .. 20 kHz onto 0..1, logarithmically: for Centroid and Rolloff.
clamp(0, log2(clamp(20, x, 20000) / 20) / 9.9658, 1)`);

write('Micromap', 'Frequency to pitch class',
`// Hz to 0 = C, 1 = C#, ... 11 = B.
var n0 := round(69 + 12 log2(max(0.000001, x) / 440));
mod(mod(n0, 12) + 12, 12)`);

write('Micromap', 'Frequency to octave',
`// Hz to the octave number the note belongs to (A440 is in octave 4).
trunc((69 + 12 log2(max(0.000001, x) / 440)) / 12) - 1`);

// --- Midi / MIDI to array -----------------------------------------------
write('Micromap', 'MIDI velocity to gain',
`// Velocity feels linear only once it is squared.
(clamp(0, x, 127) / 127) ^ 2`);

write('Micromap', 'MIDI CC to bipolar',
`// 0..127 with 64 as the centre, onto -1..1.
(clamp(0, x, 127) - 64) / 63`);

write('Micromap', 'MIDI note to pitch class',
`var n0 := round(x);
mod(mod(n0, 12) + 12, 12)`);

write('Micromap', 'MIDI note to octave', `trunc(round(x) / 12) - 1`);

write('Micromap', 'MIDI channel',
`// From a "MIDI to array" message: 1..16.
mod(xv[0], 16) + 1`);

write('Micromap', 'MIDI message type',
`// From a "MIDI to array" message: 8 = note off, 9 = note on, 11 = CC,
// 12 = program change, 14 = pitch bend.
trunc(xv[0] / 16)`);

write('Micromap', 'MIDI 14-bit',
`// An [MSB, LSB] pair - pitch bend, high-resolution CC - to 0..16383.
128 xv[0] + xv[1]`);

// --- Visuals/Computer Vision, AI/Computer Vision, Spatial/Tracking ------
// Blob stats, the pose detectors, Point Tracker and the TUIO / PSN / RTTrP
// protocols all speak normalized 0..1 screen coordinates.

write('Micromap', 'Distance from centre',
`// For the 0..1 coordinates the computer-vision objects emit.
var dx := xv[0] - 0.5;
var dy := xv[1] - 0.5;
sqrt(dx dx + dy dy)`);

write('Micromap', 'Angle from centre',
`// Degrees, 0 to the right, counter-clockwise.
rad2deg(atan2(xv[1] - 0.5, xv[0] - 0.5))`);

write('Micromap', 'Distance between points',
`// Input: [x1, y1, x2, y2] - two landmarks, two blobs, two cursors.
var dx := xv[2] - xv[0];
var dy := xv[3] - xv[1];
sqrt(dx dx + dy dy)`);

// --- Visuals, Led: colour -----------------------------------------------
write('Micromap', 'Luminance',
`// Rec. 709 luma of an RGB input.
0.2126 xv[0] + 0.7152 xv[1] + 0.0722 xv[2]`);

write('Micromap', 'sRGB to linear',
`var u := clamp(0, x, 1);
if(u <= 0.04045, u / 12.92, ((u + 0.055) / 1.055) ^ 2.4)`);

write('Micromap', 'Linear to sRGB',
`var u := clamp(0, x, 1);
if(u <= 0.0031308, 12.92 u, 1.055 * (u ^ (1 / 2.4)) - 0.055)`);

write('Micromap', 'Unit to 8-bit', `round(255 clamp(0, x, 1))`);
write('Micromap', '8-bit to unit', `clamp(0, x, 255) / 255`);

// --- Arrays: MFCC, Spectrum, and any list-valued output -----------------
write('Micromap', 'Array centroid',
`// Where the weight of the array sits, as 0..1 along its length: the
// brightness of a Spectrum, the balance of a set of blobs.
var num := 0;
var den := 0;
for(var k := 0; k < xv[]; k += 1) {
  var w := abs(xv[k]);
  num += k * w;
  den += w;
};
num / (max(0.000001, den) * max(1, xv[] - 1))`);

write('Micromap', 'Array peak index',
`// Which element is loudest / brightest / closest.
var best := 0;
var peak := -1000000000;
for(var k := 0; k < xv[]; k += 1) {
  if(xv[k] > peak) { peak := xv[k]; best := k; };
};
best`);

write('Micromap', 'Array energy',
`var s := 0;
for(var k := 0; k < xv[]; k += 1) { s += xv[k] xv[k]; };
s`);

// --- Timing/Control, Beat Tracker ---------------------------------------
write('Micromap', 'BPM to milliseconds', `60000 / max(0.000001, x)`);

// --- Joysticks, sensors, Analysis/Puara ---------------------------------
write('Micromap', 'Deadzone',
`// Ignores the first 10% around zero and rescales the rest, so a resting
// stick or sensor really reads zero.
var d := 0.1;
if(abs(x) < d, 0, sgn(x) * (abs(x) - d) / (1 - d))`);

// --- Led, DMX, and per-element colour -----------------------------------
write('Arraymap', 'Unit to DMX', `round(255 clamp(0, x, 1))`);
write('Arraymap', 'DMX to unit', `clamp(0, x, 255) / 255`);

write('Arraymap', 'Deadzone (rescaled)',
`var d := 0.1;
if(abs(x) < d, 0, sgn(x) * (abs(x) - d) / (1 - d))`);

write('Arraymap', 'sRGB to linear',
`var u := clamp(0, x, 1);
if(u <= 0.04045, u / 12.92, ((u + 0.055) / 1.055) ^ 2.4)`);

write('Arraymap', 'Linear to sRGB',
`var u := clamp(0, x, 1);
if(u <= 0.0031308, 12.92 u, 1.055 * (u ^ (1 / 2.4)) - 0.055)`);

// --- Analysis/Onsets, Beat Tracker: getting a bang out of a stream ------
write('Expression Value Filter', 'Onset to bang',
`// For the onset detectors (Spectral Difference, Complex Spectral Difference,
// High-Frequency Content, Energy Difference).
// a: threshold, b: minimum time between two bangs.
${SEC}if(m2 < 0.5) { m2 := 1; m1 := -1000; };
var fire := if(x > a and tsec - m1 > 0.005 + b, 1, 0);
m1 := if(fire > 0.5, tsec, m1);
fire`, { abc: [0.2, 0.05, 0.5] });

write('Expression Value Filter', 'Beat to pulse',
`// For a 0..1 beat-phase input, as the Beat Tracker emits: 1 on the tick
// where the phase wraps back round, 0 otherwise.
var p := frac(x);
var fire := if(p < m1, 1, 0);
m1 := p;
fire`);

write('Expression Value Filter', 'Deadzone',
`// a: size of the dead band around zero.
var d := 0.001 + a;
if(abs(x) < d, 0, sgn(x) * (abs(x) - d) / (1 - d))`, { abc: [0.1, 0.5, 0.5] });

// --- Musical quantization ------------------------------------------------
const scale = (name, tab, len, span) => write('Expression Value Filter', name,
`// 0..1 in, MIDI note out.
var sc[${len}] := {${tab}};
var d := round(${span} clamp(0, x, 1));
var o := trunc(d / ${len});
36 + 12 o + sc[d % ${len}]`);

scale('Quantize to minor scale', '0, 2, 3, 5, 7, 8, 10', 7, 21);
scale('Quantize to pentatonic', '0, 2, 4, 7, 9', 5, 15);

// --- Audio/Spatialization, Spatialization: speaker layouts ---------------
write('Arraygen', 'Speaker ring',
`// One point per speaker, first one straight ahead, going clockwise.
var a := 2 pi i / n - pi / 2;
return [cos(a), sin(a)]`, { size: 8 });

write('Arraygen', 'Arc',
`// Spread over the 180 degrees in front: frontal speaker arcs, LED bars.
var u := i / max(1, n - 1);
var a := pi (u - 0.5);
return [sin(a), cos(a)]`, { size: 8 });

write('Arraygen', 'Dome',
`// Even coverage of the upper half-sphere.
var y := 1 - (i + 0.5) / n;
var r := sqrt(max(0, 1 - y y));
var a := i * 2.39996322972865332;
return [r cos(a), y, r sin(a)]`, { size: 16 });

// --- Audio/Utilities: stereo plumbing ------------------------------------
write('Expression Audio Filter', 'Mid-side encode',
`// Channel pairs to mid / side.
for(var i := 0; i < x[]; i += 1) { out[i] := x[i]; };
for(var i := 0; i + 1 < x[]; i += 2) {
  out[i] := 0.5 * (x[i] + x[i+1]);
  out[i+1] := 0.5 * (x[i] - x[i+1]);
}`);

write('Expression Audio Filter', 'Mid-side decode',
`// Mid / side pairs back to left / right.
for(var i := 0; i < x[]; i += 1) { out[i] := x[i]; };
for(var i := 0; i + 1 < x[]; i += 2) {
  out[i] := x[i] + x[i+1];
  out[i+1] := x[i] - x[i+1];
}`);

write('Expression Audio Filter', 'Swap channels',
`for(var i := 0; i < x[]; i += 1) { out[i] := x[i]; };
for(var i := 0; i + 1 < x[]; i += 2) {
  out[i] := x[i+1];
  out[i+1] := x[i];
}`);

// --- Audio/Synth: the rest of a drum kit ---------------------------------
write('Expression Audio Generator', 'Snare',
`// a: tempo, b: decay, c: level.
if(m2[0] == 0 and m3[0] == 0) { m2[0] := 1; };
m2[0] += (0.2 + 4 a) / fs;
if(m2[0] >= 1) { m2[0] := m2[0] - 1; m3[0] := 1; };
m3[0] := m3[0] * (1 - (10 + 60 b) / fs);
m1[0] := frac(m1[0] + 180 / fs);
var s := 0.5 c * m3[0] * (1.4 random(-1, 1) + 0.6 sin(2 pi m1[0]));
out[0] := s;
out[1] := s;`, { abc: [0.15, 0.4, 0.8] });

write('Expression Audio Generator', 'Hi-hat',
`// a: tempo, b: decay, c: level.
if(m2[0] == 0 and m3[0] == 0) { m2[0] := 1; };
m2[0] += (0.4 + 8 a) / fs;
if(m2[0] >= 1) { m2[0] := m2[0] - 1; m3[0] := 1; };
m3[0] := m3[0] * (1 - (40 + 300 b) / fs);
var nz := random(-1, 1);
m1[0] := m1[0] + 0.6 * (nz - m1[0]);
var s := 0.5 c * m3[0] * (nz - m1[0]);
out[0] := s;
out[1] := s;`, { abc: [0.2, 0.3, 0.8] });

/* ===================================================================== */
/* Categorise everything else, then check nothing was missed              */
/* ===================================================================== */

// The presets this script does not author still need a menu category, and they
// may not live in the folder named after their process - "Value Generator"
// holds Expression Value Generator presets too. So walk the whole library and
// dispatch on the uuid.
const BY_UUID = Object.fromEntries(
    Object.entries(PROC).map(([name, p]) => [p.uuid, name]));

function walk(dir, out = []) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if (e.isDirectory())
      walk(p, out);
    else if (e.name.endsWith('.scp'))
      out.push(p);
  }
  return out;
}

let stamped = 0, checked = 0;
const missing = [];
for (const file of walk(ROOT)) {
  const raw = fs.readFileSync(file, 'utf8');
  const j = JSON.parse(raw);
  const proc = BY_UUID[j.Key && j.Key.Uuid];
  if (!proc)
    continue; // a preset for some other process: not ours to categorise

  checked++;
  const wanted = CATEGORIES[proc][j.Name];
  if (!wanted) {
    missing.push(`${proc} / ${j.Name}  (${path.relative(ROOT, file)})`);
    continue;
  }
  if (j.Category === wanted)
    continue;

  // Re-emit with the category in place, keeping everything else byte-identical.
  const withCat = raw.replace(
      /("Name":(?:[^"\\]|\\.)*"(?:[^"\\]|\\.)*")\s*,\s*(?:"Category":(?:[^"\\]|\\.)*"(?:[^"\\]|\\.)*"\s*,\s*)?/,
      `$1,"Category":${JSON.stringify(wanted)},`);
  if (withCat === raw)
    throw new Error('could not stamp a category onto ' + file);
  fs.writeFileSync(file, withCat);
  stamped++;
}

if (missing.length) {
  console.error('presets with no entry in CATEGORIES:');
  for (const m of missing)
    console.error('  ' + m);
  process.exit(1);
}

console.error(`${checked} ExprTK presets, ${stamped} newly categorised`);
