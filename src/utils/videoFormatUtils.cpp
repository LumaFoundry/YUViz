#include "videoFormatUtils.h"

// Static member definitions
QList<VideoFormat> VideoFormatUtils::s_formats;
QStringList VideoFormatUtils::s_rawVideoExtensions;
bool VideoFormatUtils::s_initialized = false;

void VideoFormatUtils::initializeFormats() {
    if (s_initialized) {
        return;
    }

    s_formats.clear();

    s_formats.append({"420P", "420P - YUV420P (Planar)", AV_PIX_FMT_YUV420P, FormatType::RAW_YUV});
    s_formats.append({"422P", "422P - YUV422P (Planar)", AV_PIX_FMT_YUV422P, FormatType::RAW_YUV});
    s_formats.append({"444P", "444P - YUV444P (Planar)", AV_PIX_FMT_YUV444P, FormatType::RAW_YUV});
    s_formats.append({"YUYV", "YUYV - YUV422 (Packed)", AV_PIX_FMT_YUYV422, FormatType::RAW_YUV});
    s_formats.append({"UYVY", "UYVY - YUV422 (Packed)", AV_PIX_FMT_UYVY422, FormatType::RAW_YUV});
    s_formats.append({"NV12", "NV12 - YUV420 (Semi-planar)", AV_PIX_FMT_NV12, FormatType::RAW_YUV});
    s_formats.append({"NV21", "NV21 - YUV420 (Semi-planar)", AV_PIX_FMT_NV21, FormatType::RAW_YUV});
    s_formats.append({"COMPRESSED", "Compressed Video", AV_PIX_FMT_NONE, FormatType::COMPRESSED});

    s_rawVideoExtensions = {".yuv", ".y4m", ".raw", ".nv12", ".nv21", ".yuyv", ".uyvy"};

    s_initialized = true;
}

QStringList VideoFormatUtils::getRawVideoExtensions() {
    initializeFormats();
    return s_rawVideoExtensions;
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

    return FormatType::COMPRESSED; // Default assumption
}

QString VideoFormatUtils::detectFormatFromExtension(const QString& filename) {
    initializeFormats();

    QString lowerFilename = filename.toLower();

    bool isRaw = false;
    for (const QString& ext : s_rawVideoExtensions) {
        if (lowerFilename.endsWith(ext)) {
            isRaw = true;
            break;
        }
    }

    if (isRaw) {
        if (lowerFilename.contains("420p")) {
            return "420P";
        } else if (lowerFilename.contains("422p")) {
            return "422P";
        } else if (lowerFilename.contains("444p")) {
            return "444P";
        } else if (lowerFilename.contains("yuyv")) {
            return "YUYV";
        } else if (lowerFilename.contains("uyvy")) {
            return "UYVY";
        } else if (lowerFilename.contains("nv12")) {
            return "NV12";
        } else if (lowerFilename.contains("nv21")) {
            return "NV21";
        } else {
            return "420P";
        }
    }

    return "COMPRESSED";
}
