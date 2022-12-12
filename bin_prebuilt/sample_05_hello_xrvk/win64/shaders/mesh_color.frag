// Copyright (c) 2017-2022, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#version 450

#pragma shader_stage(fragment)
#pragma fragment

layout (location = 0) in vec4 oColor;
layout (location = 0) out vec4 FragColor;

void main()
{
    FragColor = oColor;
}
