/*{
  "DESCRIPTION": "RAW_RASTER_PIPELINE consumer that reads BOTH halves of ScenePreprocessor's interleaved per-instance slot: the translation at byte 0 of the 32-byte slot and the color at byte 16. It draws each instance's quad displaced by the translation and shaded by the color, so the two halves are separable in pixels: the preprocessor fills a regular (non-instance-group) slot with translation (0,0,0,0) and color (1,1,1,1), which means reading the color at the wrong offset paints black and reading the translation at the wrong offset shifts the quad a full NDC unit off screen. A blank frame is therefore the failure mode of BOTH mistakes, and a centred white quad is only produced by reading both correctly.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE", "TEST-INSTANCING"],
  "VERTEX_INPUTS": [
    { "TYPE": "vec4", "NAME": "position" },
    { "TYPE": "vec3", "NAME": "inst_translation", "SEMANTIC": "translation" },
    { "TYPE": "vec4", "NAME": "inst_color0", "SEMANTIC": "instance_color0" }
  ],
  "VERTEX_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "v_col" }
  ],
  "FRAGMENT_INPUTS": [
    { "TYPE": "vec4", "NAME": "v_col" }
  ],
  "FRAGMENT_OUTPUTS": [
    { "TYPE": "vec4", "NAME": "isf_FragColor" }
  ],
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = vec4(v_col.rgb, 1.0);
}
