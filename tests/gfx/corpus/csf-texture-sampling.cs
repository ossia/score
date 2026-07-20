/*{
  "DESCRIPTION": "Tests CSF texture input (sampler2D in compute shader). Pipeline: isf-solid-color.fs -> this(inputTex) -> Window (via the outputImage). Reads an upstream texture via texture() and writes inverted colors to an output image. If texture sampling works in compute: output shows inverted input. If broken: output is black or garbage.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-TEXTURE"],
  "RESOURCES": [
    { "NAME": "inputTex", "TYPE": "texture" },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "WIDTH": "512", "HEIGHT": "512" }
  ],
  "PASSES": [
    { "LOCAL_SIZE": [16, 16, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE" } }
  ]
}*/

void main()
{
    ivec2 pos = ivec2(gl_GlobalInvocationID.xy);
    ivec2 size = imageSize(outputImage);
    if(pos.x >= size.x || pos.y >= size.y)
        return;

    vec2 uv = (vec2(pos) + 0.5) / vec2(size);

    // Sample the upstream texture
    vec4 color = texture(inputTex, uv);

    // Invert colors as proof we actually read meaningful data
    vec4 inverted = vec4(1.0 - color.rgb, color.a);

    imageStore(outputImage, pos, inverted);
}
