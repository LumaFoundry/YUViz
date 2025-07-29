#version 440

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

// two Y texture represent 2 frames
layout(binding = 1) uniform sampler2D y_tex_frame1;
layout(binding = 2) uniform sampler2D y_tex_frame2;

void main() {
    vec2 texCoord = v_texCoord;
    
    float y1 = texture(y_tex_frame1, texCoord).r;
    float y2 = texture(y_tex_frame2, texCoord).r;
    
    float diff = y1 - y2;
    
    // map to grey only
    // 0.5 (grey) for same Y
    // 1.0 (white) y1 > y2
    // 0.0 (black) y1 < y2
    float gray = 0.5 + diff * 0.5;
    
    gray = clamp(gray, 0.0, 1.0);
    
    fragColor = vec4(gray, gray, gray, 1.0);
}
