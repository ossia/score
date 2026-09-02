/*{
  "DESCRIPTION": "HDR probe for the rt FORMAT change test (GfxRtFormatChanged.cpp). Samples its input and outputs rgb * 0.5 with alpha 1. Fed by syn-hdr-writer.fs (red = 2.0): if the intermediate render target the writer drew into is RGBA8, the stored red clipped to 1.0 and this outputs 0.5 (readback ~128); if it is RGBA16F the 2.0 survived and this outputs 1.0 (readback 255). Same shaders, two formats -- the >1.0 survival is the oracle, no golden needed. Green (0.5 -> 0.25 -> ~64) is exact in BOTH formats: a control channel proving the pass still samples the producer (not black) across the format change.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-IMAGE"],
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" }
  ]
}*/

void main()
{
    vec4 c = IMG_NORM_PIXEL(inputImage, isf_FragNormCoord);
    gl_FragColor = vec4(c.rgb * 0.5, 1.0);
}
