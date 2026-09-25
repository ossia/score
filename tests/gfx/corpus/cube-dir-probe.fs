/*{
  "DESCRIPTION": "samplerCube probe for direction-coded cubes: 24 columns, four off-centre directions per face (face = column / 4 in the order +X -X +Y -Y +Z -Z). Column c samples axis + 0.55 * s1 * T1 + 0.3 * s2 * T2 with s1 = +-1 from bit 0 and s2 = +-1 from bit 1 of c % 4, T1/T2 = Y/Z for the X faces, X/Z for the Y faces, X/Y for the Z faces. A face stored mirrored along s or t returns the encoding of a different direction.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-CUBEMAP"],
  "INPUTS": [
    { "NAME": "probe", "TYPE": "cubemap" }
  ]
}*/

void main()
{
    int c = int(min(floor(isf_FragNormCoord.x * 24.0), 23.0));
    int face = c / 4;
    int q = c % 4;
    float s1 = (q & 1) != 0 ? 1.0 : -1.0;
    float s2 = (q & 2) != 0 ? 1.0 : -1.0;
    float sg = (face % 2) == 0 ? 1.0 : -1.0;

    vec3 dir;
    if(face < 2)      dir = vec3(sg, 0.55 * s1, 0.3 * s2);
    else if(face < 4) dir = vec3(0.55 * s1, sg, 0.3 * s2);
    else              dir = vec3(0.55 * s1, 0.3 * s2, sg);

    gl_FragColor = textureLod(probe, dir, 0.0);
}
