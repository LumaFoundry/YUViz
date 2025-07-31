#version 440

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D y_tex;
layout(binding = 2) uniform sampler2D u_tex;
layout(binding = 3) uniform sampler2D v_tex;

// Color space and color range uniform block
layout(binding = 4) uniform ColorSpaceParams {
    int colorSpace; // AVColorSpace: 1=BT.709, 5=BT.470BG, 6=SMPTE170M, 9=BT.2020_NCL,
                    // 10=BT.2020_CL
    int colorRange; // AVColorRange: 1=MPEG (16-235), 2=JPEG (0-255)
    int componentDisplayMode; // 0=RGB, 1=Y only, 2=U only, 3=V only
    int padding; // Alignment to 16-byte boundary
};

void main() {
    vec2 texCoord = v_texCoord;
    float y = texture(y_tex, texCoord).r;
    float u = texture(u_tex, texCoord).r;
    float v = texture(v_tex, texCoord).r;

    // Normalize based on color range
    if (colorRange == 1) {
        // MPEG Range (16-235 for Y, 16-240 for U/V)
        y = (y - 16.0 / 255.0) / (235.0 / 255.0 - 16.0 / 255.0);
        u = (u - 16.0 / 255.0) / (240.0 / 255.0 - 16.0 / 255.0);
        v = (v - 16.0 / 255.0) / (240.0 / 255.0 - 16.0 / 255.0);
    } else {
        // JPEG Range (0-255) - Use directly, no normalization needed
        y = y;
        u = u;
        v = v;
    }

    // Convert U/V from [0,1] to [-0.5, 0.5]
    u = u - 0.5;
    v = v - 0.5;

    // Handle component display modes
    if (componentDisplayMode == 1) {
        // Y only - display as grayscale
        fragColor = vec4(y, y, y, 1.0);
        return;
    } else if (componentDisplayMode == 2) {
        // U only - display as blue-green gradient
        // Convert U from [-0.5, 0.5] to [0, 1] for color mapping
        float u_normalized = u + 0.5;
        // Blue-green gradient: green (0) -> cyan (0.5) -> blue (1)
        vec3 rgb = vec3(0.0, 1.0 - u_normalized, u_normalized);
        fragColor = vec4(rgb, 1.0);
        return;
    } else if (componentDisplayMode == 3) {
        // V only - display as green-red gradient
        // Convert V from [-0.5, 0.5] to [0, 1] for color mapping
        float v_normalized = v + 0.5;
        // Green-red gradient: green (0) -> yellow (0.5) -> red (1)
        vec3 rgb = vec3(v_normalized, 1.0 - v_normalized, 0.0);
        fragColor = vec4(rgb, 1.0);
        return;
    }

    // Default RGB conversion
    // Select conversion matrix based on color space (using FFmpeg standard enum values)
    vec3 rgb;
    switch (colorSpace) {
    case 1: // AVCOL_SPC_BT709 (HDTV, 1080p) - Default scheme
        rgb = vec3(y + 1.5748 * v, y - 0.1873 * u - 0.4681 * v, y + 1.8556 * u);
        break;
    case 5: // AVCOL_SPC_BT470BG (PAL)
    case 6: // AVCOL_SPC_SMPTE170M (NTSC)
        rgb = vec3(y + 1.402 * v, y - 0.344 * u - 0.714 * v, y + 1.772 * u);
        break;
    case 9:  // AVCOL_SPC_BT2020_NCL (UHDTV 4K/8K)
    case 10: // AVCOL_SPC_BT2020_CL (UHDTV 4K/8K)
        rgb = vec3(y + 1.4746 * v, y - 0.1646 * u - 0.5714 * v, y + 1.8814 * u);
        break;
    default: // Default to BT.709
        rgb = vec3(y + 1.5748 * v, y - 0.1873 * u - 0.4681 * v, y + 1.8556 * u);
        break;
    }

    // Clamp RGB values to [0,1] range
    rgb = clamp(rgb, 0.0, 1.0);

    fragColor = vec4(rgb, 1.0);
}