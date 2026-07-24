/*{
  "DESCRIPTION": "Tests CSF image resource with R32F format. Writes single-channel float data to a 256x256 R32F image, then reads it in a second ISF shader. If R32F format works: output shows a grayscale radial gradient. If format is wrong: garbled or black. Connect to '3d-slice-viewer' or any ISF that reads an image.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "CATEGORIES": ["TEST-IMAGE-FORMAT"],
  "RESOURCES": [
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "r32f", "WIDTH": "256", "HEIGHT": "256" }
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
    float dist = length(uv - 0.5) * 2.0;

    // Pulsing radial gradient
    float val = 1.0 - smoothstep(0.0, 1.0, dist);
    val *= 0.5 + 0.5 * sin(TIME * 2.0);

    imageStore(outputImage, pos, vec4(val, 0.0, 0.0, 1.0));
}
