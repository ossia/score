float s1trWindowDepth(float d)
{
#if !defined(QSHADER_SPIRV) && !defined(QSHADER_HLSL) && !defined(QSHADER_MSL)
  return d * 2.0 - 1.0;
#else
  return d;
#endif
}

void main()
{
  isf_vertShaderInit();
  const vec2 corners[6] = vec2[6](
      vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
      vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));
  int quad = gl_VertexIndex / 6;
  bool red = quad == 1;
  v_color = red ? vec4(0.5, 0.0, 0.0, 0.5) : vec4(0.0, 0.6, 0.0, 0.6);
  gl_Position = vec4(corners[gl_VertexIndex % 6], s1trWindowDepth(red ? 0.3 : 0.6), 1.0);
  isf_vertShaderFinish();
}
