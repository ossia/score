/*{
  "DESCRIPTION": "A quad the vertex shader rotates and projects in perspective, as VIDVOX 3d Rotate does, drawn solid white. Its vertex shader multiplies the projected position by clipSpaceCorrMatrix itself, then ends with isf_vertShaderFinish. Used by GfxIsfVertexProjection.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "INPUTS": []
}*/
void main()
{
  gl_FragColor = vec4(1.0);
}
