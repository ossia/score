/*{
  "DESCRIPTION": "Draws the flattened scene's geometry in solid green, reading only the vec4 position stream. Used by GfxScenePreprocessorGpuAttributeSwap to see which upstream GPU position buffer the preprocessor copied into its arena.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE"],
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none" },
  "INPUTS": []
}*/
void main()
{
    isf_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
