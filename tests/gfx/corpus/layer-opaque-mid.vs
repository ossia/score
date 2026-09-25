void main()
{
  isf_vertShaderInit();
  vec2 p = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
#if !defined(QSHADER_SPIRV) && !defined(QSHADER_HLSL) && !defined(QSHADER_MSL)
  gl_Position = vec4(p * 2.0 - 1.0, 0.45 * 2.0 - 1.0, 1.0);
#else
  gl_Position = vec4(p * 2.0 - 1.0, 0.45, 1.0);
#endif
  isf_vertShaderFinish();
}
