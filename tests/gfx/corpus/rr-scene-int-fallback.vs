void main()
{
    isf_vertShaderInit();
    v_joints = joints_0;
    gl_Position = vec4(position.xy, 0.0, 1.0);
    isf_vertShaderFinish();
}
