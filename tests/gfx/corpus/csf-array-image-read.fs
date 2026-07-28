/*{
  "DESCRIPTION": "Samples a sampler2DArray in four quadrants at fixed layers so a test can assert individual layers are non-black: TL=layer 0 (written even by the buggy path), TR=layer 1, BL=layer 2, BR=layer 3. Pairs with csf-array-image-write.cs.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-ARRAY"],
  "INPUTS": [
    { "NAME": "layers", "TYPE": "image", "IS_ARRAY": true }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;
    float layer;
    if(uv.x < 0.5 && uv.y < 0.5)
        layer = 0.0;
    else if(uv.x >= 0.5 && uv.y < 0.5)
        layer = 1.0;
    else if(uv.x < 0.5 && uv.y >= 0.5)
        layer = 2.0;
    else
        layer = 3.0;

    vec2 cell = fract(uv * 2.0);
    gl_FragColor = texture(layers, vec3(cell, layer));
}
