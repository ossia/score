/*{
  "DESCRIPTION": "Classic ISF whose PIPELINE_STATE asks for 64 points (VERTEX_COUNT), which classic ISF refuses (shader board F16). Used by GfxIsfVertexCount.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "INPUTS": [],
  "PIPELINE_STATE": { "TOPOLOGY": "points", "VERTEX_COUNT": 64 }
}*/
void main()
{
  gl_FragColor = vec4(1.0);
}
