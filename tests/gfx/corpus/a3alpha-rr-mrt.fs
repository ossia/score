/*{
  "DESCRIPTION": "Procedural raw raster with two FRAGMENT_OUTPUTS (the MRT path), both writing premultiplied (0.5, 0, 0, 0.5).",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "out0" }, { "TYPE": "vec4", "NAME": "out1" } ],
  "INPUTS": [],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none", "VERTEX_COUNT": 3, "TOPOLOGY": "triangles" }
}*/
void main()
{
    out0 = vec4(0.5, 0.0, 0.0, 0.5);
    out1 = vec4(0.5, 0.0, 0.0, 0.5);
}
