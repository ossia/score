/*{
  "DESCRIPTION": "VSA triangle fan: vertex 0 at the centre, vertices 1-7 around a regular hexagon of radius 0.8. Drawn as a fan it fills the hexagon (41.6 % of the frame); drawn as a triangle list it fills two sixths of it.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "MODE": "VERTEX_SHADER_ART",
  "POINT_COUNT": 8,
  "PRIMITIVE_MODE": "TRI_FAN",
  "BACKGROUND_COLOR": [0.0, 0.0, 0.0, 1.0],
  "INPUTS": []
}*/

void main() {
  vec2 p = vec2(0.0);
  if(vertexId > 0.5)
  {
    float a = (vertexId - 1.0) * 6.28318530718 / 6.0;
    p = 0.8 * vec2(cos(a), sin(a));
  }
  gl_Position = vec4(p, 0.0, 1.0);
  v_color = vec4(0.9, 0.2, 0.1, 1.0);
}
