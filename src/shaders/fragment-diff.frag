#version 440

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

// two Y texture represent 2 frames
layout(binding = 1) uniform sampler2D y_tex_frame1;
layout(binding = 2) uniform sampler2D y_tex_frame2;

// Diff configuration uniform block
layout(binding = 4) uniform DiffConfigParams {
    int displayMode;    // 0=黑白灰经典模式, 1=热力图模式
    float diffMultiplier; // diff放大倍率
    int diffMethod;     // 0=直接相减(y1-y2), 1=差的平方, 2=归一化(y1-y2)/y1, 3=差的绝对值
    int padding;        // 对齐到16字节边界
};

// Viridis颜色映射函数
vec3 viridis(float t) {
    // Viridis颜色映射：从深蓝到黄绿
    vec3 c0 = vec3(0.267004, 0.004874, 0.329415);
    vec3 c1 = vec3(0.127568, 0.566949, 0.550556);
    vec3 c2 = vec3(0.369214, 0.788888, 0.382914);
    vec3 c3 = vec3(0.992156, 0.803922, 0.145098);
    
    return mix(mix(c0, c1, t), mix(c2, c3, t), smoothstep(0.0, 1.0, t));
}

void main() {
    vec2 texCoord = v_texCoord;

    float y1 = texture(y_tex_frame1, texCoord).r;
    float y2 = texture(y_tex_frame2, texCoord).r;

    // 根据配置计算差异
    float diff;
    switch (diffMethod) {
    case 0: // 直接相减
        diff = y1 - y2;
        break;
    case 1: // 差的平方
        diff = (y1 - y2) * (y1 - y2);
        break;
    case 2: // 归一化
        diff = y1 > 0.0 ? (y1 - y2) / y1 : 0.0;
        break;
    case 3: // 差的绝对值
        diff = abs(y1 - y2);
        break;
    default:
        diff = y1 - y2;
        break;
    }

    // 应用放大倍率
    diff *= diffMultiplier;

    vec3 color;
    if (displayMode == 0) {
        // 黑白灰经典模式
        float gray = 0.5 + diff * 4.0;
        gray = clamp(gray, 0.0, 1.0);
        color = vec3(gray, gray, gray);
    } else {
        // 热力图模式
        if (diffMethod == 1 || diffMethod == 3) {
            // 绝对值或平方diff：使用viridis单色渐变
            float normalizedDiff = clamp(diff, 0.0, 1.0);
            color = viridis(normalizedDiff);
        } else {
            // 其他diff方法：使用蓝白红渐变
            vec3 blueColor = vec3(0.0, 0.0, 1.0);   // 蓝色
            vec3 whiteColor = vec3(1.0, 1.0, 1.0);  // 白色
            vec3 redColor = vec3(1.0, 0.0, 0.0);    // 红色
            
            // 计算差异强度，映射到[-1, 1]范围
            float normalizedDiff = clamp(diff, -1.0, 1.0);
            
            if (normalizedDiff < 0.0) {
                // 负差异：蓝色到白色
                color = mix(blueColor, whiteColor, 1.0 + normalizedDiff);
            } else {
                // 正差异：白色到红色
                color = mix(whiteColor, redColor, normalizedDiff);
            }
        }
    }
    
    // 确保颜色在有效范围内
    color = clamp(color, 0.0, 1.0);

    fragColor = vec4(color, 1.0);
}
