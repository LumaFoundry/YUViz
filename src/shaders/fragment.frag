#version 440

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D y_tex;
layout(binding = 2) uniform sampler2D u_tex;
layout(binding = 3) uniform sampler2D v_tex;

void main() {
    vec2 texCoord = v_texCoord;
    float y = texture(y_tex, texCoord).r;
    float u = texture(u_tex, texCoord).r;
    float v = texture(v_tex, texCoord).r;
    
    y = (y - 16.0/255.0) / (235.0/255.0 - 16.0/255.0);
    u = (u - 16.0/255.0) / (240.0/255.0 - 16.0/255.0);
    v = (v - 16.0/255.0) / (240.0/255.0 - 16.0/255.0);
    
    u = u - 0.5;
    v = v - 0.5;
    
    // BT.709 YUV to RGB conversion
    float r = y + 1.5748 * v;
    float g = y - 0.1873 * u - 0.4681 * v;
    float b = y + 1.8556 * u;
    
    vec3 rgb = vec3(r, g, b);
    rgb = clamp(rgb, 0.0, 1.0);
    
    fragColor = vec4(rgb, 1.0);
} 