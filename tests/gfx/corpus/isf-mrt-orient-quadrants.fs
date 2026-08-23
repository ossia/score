/*{
  "DESCRIPTION": "Same four-quadrant orientation key as isf-orient-quadrants.fs, emitted through two declared OUTPUTS so the node takes the MRT path (colorCount > 1). The MRT path reaches its destination through a blit of the intermediate attachment rather than drawing into it directly, so it is the one that can turn the picture over on the way. Both attachments must carry the key the same way up as the single-output path, on every backend.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-ORIENTATION", "TEST-MRT"],
  "INPUTS": [],
  "OUTPUTS": [
    { "NAME": "outA" },
    { "NAME": "outB" }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;
    bool right = uv.x > 0.5;
    bool top = uv.y > 0.5;

    vec3 c;
    if(top && !right)
        c = vec3(1.0, 0.0, 0.0);   // top-left     red
    else if(top && right)
        c = vec3(0.0, 1.0, 0.0);   // top-right    green
    else if(!top && !right)
        c = vec3(0.0, 0.0, 1.0);   // bottom-left  blue
    else
        c = vec3(1.0, 1.0, 1.0);   // bottom-right white

    outA = vec4(c, 1.0);
    outB = vec4(c, 1.0);
}
