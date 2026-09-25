/*{
  "DESCRIPTION": "Histogram row readout: pixel column x shows the largest texel of histogram row x (row 0 is the newest).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-AUDIO"],
  "INPUTS": [
    { "NAME": "hist", "TYPE": "audioHistogram" }
  ]
}*/

void main()
{
    int y = int(gl_FragCoord.x);
    ivec2 sz = textureSize(hist, 0);
    float m = 0.0;
    for(int c = 0; c < sz.x && y < sz.y; c++)
        m = max(m, texelFetch(hist, ivec2(c, y), 0).r);
    gl_FragColor = vec4(clamp(m, 0.0, 1.0), 0.0, 0.0, 1.0);
}
