// Copyright (c) 2017-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#version 450

#pragma shader_stage(vertex)
#pragma vertex

// mvp matrix as a pushconst
layout (std140, push_constant) uniform buf
{
    mat4 mvp;
} ubuf;

// vertex and color info
layout (location = 0) in vec3 Position;
layout (location = 1) in vec3 Color;

layout (location = 0) out vec4 oColor;

// output for the fragment shader
out gl_PerVertex
{
    vec4 gl_Position;
};


void main()
{
    oColor.rgb  = Color.rgb;
    oColor.a  = 1.0;
    gl_Position = ubuf.mvp * vec4(Position, 1);
}
