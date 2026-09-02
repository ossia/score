/*{
  "DESCRIPTION": "Point-count oracle for the pointcloud path (P1-11). Position-only RAW_RASTER_PIPELINE: draws upstream geometry in whatever topology the mesh declares; with a Points-topology pointcloud mesh and gl_PointSize = 1.0 in the vertex stage, every vertex whose position is placed exactly on a pixel center rasterizes as exactly one solid-red pixel, so lit-pixel count == drawn vertex count. Deliberately declares ONLY the position vertex input so a position-only mesh (PCLToMesh2 XYZ layout) matches the shader exactly with no fallback bindings.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "PIPELINE_STATE": { "TOPOLOGY": "points" },
  "CATEGORIES": ["TEST-RAW-RASTER"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "v_color" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec4", "NAME": "v_color" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = v_color;
}
