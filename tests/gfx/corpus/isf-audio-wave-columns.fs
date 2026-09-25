/*{
  "DESCRIPTION": "Waveform readout: pixel column x shows waveform texel x of channel 0. R = texel, B = 1 when x lies inside the waveform texture.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-AUDIO"],
  "INPUTS": [
    { "NAME": "wave", "TYPE": "audio" }
  ]
}*/

void main()
{
    int x = int(gl_FragCoord.x);
    int w = textureSize(wave, 0).x;
    float v = x < w ? texelFetch(wave, ivec2(x, 0), 0).r : 0.0;
    gl_FragColor = vec4(clamp(v, 0.0, 1.0), 0.0, x < w ? 1.0 : 0.0, 1.0);
}
