/*{
  "DESCRIPTION": "EXECUTION_MODEL PER_CUBE_FACE over a cubemap output: six invocations, one per face, with PASSINDEX carrying the face index and the render target bound to that layer. Each face is painted with its own index, so face 0 reads 0 -- a runtime that ran once, or bound one layer six times, is distinguishable from one that walked the faces.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "RAW_RASTER_PIPELINE",
  "CATEGORIES": ["TEST-SYNTHETIC", "TEST-EXECUTION-MODEL"],
  "VERTEX_INPUTS": [ { "TYPE": "vec4", "NAME": "position" } ],
  "VERTEX_OUTPUTS": [],
  "FRAGMENT_INPUTS": [],
  "FRAGMENT_OUTPUTS": [ { "TYPE": "vec4", "NAME": "isf_FragColor" } ],
  "OUTPUTS": [
    { "NAME": "faces", "TYPE": "color", "FORMAT": "rgba8", "CUBEMAP": true,
      "LAYERS": 6, "WIDTH": 32, "HEIGHT": 32 }
  ],
  "EXECUTION_MODEL": { "TYPE": "PER_CUBE_FACE", "TARGET": "faces" },
  "INPUTS": []
}*/

void main()
{
    isf_FragColor = vec4(float(PASSINDEX) / 255.0, 1.0, 0.0, 1.0);
}
