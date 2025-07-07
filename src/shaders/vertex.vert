#version 440

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec2 v_texCoord;

layout(std140, binding = 5) uniform ResizeParams {
    vec2 u_scale;
    vec2 u_offset;
};

void main() {
    gl_Position = vec4(position * u_scale, 0.0, 1.0);
    v_texCoord = texCoord;
} 