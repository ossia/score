void main()
{
  isf_vertShaderInit();
  // Half the frame, turned by 0.35 rad: four slanted edges inside the picture.
  const float c = cos(0.35), s = sin(0.35);
  vec2 p = gl_Position.xy * 0.5;
  gl_Position.xy = vec2(c * p.x - s * p.y, s * p.x + c * p.y);
  isf_vertShaderFinish();
}
