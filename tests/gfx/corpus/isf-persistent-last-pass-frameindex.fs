/*{
  "DESCRIPTION": "Writes FRAMEINDEX / 255 in red through a PERSISTENT last pass. Must read back exactly like isf-frameindex.fs, which writes the same value straight to the output.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-PERSISTENT"],
  "PASSES": [ { "TARGET": "history", "PERSISTENT": true } ]
}*/

void main()
{
    gl_FragColor = vec4(float(FRAMEINDEX) / 255.0, 0.0, 0.0, 1.0);
}
