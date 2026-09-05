/*{
  "DESCRIPTION": "samplerCube viewer for syn-cube-six-colors: divides the output into a 3x2 grid and samples one cube face axis per cell, so a test can probe every face of the upstream cubemap, each exactly once. Cell -> direction: top row +X, -X, +Y; bottom row -Y, +Z, -Z (QRhi/GL face order 0..5 reading the grid left-to-right, top-to-bottom). Face centres of solid-colour faces are orientation-proof: texture(probe, axis) hits the centre of that axis' face under every backend's cube sampling rules. Same fixture pattern as csf-cube-image-read.fs, extended from 4 quadrant probes to all 6 faces.",
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
    int col = int(min(floor(uv.x * 3.0), 2.0));
    int row = uv.y < 0.5 ? 0 : 1;
    int face = row * 3 + col;

    vec3 dir;
    if(face == 0)      dir = vec3( 1.0,  0.0,  0.0); // +X  face 0
    else if(face == 1) dir = vec3(-1.0,  0.0,  0.0); // -X  face 1
    else if(face == 2) dir = vec3( 0.0,  1.0,  0.0); // +Y  face 2
    else if(face == 3) dir = vec3( 0.0, -1.0,  0.0); // -Y  face 3
    else if(face == 4) dir = vec3( 0.0,  0.0,  1.0); // +Z  face 4
    else               dir = vec3( 0.0,  0.0, -1.0); // -Z  face 5

    gl_FragColor = texture(probe, dir);
}
