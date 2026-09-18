// Standard-Z counterpart of rr-sinkdepth-greater.vs.
void main()
{
    vec4 clipPos = MODEL_MATRIX * position;
    clipPos.z = -clipPos.z;
    gl_Position = clipSpaceCorrMatrix * clipPos;
    gl_PointSize = 2.0;
}
