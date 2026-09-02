/*{
  "DESCRIPTION": "HDR producer for the rt FORMAT change test (GfxRtFormatChanged.cpp). Writes a constant color with RED ABOVE 1.0: vec4(2.0, 0.5, 0.0, 1.0). Whether the 2.0 survives depends ONLY on the render target it is drawn into: an RGBA8 (UNorm) target clamps the stored red to 1.0; an RGBA16F (float) target stores 2.0 exactly. Half floats represent 2.0, 0.5 and their halves exactly, so the whole chain is closed-form. Paired with syn-hdr-halver.fs which multiplies by 0.5 so the difference becomes visible in an 8-bit readback.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-SYNTH"],
  "INPUTS": []
}*/

void main()
{
    gl_FragColor = vec4(2.0, 0.5, 0.0, 1.0);
}
