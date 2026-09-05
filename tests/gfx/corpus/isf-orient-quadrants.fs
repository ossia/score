/*{
  "DESCRIPTION": "Orientation oracle: four differently coloured quadrants, so that a vertical flip, a horizontal flip, a transpose and a 180 degree rotation are all distinguishable from each other and from the identity. ISF places the origin of isf_FragNormCoord at the BOTTOM-left, so isf_FragNormCoord.y > 0.5 is the TOP half of the delivered image. Delivered picture, row 0 first: red top-left, green top-right, blue bottom-left, white bottom-right. A single-corner marker cannot tell a transpose from a rotation, and a gradient cannot tell a mirror from the identity along the constant axis; four keyed quadrants pin all of them.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-ORIENTATION"],
  "INPUTS": []
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

    gl_FragColor = vec4(c, 1.0);
}
