/*{
  "DESCRIPTION": "Lays out up to four inputs in a 2x2 grid. Each tile has a gain, for inputs whose values are not in [0,1] (raw depth, logits). Single-channel textures (R8 / R32F masks and depth maps) show up red.",
  "CREDIT": "ossia score",
  "ISFVSN": "2.0",
  "CATEGORIES": ["Utility"],
  "INPUTS": [
    { "NAME": "topLeft",     "TYPE": "image" },
    { "NAME": "topRight",    "TYPE": "image" },
    { "NAME": "bottomLeft",  "TYPE": "image" },
    { "NAME": "bottomRight", "TYPE": "image" },
    { "NAME": "gap", "LABEL": "Gap", "TYPE": "float", "DEFAULT": 0.01, "MIN": 0.0, "MAX": 0.1 },
    { "NAME": "gainTopLeft",     "LABEL": "Gain top left",     "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 10.0 },
    { "NAME": "gainTopRight",    "LABEL": "Gain top right",    "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 10.0 },
    { "NAME": "gainBottomLeft",  "LABEL": "Gain bottom left",  "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 10.0 },
    { "NAME": "gainBottomRight", "LABEL": "Gain bottom right", "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 10.0 }
  ]
}*/

void main()
{
  vec2 uv = isf_FragNormCoord;

  // Tile coordinates, then shrink each tile by the gap.
  vec2 local = fract(uv * 2.0);
  float half_gap = gap;
  if(any(lessThan(local, vec2(half_gap))) || any(greaterThan(local, vec2(1.0 - half_gap))))
  {
    gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    return;
  }
  local = (local - half_gap) / (1.0 - 2.0 * half_gap);

  bool left = uv.x < 0.5;
  bool top = uv.y >= 0.5;

  vec4 c;
  if(top && left)
    c = IMG_NORM_PIXEL(topLeft, local) * gainTopLeft;
  else if(top)
    c = IMG_NORM_PIXEL(topRight, local) * gainTopRight;
  else if(left)
    c = IMG_NORM_PIXEL(bottomLeft, local) * gainBottomLeft;
  else
    c = IMG_NORM_PIXEL(bottomRight, local) * gainBottomRight;

  gl_FragColor = vec4(c.rgb, 1.0);
}
