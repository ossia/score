/*{
  "DESCRIPTION": "Samples a cubemap in four quadrants along fixed directions so a test can assert individual faces are non-black: TL=+X (face 0, written even by the buggy path), TR=-X (face 1), BL=+Y (face 2), BR=+Z (face 4). Pairs with csf-cube-image-write.cs.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-CUBEMAP"],
  "INPUTS": [
    { "NAME": "probe", "TYPE": "cubemap" }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;
    vec3 dir;
    if(uv.x < 0.5 && uv.y < 0.5)
        dir = vec3(1.0, 0.0, 0.0);   // +X  face 0
    else if(uv.x >= 0.5 && uv.y < 0.5)
        dir = vec3(-1.0, 0.0, 0.0);  // -X  face 1
    else if(uv.x < 0.5 && uv.y >= 0.5)
        dir = vec3(0.0, 1.0, 0.0);   // +Y  face 2
    else
        dir = vec3(0.0, 0.0, 1.0);   // +Z  face 4

    gl_FragColor = texture(probe, dir);
}
