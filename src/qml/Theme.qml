pragma Singleton
import QtQuick 6.0

QtObject {
    // Colors
    property color backgroundColor: "#121212"      // App background
    property color surfaceColor: "#1E1E1E"         // Card / Panel background
    property color primaryColor: "#4e9fdd"         // Accent purple
    property color secondaryColor: "#03DAC6"       // Accent teal
    property color textColor: "#FFFFFF"            // Main text
    property color textSecondary: "#B0B0B0"        // Disabled or secondary text
    property color borderColor: "#333333"          // Border lines
    property color iconColor: "#FFFFFF"            // Icons and interactive elements
    
    // Additional colors for input fields and buttons
    property color inputBackgroundColor: "#2D2D2D" // Input field background
    property color buttonColor: "#404040"          // Button background
    property color buttonPressedColor: "#505050"   // Button pressed state
    property color accentColor: "#4e9fdd"          // Accent color (same as primaryColor)
    property color accentPressedColor: "#3e8fcd"   // Pressed accent color

    // Device-scale factor (defaults to 1.0, scales on high DPI)
    // readonly property real scaleFactor: Screen.logicalPixelDensity / 96
    readonly property real scaleFactor: 1.0
    readonly property real iconScaleFactor: Screen.logicalPixelDensity / 96

    // Fonts
    property int fontSizeSmall: Math.round(12 * scaleFactor)
    property int fontSizeNormal: Math.round(14 * scaleFactor)
    property int fontSizeLarge: Math.round(18 * scaleFactor)

    // Spacing
    property int spacingSmall: Math.round(4 * scaleFactor)
    property int spacingNormal: Math.round(8 * scaleFactor)
    property int spacingLarge: Math.round(16 * scaleFactor)

    // Component sizes
    property int inputHeight: Math.round(32 * scaleFactor)
    property int cornerRadius: Math.round(6 * scaleFactor)

    // Button sizes
    property int buttonWidth: Math.round(100 * scaleFactor)
    property int buttonHeight: Math.round(30 * scaleFactor)

    // Slider handle size
    property int sliderHandleSize: Math.round(16 * scaleFactor)
    property int sliderHeight: Math.round(24 * scaleFactor)

    // Icon sizes
    property int iconSize: Math.round(24 * scaleFactor)

    // Switch sizes
    property int switchWidth: Math.round(30 * scaleFactor)
    property int switchHeight: Math.round(10 * scaleFactor)

    // ComboBox sizes
    property int comboBoxWidth: Math.round(70 * scaleFactor)
    property int comboBoxHeight: Math.round(30 * scaleFactor)
}
