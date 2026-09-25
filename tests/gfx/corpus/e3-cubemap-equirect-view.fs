/*{
  "DESCRIPTION": "Equirectangular unwrap of a cubemap with the convention of csf-examples presets/rasterizers/cubemap_view.fs: the centre looks toward -Z, +X is a quarter turn to the right, the top edge is +Y.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-CUBEMAP"],
  "INPUTS": [
    { "NAME": "skybox", "TYPE": "cubemap" }
  ]
}*/

void main()
{
    const float pi = 3.14159265358979323846;
    vec2 uv = isf_FragNormCoord;
    float theta = (uv.x * 2.0 - 1.0) * pi;
    float phi = (uv.y - 0.5) * pi;
    vec3 dir = vec3(cos(phi) * sin(theta), sin(phi), -cos(phi) * cos(theta));
    gl_FragColor = vec4(textureLod(skybox, normalize(dir), 0.0).rgb, 1.0);
}
