/*{
  "DESCRIPTION": "EXECUTION_MODEL PER_MIP over a mipped output: the pass is looped once per level with PASSINDEX carrying the level. Each level is painted with its own index, so reading level 0 back gives 0 and a runtime that ran the pass once (or bound the wrong level) is distinguishable from one that looped correctly.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-EXECUTION-MODEL"],
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [
    { "NAME": "mipped", "TYPE": "color", "FORMAT": "rgba8", "GENERATE_MIPS": true,
      "WIDTH": 64, "HEIGHT": 64 }
  ],
  "EXECUTION_MODEL": { "TYPE": "PER_MIP", "TARGET": "mipped" },
  "INPUTS": []
}*/

void main()
{
    // PASSINDEX is the mip level under PER_MIP: level 0 paints 0, level 1
    // paints 1/255, and so on.
    isf_FragColor = vec4(float(PASSINDEX) / 255.0, 1.0, 0.0, 1.0);
}
