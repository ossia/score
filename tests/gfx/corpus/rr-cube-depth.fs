/*{
  "DESCRIPTION": "Procedural raw raster with a cube colour output and a declared depth output, neither sized, so both follow the render size. Used by GfxCubeDepthAttachmentSize: the depth attachment must be as square as the cube it is rendered with.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-CUBEMAP"],
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [
    { "NAME": "cube", "TYPE": "color", "FORMAT": "rgba8", "CUBEMAP": true },
    { "NAME": "depth", "TYPE": "depth", "FORMAT": "d32f" }
  ],
  "INPUTS": [],
  "PIPELINE_STATE": {
    "DEPTH_TEST": true,
    "DEPTH_WRITE": true,
    "DEPTH_COMPARE": "always",
    "CULL_MODE": "none",
    "VERTEX_COUNT": 3,
    "TOPOLOGY": "triangles"
  }
}*/
void main()
{
    isf_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
