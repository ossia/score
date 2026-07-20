/*{
  "DESCRIPTION": "VSA smoke shader: a 64x64 grid of colored POINTS filling the viewport. Points are not affected by cull mode, so this proves the VSA node construction + procedural vertexId draw + readback works and AGREES across backends, independent of the triangle cull path.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "MODE": "VERTEX_SHADER_ART",
  "POINT_COUNT": 4096,
  "PRIMITIVE_MODE": "POINTS",
  "BACKGROUND_COLOR": [0.0, 0.0, 0.0, 1.0],
  "INPUTS": []
}*/

void main() {
  float n = 64.0;
  float col = mod(vertexId, n);
  float row = floor(vertexId / n);

  // Grid position across the whole viewport, centered.
  vec2 p = (vec2(col, row) + 0.5) / n * 2.0 - 1.0;

  gl_Position = vec4(p, 0.0, 1.0);
  gl_PointSize = 2.0;

  // Static color = position -> deterministic, time-independent readback.
  v_color = vec4(col / n, row / n, 0.5, 1.0);
}
