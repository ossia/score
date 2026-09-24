/*{
  "DESCRIPTION": "Step-clock probe for GfxContext::renderFrames at a step rate of 60. A persistent storage buffer counts, over every frame drawn: frames whose TIMEDELTA is not 1/60, frames whose TIME did not advance by 1/60 since the previous frame, and frames with a negative TIMEDELTA. Left half: those three counts in r, g, b (/255): black when the step clock is exact. Right half: the number of frames counted (/255) in all three channels, so a black left half is known to cover real frames.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-UNIFORMS"],
  "INPUTS": [
    {
      "NAME": "acc",
      "TYPE": "storage",
      "ACCESS": "read_write",
      "PERSISTENT": true,
      "LAYOUT": [
        { "NAME": "frames", "TYPE": "float" },
        { "NAME": "badDelta", "TYPE": "float" },
        { "NAME": "badTime", "TYPE": "float" },
        { "NAME": "negDelta", "TYPE": "float" },
        { "NAME": "prevTime", "TYPE": "float" },
        { "NAME": "hasPrev", "TYPE": "float" }
      ]
    }
  ]
}*/

void main()
{
    const float dt = 1.0 / 60.0;
    float frames = acc_prev.frames + 1.0;
    float badDelta = acc_prev.badDelta + (abs(TIMEDELTA - dt) > 0.01 * dt ? 1.0 : 0.0);
    float badTime = acc_prev.badTime;
    if(acc_prev.hasPrev > 0.5 && abs((TIME - acc_prev.prevTime) - dt) > 0.01 * dt)
        badTime += 1.0;
    float negDelta = acc_prev.negDelta + (TIMEDELTA < 0.0 ? 1.0 : 0.0);

    acc.frames = frames;
    acc.badDelta = badDelta;
    acc.badTime = badTime;
    acc.negDelta = negDelta;
    acc.prevTime = TIME;
    acc.hasPrev = 1.0;

    if(isf_FragNormCoord.x < 0.5)
        gl_FragColor = vec4(min(vec3(badDelta, badTime, negDelta), 255.0) / 255.0, 1.0);
    else
        gl_FragColor = vec4(vec3(min(frames, 255.0) / 255.0), 1.0);
}
