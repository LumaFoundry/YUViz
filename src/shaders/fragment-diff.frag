#version 440

layout(location = 0) in vec2 v_texCoord;
layout(location = 0) out vec4 fragColor;

// two Y texture represent 2 frames
layout(binding = 1) uniform sampler2D y_tex_frame1;
layout(binding = 2) uniform sampler2D y_tex_frame2;

// Diff configuration uniform block
layout(binding = 4) uniform DiffConfigParams {
    int displayMode;    // 0=Grayscale Classic, 1=Heatmap, 2=Binary
    float diffMultiplier; // diff multiplier
    int diffMethod;     // 0=Direct Subtraction, 1=Squared Difference, 2=Normalized, 3=Absolute Difference
    int padding;        // Alignment to 16-byte boundary
};

// Viridis color mapping function
vec3 viridis(float t) {
    // Viridis color mapping: from dark blue to yellow-green
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

    // Calculate difference based on configuration
    float diff;
    switch (diffMethod) {
    case 0: // Direct subtraction
        diff = y1 - y2;
        break;
    case 1: // Squared difference
        diff = (y1 - y2) * (y1 - y2);
        break;
    case 2: // Normalized
        diff = y1 > 0.0 ? (y1 - y2) / y1 : 0.0;
        break;
    case 3: // Absolute difference
        diff = abs(y1 - y2);
        break;
    default:
        diff = y1 - y2;
        break;
    }

    // Apply multiplier
    diff *= diffMultiplier;

    vec3 color;
    if (displayMode == 0) {
        // Grayscale classic mode
        float gray = 0.5 + diff;
        gray = clamp(gray, 0.0, 1.0);
        color = vec3(gray, gray, gray);
    } else if (displayMode == 1) {
        // Heatmap mode
        if (diffMethod == 1 || diffMethod == 3) {
            // Absolute or squared diff: use viridis single-color gradient
            float normalizedDiff = clamp(diff, 0.0, 1.0);
            color = viridis(normalizedDiff);
        } else {
            // Other diff methods: use blue-white-red gradient
            vec3 blueColor = vec3(0.0, 0.0, 1.0);   // Blue
            vec3 whiteColor = vec3(1.0, 1.0, 1.0);  // White
            vec3 redColor = vec3(1.0, 0.0, 0.0);    // Red
            
            // Calculate difference intensity, map to [-1, 1] range
            float normalizedDiff = clamp(diff, -1.0, 1.0);
            
            if (normalizedDiff < 0.0) {
                // Negative difference: blue to white
                color = mix(blueColor, whiteColor, 1.0 + normalizedDiff);
            } else {
                // Positive difference: white to red
                color = mix(whiteColor, redColor, normalizedDiff);
            }
        }
    } else {
        // Binary mode: white for differences, black for no difference
        float threshold = 0.01; // Threshold for considering a difference
        if (abs(diff) > threshold) {
            color = vec3(1.0, 1.0, 1.0); // White for differences
        } else {
            color = vec3(0.0, 0.0, 0.0); // Black for no difference
        }
    }
    
    // Ensure color is within valid range
    color = clamp(color, 0.0, 1.0);

    fragColor = vec4(color, 1.0);
}
