/*{
  "DESCRIPTION": "Minimal image passthrough: outputs its input texture verbatim on every pixel (no grid, no border). Used by the rt_changed regression: A -> this(intermediate) -> sink. When this intermediate node's input render-target spec changes at runtime, its OWN output pass into the sink must survive; otherwise the sink goes black.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-IMAGE"],
  "INPUTS": [
    { "NAME": "inputImage", "TYPE": "image" }
  ]
}*/

void main()
{
    gl_FragColor = IMG_NORM_PIXEL(inputImage, isf_FragNormCoord);
}
