#version 440

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec2 v_texCoord;

layout(std140, binding = 0) uniform MVP { 
    mat4 mvp;
};

void main() {
    gl_Position = mvp * vec4(position, 0.0, 1.0);
    v_texCoord = texCoord;
} 