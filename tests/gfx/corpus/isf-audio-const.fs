/*{
  "DESCRIPTION": "Audio-input coverage: a single TYPE:audio INPUT emitted verbatim as a grayscale level. The audio input is a sampler2D (X=sample, Y=channel); score's temporal path stores a sample s as texel 0.5 + s/2. Feed a CONSTANT single-channel buffer of value s and every readback pixel MUST equal round(255*(0.5 + s/2)). Proves the audio sampler is fed and bound in the offscreen path.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-AUDIO"],
  "INPUTS": [
    { "NAME": "wave", "TYPE": "audio", "MAX": 256 }
  ]
}*/

void main()
{
    // Sample channel 0 at the horizontal position. For a constant buffer the
    // value is identical everywhere, so the output is a uniform gray level.
    float v = IMG_NORM_PIXEL(wave, vec2(isf_FragNormCoord.x, 0.0)).r;
    gl_FragColor = vec4(vec3(v), 1.0);
}
