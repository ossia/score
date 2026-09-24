/*{
  "DESCRIPTION": "Samples an audio waveform input and stores its centre texel in red. The audio input comes before the storage image, so the image binding only lines up when the audio sampler takes its slot.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "MODE": "COMPUTE_SHADER",
  "RESOURCES": [
    { "NAME": "wave", "TYPE": "audio" },
    { "NAME": "outputImage", "TYPE": "image", "ACCESS": "write_only", "FORMAT": "RGBA8", "WIDTH": "64", "HEIGHT": "64" }
  ],
  "PASSES": [ { "LOCAL_SIZE": [8, 8, 1], "EXECUTION_MODEL": { "TYPE": "2D_IMAGE", "TARGET": "outputImage" } } ]
}*/
void main()
{
    ivec2 p = ivec2(gl_GlobalInvocationID.xy);
    if (p.x >= imageSize(outputImage).x || p.y >= imageSize(outputImage).y) return;
    float v = textureLod(wave, vec2(0.5, 0.5), 0.0).r;
    IMG_STORE(outputImage, p, vec4(v, 0.0, 1.0, 1.0));
}
