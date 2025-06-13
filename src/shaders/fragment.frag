#version 440

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D y_tex;
layout(binding = 2) uniform sampler2D u_tex;
layout(binding = 3) uniform sampler2D v_tex;

// BT.709 YUV to RGB conversion matrix
const mat3 yuv2rgb = mat3(
    1.0,  0.0,       1.5748,    // R = Y + 1.5748 * V
    1.0, -0.1873,   -0.4681,    // G = Y - 0.1873 * U - 0.4681 * V
    1.0,  1.8556,    0.0        // B = Y + 1.8556 * U
);

void main() {
    // 读取YUV值
    float y = texture(y_tex, v_texCoord).r;
    float u = texture(u_tex, v_texCoord).r - 0.5;
    float v = texture(v_tex, v_texCoord).r - 0.5;
    
    // 应用BT.709转换矩阵
    vec3 rgb = yuv2rgb * vec3(y, u, v);
    
    // MPEG色彩范围（16-235 for Y, 16-240 for UV）
    rgb = (rgb - 16.0/255.0) * (255.0/219.0);
    
    // 确保值在有效范围内
    rgb = clamp(rgb, 0.0, 1.0);
    
    fragColor = vec4(rgb, 1.0);
} 