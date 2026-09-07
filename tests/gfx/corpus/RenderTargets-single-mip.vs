void main() { vec2 p=vec2(float((gl_VertexIndex << 1) & 2),float(gl_VertexIndex & 2)); gl_Position=vec4(p*2.-1.,0.,1.); }
