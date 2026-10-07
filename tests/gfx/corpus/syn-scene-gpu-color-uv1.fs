/*{
  "DESCRIPTION": "Draws the flattened scene's geometry with its color_0 stream on the left half of the target and its texcoord_1 stream (as red, green) on the right half. Used by GfxScenePreprocessorGpuColorUv1 to see whether the preprocessor copied an upstream GPU color0 / texcoord1 into its arena.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec4", "NAME": "color_0" },
    { "TYPE": "vec2", "NAME": "texcoord_1" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "v_color" },
    { "TYPE": "vec2", "NAME": "v_uv1" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec4", "NAME": "v_color" },
    { "TYPE": "vec2", "NAME": "v_uv1" }
  ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "PIPELINE_STATE": { "DEPTH_TEST": false, "DEPTH_WRITE": false, "CULL_MODE": "none" },
  "INPUTS": []
}*/
void main()
{
    if(gl_FragCoord.x < 32.0)
        isf_FragColor = vec4(v_color.rgb, 1.0);
    else
        isf_FragColor = vec4(v_uv1, 0.0, 1.0);
}
