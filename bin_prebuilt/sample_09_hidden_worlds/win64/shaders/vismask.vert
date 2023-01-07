#version 450

#pragma shader_stage(vertex)
#pragma vertex

// mvp matrix as a pushconst
layout (std140, push_constant) uniform buf
{
    mat4 mvp;
} ubuf;

// vertex and color info
layout (location = 0) in vec2 Position;

void main()
{
    gl_Position = ubuf.mvp * vec4(Position, 0, 1);
}
