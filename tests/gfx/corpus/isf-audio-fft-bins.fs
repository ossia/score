/*{
  "DESCRIPTION": "Audio FFT readout: pixel column x shows FFT texel x of channel 0. R = 4 * magnitude, G = 1 when the texel is NaN or infinite, B = 1 when x lies inside the FFT texture.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-AUDIO"],
  "INPUTS": [
    { "NAME": "fft", "TYPE": "audioFFT", "MAX": 256 }
  ]
}*/

void main()
{
    int x = int(gl_FragCoord.x);
    int w = textureSize(fft, 0).x;
    float v = x < w ? texelFetch(fft, ivec2(x, 0), 0).r : 0.0;
    bool bad = isnan(v) || isinf(v);
    gl_FragColor = vec4(bad ? 0.0 : clamp(v * 4.0, 0.0, 1.0), bad ? 1.0 : 0.0, x < w ? 1.0 : 0.0, 1.0);
}
