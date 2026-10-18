/*{
  "DESCRIPTION": "VSA event input: one triangle covering the frame, red on the frame the event fires, dark grey otherwise. Used by GfxVsa.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "MODE": "VERTEX_SHADER_ART",
  "POINT_COUNT": 3,
  "PRIMITIVE_MODE": "TRIANGLES",
  "BACKGROUND_COLOR": [0.0, 0.0, 0.0, 1.0],
  "INPUTS": [ { "NAME": "flash", "TYPE": "event" } ]
}*/

void main() {
  vec2 p = vertexId < 0.5 ? vec2(-1.0, -1.0) : vertexId < 1.5 ? vec2(3.0, -1.0) : vec2(-1.0, 3.0);
  gl_Position = vec4(p, 0.0, 1.0);
  v_color = flash ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.2, 0.2, 0.2, 1.0);
}
