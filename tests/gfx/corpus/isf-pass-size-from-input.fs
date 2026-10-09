/*{
  "DESCRIPTION": "Pass 0 is $size x $size; pass 1 paints that pass's width / 255 in red. Used by GfxSizeExpressionInput.",
  "CREDIT": "test",
  "ISFVSN": "2",
  "INPUTS": [ { "NAME": "size", "TYPE": "long", "DEFAULT": 16, "MIN": 1, "MAX": 64 } ],
  "PASSES": [
    { "TARGET": "sized", "WIDTH": "$size", "HEIGHT": "$size" },
    {}
  ]
}*/
void main()
{
  if(PASSINDEX == 0)
    gl_FragColor = vec4(1.0);
  else
    gl_FragColor = vec4(float(IMG_SIZE(sized).x) / 255.0, 0.0, 0.0, 1.0);
}
