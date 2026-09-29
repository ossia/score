/*{
  "DESCRIPTION": "Draws a Voxel Loader mesh with its face normal as colour (n * 0.5 + 0.5). Declares no PIPELINE_STATE, so culling and the front face are the geometry's own, as in VoxelMeshRenderer.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-RAW-RASTER"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec4", "NAME": "normal" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec3", "NAME": "v_normal" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec3", "NAME": "v_normal" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "INPUTS": []
}*/

void main()
{
  isf_FragColor = vec4(normalize(v_normal) * 0.5 + 0.5, 1.0);
}
