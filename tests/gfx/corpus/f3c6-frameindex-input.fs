/*{
  "DESCRIPTION": "Has an image input it never samples, so a test node can feed it, and outputs FRAMEINDEX / 255 in red with green at 1 as the witness. Used by GfxDefinePiFrameLeakF3.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" }
  ]
}*/
void main()
{
    isf_FragColor = vec4(float(FRAMEINDEX) / 255.0, 1.0, 0.0, 1.0);
}
