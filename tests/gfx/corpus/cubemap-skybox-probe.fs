/*{
  "DESCRIPTION": "Procedural raw raster sampling the cube named skybox along -Z: fed either by a cubemap cable or by the Scene Preprocessor's skybox auxiliary texture.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "VERTEX_INPUTS": [],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": [ { "NAME": "skybox", "TYPE": "cubemap" } ],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none", "VERTEX_COUNT": 3, "TOPOLOGY": "triangles" }
}*/
void main()
{
    isf_FragColor = vec4(texture(skybox, vec3(0.0, 0.0, -1.0)).rgb, 1.0);
}
