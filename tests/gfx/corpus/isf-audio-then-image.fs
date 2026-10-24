/*{
  "DESCRIPTION": "Sampler order: an audio INPUT declared before an image INPUT. libisf binds the samplers in declaration order, wave then inputImage; the renderer must bind the textures in the same order. Feed a constant audio buffer and a solid green image: the output MUST be the image's green. Bound in the wrong order, inputImage samples the audio texture and the output is the audio gray.",
  "CREDIT": "test",
  "ISFVSN": "2.0",
  "CATEGORIES": ["TEST-AUDIO"],
  "INPUTS": [
    { "NAME": "wave", "TYPE": "audio", "MAX": 256 },
    { "NAME": "inputImage", "TYPE": "image" }
  ]
}*/

void main()
{
    gl_FragColor = IMG_NORM_PIXEL(inputImage, isf_FragNormCoord);
}
