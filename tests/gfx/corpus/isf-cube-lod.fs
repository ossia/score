/*{
  "DESCRIPTION": "Samples the cubemap input with textureLod at the given mip level along +X, like cubemap_view.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST"],
  "INPUTS": [
    { "NAME": "skybox", "TYPE": "cubemap" },
    { "NAME": "mip", "TYPE": "float", "MIN": 0.0, "MAX": 10.0, "DEFAULT": 0.0 }
  ]
}*/

void main()
{
    gl_FragColor = vec4(textureLod(skybox, vec3(1.0, 0.0, 0.0), mip).rgb, 1.0);
}
