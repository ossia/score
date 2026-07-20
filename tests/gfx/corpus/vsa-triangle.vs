/*{
  "DESCRIPTION": "VSA cull regression (R3-N4): one solid triangle covering the viewport center. VSA applies a per-backend cull mode (CullMode::Front on OpenGL, CullMode::Back elsewhere, FrontFace CW) so that a procedurally-emitted front-facing triangle is VISIBLE on every backend. If the cull-mode setup regresses (clobbered back to None, or the same mode on all backends), the front face is either over-drawn with backfaces or culled entirely on one backend, so the backends diverge or the center goes to the background color.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "MODE": "VERTEX_SHADER_ART",
  "POINT_COUNT": 3,
  "PRIMITIVE_MODE": "TRIANGLES",
  "BACKGROUND_COLOR": [0.0, 0.0, 0.0, 1.0],
  "INPUTS": []
}*/

void main() {
  // Three vertices of a triangle large enough to cover the viewport center.
  vec2 p = vec2(0.0);
  if(vertexId < 0.5)
    p = vec2(-1.5, -1.5);
  else if(vertexId < 1.5)
    p = vec2( 1.5, -1.5);
  else
    p = vec2( 0.0,  1.5);

  gl_Position = vec4(p, 0.0, 1.0);

  // Solid, distinctive front-face color.
  v_color = vec4(0.9, 0.4, 0.1, 1.0);
}
