/*{
  "DESCRIPTION": "GfxRawRasterOutputAndVertexInput: reads a required vertex input, unpublished_input, that no geometry producer publishes. The draw is skipped and the skip is reported at warning level, naming the input.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec4", "NAME": "unpublished_input" }
  ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = vec4(1.0);
}
