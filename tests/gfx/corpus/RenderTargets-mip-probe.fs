/*{
  "ISFVSN": "2.0",
  "INPUTS": [
    {
      "NAME": "image",
      "TYPE": "image",
      "STATIC": true,
      "SAMPLER": { "MIPMAP_MODE": "nearest" }
    }
  ]
}*/
void main(){ gl_FragColor=textureLod(image,vec2(.5),7.); }
