/*{
  "ISFVSN": "2.0",
  "INPUTS": [
    {
      "NAME": "a",
      "TYPE": "image",
      "STATIC": true
    },
    {
      "NAME": "b",
      "TYPE": "image",
      "STATIC": true
    }
  ]
}*/
void main(){ gl_FragColor=vec4(texture(a,vec2(.5)).r,texture(b,vec2(.5)).r,0.,1.); }
