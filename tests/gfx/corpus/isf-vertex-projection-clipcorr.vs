// The transform GfxIsfVertexProjection recomputes on the CPU.
const float PI_CONST = 3.14159265359;

mat4 view_frustum(float angle_of_view, float aspect_ratio, float z_near, float z_far)
{
  return mat4(
      vec4(1.0 / tan(angle_of_view), 0.0, 0.0, 0.0),
      vec4(0.0, aspect_ratio / tan(angle_of_view), 0.0, 0.0),
      vec4(0.0, 0.0, (z_far + z_near) / (z_far - z_near), 1.0),
      vec4(0.0, 0.0, -2.0 * z_far * z_near / (z_far - z_near), 0.0));
}

mat4 translate(float x, float y, float z)
{
  return mat4(
      vec4(1.0, 0.0, 0.0, 0.0), vec4(0.0, 1.0, 0.0, 0.0), vec4(0.0, 0.0, 1.0, 0.0),
      vec4(x, y, z, 1.0));
}

mat4 rotate_x(float t)
{
  float c = cos(t * PI_CONST), s = sin(t * PI_CONST);
  return mat4(
      vec4(1.0, 0.0, 0.0, 0.0), vec4(0.0, c, s, 0.0), vec4(0.0, -s, c, 0.0),
      vec4(0.0, 0.0, 0.0, 1.0));
}

mat4 rotate_y(float t)
{
  float c = cos(t * PI_CONST), s = sin(t * PI_CONST);
  return mat4(
      vec4(c, 0.0, s, 0.0), vec4(0.0, 1.0, 0.0, 0.0), vec4(-s, 0.0, c, 0.0),
      vec4(0.0, 0.0, 0.0, 1.0));
}

mat4 rotate_z(float t)
{
  float c = cos(t * PI_CONST), s = sin(t * PI_CONST);
  return mat4(
      vec4(c, -s, 0.0, 0.0), vec4(s, c, 0.0, 0.0), vec4(0.0, 0.0, 1.0, 0.0),
      vec4(0.0, 0.0, 0.0, 1.0));
}

mat4 scale(float x, float y, float z)
{
  return mat4(
      vec4(x, 0.0, 0.0, 0.0), vec4(0.0, y, 0.0, 0.0), vec4(0.0, 0.0, z, 0.0),
      vec4(0.0, 0.0, 0.0, 1.0));
}

void main()
{
  isf_vertShaderInit();
  float aspect = RENDERSIZE.x / RENDERSIZE.y;
  // Written for an engine whose init gave no clip-space correction of its own.
  gl_Position = clipSpaceCorrMatrix * view_frustum(radians(45.0), aspect, 0.5, 10.0)
                * translate(0.3, -0.2, 2.0) * rotate_x(-0.15) * rotate_y(0.1)
                * rotate_z(-0.1) * scale(0.6 * aspect, 0.6, 0.6) * gl_Position;
  isf_vertShaderFinish();
}
