/*{
  "DESCRIPTION": "Encodes MODEL_MATRIX's translation column into the fragment colour, so a test can see whether a scene transform reached the shader at all -- separately from whether it moved the geometry.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-SCENE"],
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [ { "TYPE": "vec4", "NAME": "v_mm" } ],
  "FRAGMENT_INPUTS": [ { "TYPE": "vec4", "NAME": "v_mm" } ],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "INPUTS": []
}*/

void main()
{
    // +0.5 bias so a translation of 0 reads mid-grey and a negative one is visible.
    isf_FragColor = vec4(v_mm.x + 0.5, v_mm.y + 0.5, v_mm.z + 0.5, 1.0);
}
