/*{
  "DESCRIPTION": "Audio histogram readout next to a waveform input of the same node: pixel (x, y) shows histogram texel (x, y) for the two newest rows. R = level, G = 1 when any of the 240 rows holds a NaN or infinite value in column x, B = 1 when x lies inside the histogram texture.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-AUDIO"],
  "INPUTS": [
    { "NAME": "hist", "TYPE": "audioHistogram", "MAX": 256 },
    { "NAME": "wave", "TYPE": "audio", "MAX": 256 }
  ]
}*/

void main()
{
    int x = int(gl_FragCoord.x);
    int y = int(gl_FragCoord.y);
    ivec2 sz = textureSize(hist, 0);
    bool inside = x < sz.x;
    bool bad = false;
    for(int r = 0; r < sz.y && inside; r++)
    {
        float t = texelFetch(hist, ivec2(x, r), 0).r;
        bad = bad || isnan(t) || isinf(t);
    }
    float v = inside ? texelFetch(hist, ivec2(x, y), 0).r : 0.0;
    float w = texelFetch(wave, ivec2(0, 0), 0).r;
    gl_FragColor = vec4(bad ? 0.0 : clamp(v, 0.0, 1.0), bad ? 1.0 : 0.0, inside ? 1.0 : 0.0, w > -1.0 ? 1.0 : 0.0);
}
