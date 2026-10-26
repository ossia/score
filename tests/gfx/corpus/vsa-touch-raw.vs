/*{
  "DESCRIPTION": "Reads a cabled texture on the touch input with raw texture() / texelFetch, as VSA art does, in three vertical bands: texture(0.25, 0.25), texture(0.25, 0.75), texelFetch(0, 0). Cabled to isf-orient-quadrants (blue bottom-left, red top-left), v = 0 is the bottom of the image on every backend: blue, red, blue. Used by GfxVsaTouch.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "MODE": "VERTEX_SHADER_ART",
  "POINT_COUNT": 18,
  "PRIMITIVE_MODE": "TRIANGLES",
  "BACKGROUND_COLOR": [0.0, 0.0, 0.0, 1.0],
  "INPUTS": [ { "LABEL": "Touch", "NAME": "touch", "TYPE": "image" } ]
}*/

void main() {
  float band = floor(vertexId / 6.0);
  float k = mod(vertexId, 6.0);
  vec2 c = vec2(0.0);
  if(k < 0.5) c = vec2(0.0, 0.0);
  else if(k < 1.5) c = vec2(1.0, 0.0);
  else if(k < 2.5) c = vec2(0.0, 1.0);
  else if(k < 3.5) c = vec2(0.0, 1.0);
  else if(k < 4.5) c = vec2(1.0, 0.0);
  else c = vec2(1.0, 1.0);
  float x = -1.0 + 2.0 * (band + c.x) / 3.0;
  gl_Position = vec4(x, c.y * 2.0 - 1.0, 0.0, 1.0);

  if(band < 0.5)
    v_color = vec4(texture(touch, vec2(0.25, 0.25)).rgb, 1.0);
  else if(band < 1.5)
    v_color = vec4(texture(touch, vec2(0.25, 0.75)).rgb, 1.0);
  else
    v_color = vec4(texelFetch(touch, ivec2(0, 0), 0).rgb, 1.0);
}
