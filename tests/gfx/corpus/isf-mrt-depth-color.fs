/*{
  "DESCRIPTION": "Tests ISF OUTPUTS with both 'color' and 'depth' types simultaneously. Outputs a gradient color and writes depth based on brightness. Connect the depth output to isf-image-depth.fs to verify. If both output types work: color port shows gradient, depth port shows grayscale depth. If depth output broken: depth is all zeros.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-MRT"],
  "INPUTS": [
    { "NAME": "depthScale", "TYPE": "float", "DEFAULT": 1.0, "MIN": 0.0, "MAX": 1.0 }
  ],
  "OUTPUTS": [
    { "NAME": "colorOut", "TYPE": "color" },
    { "NAME": "depthOut", "TYPE": "depth" }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord;

    // Radial gradient color
    float d = length(uv - 0.5);
    vec3 col = mix(vec3(1.0, 0.8, 0.2), vec3(0.1, 0.2, 0.8), d * 2.0);

    colorOut = vec4(col, 1.0);

    // Depth: brighter = closer (lower depth value)
    float brightness = dot(col, vec3(0.299, 0.587, 0.114));
    depthOut = (1.0 - brightness) * depthScale;
}
