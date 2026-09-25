/*{
  "DESCRIPTION": "Both stages declare their own #define PI, as user shaders do. One fullscreen triangle scaled by sin(PI / 2), painting sin(PI / 2) in red and PI / 4 in green, opaque. Any PI the engine declares in the text around either stage breaks it. Used by GfxDefinePiFrameLeakF3.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none", "VERTEX_COUNT": 3, "TOPOLOGY": "triangles" }
}*/
#define PI 3.1415926535

void main()
{
    isf_FragColor = vec4(sin(PI * 0.5), PI / 4.0, 0.0, 1.0);
}
