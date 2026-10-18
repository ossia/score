void main()
{
  isf_vertShaderInit();
  const int k = gl_VertexIndex;
  vec2 cell = (vec2(k % 8, k / 8) + 0.5) / 8.0;
  gl_Position = vec4(cell * 2.0 - 1.0, 0.0, 1.0);
  gl_PointSize = 1.0;
  isf_vertShaderFinish();
}
