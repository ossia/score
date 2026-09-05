void main()
{
    mat4 M = instance_transforms.data[gl_InstanceIndex];
    gl_Position = clipSpaceCorrMatrix * camera.viewProjection * M * vec4(position, 1.0);
    gl_PointSize = 2.0;
}
