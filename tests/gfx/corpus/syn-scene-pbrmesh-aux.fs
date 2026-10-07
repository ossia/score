/*{
  "DESCRIPTION": "Draws the flattened scene's geometry with red read from the pbr_aux_color buffer and blue from the pbr_aux_tex texture, two scene auxiliaries resolved by name; green is always full, so a drawn quad is told from the background when neither auxiliary is bound. Used by GfxPBRMeshUpstreamGeometry to see whether the auxiliaries of the geometry wired into PBR Mesh reach a raster downstream of the Scene Preprocessor.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE"],
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none" },
  "INPUTS": [],
  "AUXILIARY": [
    { "NAME": "pbr_aux_color", "ACCESS": "read_only",
      "LAYOUT": [ { "NAME": "color", "TYPE": "vec4" } ] },
    { "NAME": "pbr_aux_tex", "TYPE": "texture", "FILTER": "nearest" }
  ]
}*/
void main()
{
    isf_FragColor = vec4(
        pbr_aux_color.color.r, 1.0, textureLod(pbr_aux_tex, vec2(0.5), 0.0).b, 1.0);
}
