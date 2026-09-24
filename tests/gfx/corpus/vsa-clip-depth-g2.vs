/*{
  "DESCRIPTION": "VSA clip depth (N39): one triangle covering the viewport center at clip z = -0.75 w. VSA shaders are written for WebGL clip space (z in [-w, w]); this depth is inside it and must be drawn on every backend.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "MODE": "VERTEX_SHADER_ART",
  "POINT_COUNT": 3,
  "PRIMITIVE_MODE": "TRIANGLES",
  "BACKGROUND_COLOR": [0.0, 0.0, 0.0, 1.0],
  "INPUTS": []
}*/

void main() {
  vec2 p = vec2(0.0);
  if(vertexId < 0.5)
    p = vec2(-1.5, -1.5);
  else if(vertexId < 1.5)
    p = vec2( 1.5, -1.5);
  else
    p = vec2( 0.0,  1.5);

  gl_Position = vec4(p, -0.75, 1.0);
  v_color = vec4(0.9, 0.4, 0.1, 1.0);
}
