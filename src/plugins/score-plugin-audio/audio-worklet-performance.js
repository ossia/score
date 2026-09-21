// AudioWorkletGlobalScope has no performance object
// (https://github.com/WebAudio/web-audio-api/issues/2527).
//
// Emscripten works around that for emscripten_get_now(), which falls back to
// Date.now(), but the flag clock_gettime() consults is derived from the very
// same missing object:
//
//     var nowIsMonotonic = !!globalThis.performance?.now;
//     ... clock_gettime(CLOCK_MONOTONIC): nowIsMonotonic ? now : return 52
//
// so on the audio thread clock_gettime(CLOCK_MONOTONIC) fails, libc++ throws
// from std::chrono::steady_clock::now(), and the program aborts. Everything the
// DSP graph does with a steady clock is affected; MIDI input timestamping is
// merely where score hits it first.
//
// What emscripten reads is timeOrigin + now(), which on the main thread is the
// Unix epoch in milliseconds: Date.now() is the same quantity, so both threads
// keep reporting times on one scale. Clamp it to its own maximum, because a
// clock callers reach through steady_clock must not run backwards when the
// system time is adjusted.
if (globalThis.AudioWorkletGlobalScope && !globalThis.performance) {
  let last = 0;
  globalThis.performance = {
    timeOrigin: 0,
    now: () => (last = Math.max(last, Date.now())),
  };
}
