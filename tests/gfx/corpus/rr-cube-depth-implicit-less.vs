float cubeWindowDepth(float d)
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
  bool first = gl_VertexIndex < 6;
  v_color = first ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 1.0, 0.0, 1.0);
  gl_Position = vec4(corners[gl_VertexIndex % 6], cubeWindowDepth(first ? 0.2 : 0.8), 1.0);
  isf_vertShaderFinish();
}
