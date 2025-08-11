#include "videoFormatUtils.h"

// Static member definitions
QList<VideoFormat> VideoFormatUtils::s_formats;
bool VideoFormatUtils::s_initialized = false;

void VideoFormatUtils::initializeFormats() {
    if (s_initialized) {
        return;
    }

    s_formats.clear();

    // Raw YUV formats - require explicit pixel format specification
    s_formats.append({"420P", "420P - YUV420P (Planar)", AV_PIX_FMT_YUV420P, FormatType::RAW_YUV});
    s_formats.append({"422P", "422P - YUV422P (Planar)", AV_PIX_FMT_YUV422P, FormatType::RAW_YUV});
    s_formats.append({"444P", "444P - YUV444P (Planar)", AV_PIX_FMT_YUV444P, FormatType::RAW_YUV});
    s_formats.append({"YUYV", "YUYV - YUV422 (Packed)", AV_PIX_FMT_YUYV422, FormatType::RAW_YUV});
    s_formats.append({"UYVY", "UYVY - YUV422 (Packed)", AV_PIX_FMT_UYVY422, FormatType::RAW_YUV});
    s_formats.append({"NV12", "NV12 - YUV420 (Semi-planar)", AV_PIX_FMT_NV12, FormatType::RAW_YUV});
    s_formats.append({"NV21", "NV21 - YUV420 (Semi-planar)", AV_PIX_FMT_NV21, FormatType::RAW_YUV});

    // Compressed formats - handled by FFmpeg decoder
    s_formats.append({"MP4", "MP4 - Compressed Video", AV_PIX_FMT_NONE, FormatType::COMPRESSED});
    s_formats.append({"AVI", "AVI - Compressed Video", AV_PIX_FMT_NONE, FormatType::COMPRESSED});
    s_formats.append({"MKV", "MKV - Compressed Video", AV_PIX_FMT_NONE, FormatType::COMPRESSED});
    s_formats.append({"MOV", "MOV - Compressed Video", AV_PIX_FMT_NONE, FormatType::COMPRESSED});
    s_formats.append({"WEBM", "WEBM - Compressed Video", AV_PIX_FMT_NONE, FormatType::COMPRESSED});
    s_formats.append({"HEVC", "HEVC - Compressed Video", AV_PIX_FMT_NONE, FormatType::COMPRESSED});
    s_formats.append({"H264", "H264 - Compressed Video", AV_PIX_FMT_NONE, FormatType::COMPRESSED});
    s_formats.append({"H265", "H265 - Compressed Video", AV_PIX_FMT_NONE, FormatType::COMPRESSED});

    s_initialized = true;
}

const QList<VideoFormat>& VideoFormatUtils::getSupportedFormats() {
    initializeFormats();
    return s_formats;
}

AVPixelFormat VideoFormatUtils::stringToPixelFormat(const QString& formatString) {
    initializeFormats();

    for (const VideoFormat& format : s_formats) {
        if (format.identifier.compare(formatString, Qt::CaseInsensitive) == 0) {
            return format.pixelFormat;
        }
    }

    return AV_PIX_FMT_NONE;
}

QString VideoFormatUtils::pixelFormatToString(AVPixelFormat format) {
    initializeFormats();

    for (const VideoFormat& videoFormat : s_formats) {
        if (videoFormat.pixelFormat == format) {
            return videoFormat.identifier;
        }
    }

    return "UNKNOWN";
}

QStringList VideoFormatUtils::getFormatIdentifiers() {
    initializeFormats();

    QStringList codes;
    for (const VideoFormat& format : s_formats) {
        codes.append(format.identifier);
    }

    return codes;
}

QStringList VideoFormatUtils::getDisplayNames() {
    initializeFormats();

    QStringList displayNames;
    for (const VideoFormat& format : s_formats) {
        displayNames.append(format.displayName);
    }

    return displayNames;
}

bool VideoFormatUtils::isValidFormat(const QString& formatString) {
    initializeFormats();

    for (const VideoFormat& format : s_formats) {
        if (format.identifier.compare(formatString, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

VideoFormat VideoFormatUtils::getFormatByIdentifier(const QString& identifier) {
    initializeFormats();

    for (const VideoFormat& format : s_formats) {
        if (format.identifier.compare(identifier, Qt::CaseInsensitive) == 0) {
            return format;
        }
    }

    // Return empty format if not found
    return {"", "", AV_PIX_FMT_NONE};
}

bool VideoFormatUtils::isCompressedFormat(const QString& formatString) {
    initializeFormats();

    for (const VideoFormat& format : s_formats) {
        if (format.identifier.compare(formatString, Qt::CaseInsensitive) == 0) {
            return format.type == FormatType::COMPRESSED;
        }
    }

    return false;
}

FormatType VideoFormatUtils::getFormatType(const QString& formatString) {
    initializeFormats();

    for (const VideoFormat& format : s_formats) {
        if (format.identifier.compare(formatString, Qt::CaseInsensitive) == 0) {
            return format.type;
        }
    }

    return FormatType::RAW_YUV; // Default assumption
}

QString VideoFormatUtils::detectFormatFromExtension(const QString& filename) {
    QString lowerFilename = filename.toLower();

    if (lowerFilename.endsWith(".yuv") || lowerFilename.endsWith(".y4m")) {
        return "420P"; // Default YUV format
    } else if (lowerFilename.endsWith(".mp4")) {
        return "MP4";
    } else if (lowerFilename.endsWith(".avi")) {
        return "AVI";
    } else if (lowerFilename.endsWith(".mkv")) {
        return "MKV";
    } else if (lowerFilename.endsWith(".mov")) {
        return "MOV";
    } else if (lowerFilename.endsWith(".webm")) {
        return "WEBM";
    } else if (lowerFilename.endsWith(".hevc") || lowerFilename.endsWith(".265")) {
        return "HEVC";
    } else if (lowerFilename.endsWith(".264")) {
        return "H264";
    }

    return "MP4"; // Default for unknown compressed formats
}