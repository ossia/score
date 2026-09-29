// The camera construction of the library's VoxelMeshRenderer: a right-handed
// look-at from +Z and a reverse-Z perspective.
mat4 buildPerspective(float fovDeg, float aspect, float near, float far)
{
  float f = 1.0 / tan(radians(fovDeg) * 0.5);
  float nf = 1.0 / (near - far);
  return mat4(
    f / aspect, 0.0, 0.0,                    0.0,
    0.0,        f,   0.0,                    0.0,
    0.0,        0.0, -(far + near) * nf,     -1.0,
    0.0,        0.0, -2.0 * far * near * nf, 0.0);
}

mat4 buildLookAt(vec3 eye, vec3 target, vec3 up)
{
  vec3 f = normalize(target - eye);
  vec3 s = normalize(cross(f, up));
  vec3 u = cross(s, f);
  return mat4(
    s.x,          u.x,          -f.x,        0.0,
    s.y,          u.y,          -f.y,        0.0,
    s.z,          u.z,          -f.z,        0.0,
    -dot(s, eye), -dot(u, eye), dot(f, eye), 1.0);
}

void main()
{
  isf_vertShaderInit();

  mat4 proj = buildPerspective(60.0, RENDERSIZE.x / RENDERSIZE.y, 0.1, 100.0);
  mat4 view = buildLookAt(vec3(0.0, 0.0, 5.0), vec3(0.0), vec3(0.0, 1.0, 0.0));
  gl_Position = clipSpaceCorrMatrix * proj * view * MODEL_MATRIX * vec4(position.xyz, 1.0);
  v_normal = normal.xyz;

  isf_vertShaderFinish();
}
