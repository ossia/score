/*{
  "DESCRIPTION": "Displays a slice of a 3D volume texture. Connect a 3D texture producer (e.g. '3d-generate-volume') to the 'volume' input. Use 'sliceZ' to scrub through depth slices. Should show a circle cross-section that grows then shrinks as sliceZ goes 0->0.5->1.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-3D"],
  "INPUTS": [
    { "NAME": "volume", "TYPE": "image", "DIMENSIONS": 3 },
    { "NAME": "sliceZ", "TYPE": "float", "DEFAULT": 0.5, "MIN": 0.0, "MAX": 1.0 }
  ]
}*/

void main(void)
{
    vec2 uv = isf_FragNormCoord;
    gl_FragColor = texture(volume, vec3(uv, sliceZ));
}
