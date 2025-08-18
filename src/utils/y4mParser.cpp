#include "y4mParser.h"
#include <QDebug>
#include <QFile>
#include <QRegularExpression>
#include "debugManager.h"
#include "errorReporter.h"

Y4MInfo Y4MParser::parseHeader(const QString& filePath) {
    Y4MInfo info;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        ErrorReporter::instance().report("Cannot open Y4M file: " + filePath.toStdString(), LogLevel::Error);
        return info;
    }

    // Read first 512 bytes to parse header
    QByteArray headerData = file.read(512);
    file.close();

    if (headerData.isEmpty()) {
        ErrorReporter::instance().report("Y4M file is empty or cannot be read", LogLevel::Error);
        return info;
    }

    // Find first newline character, Y4M header ends with newline
    int headerEnd = headerData.indexOf('\n');
    if (headerEnd == -1) {
        ErrorReporter::instance().report("Invalid Y4M file header format: newline not found", LogLevel::Error);
        return info;
    }

    QString headerStr = QString::fromLatin1(headerData.left(headerEnd));
    info.headerSize = headerEnd + 1; // Include newline character

    debug("y4m", QString("Y4M header content: %1").arg(headerStr));

    // Check Y4M magic string
    if (!headerStr.startsWith("YUV4MPEG2")) {
        ErrorReporter::instance().report("Not a valid Y4M file: missing YUV4MPEG2 identifier", LogLevel::Error);
        return info;
    }

    // Parse parameters
    QStringList params = headerStr.split(' ', Qt::SkipEmptyParts);

    for (const QString& param : params) {
        if (param.startsWith('W')) {
            // Width parameter
            bool ok;
            info.width = param.mid(1).toInt(&ok);
            if (!ok || info.width <= 0) {
                ErrorReporter::instance().report("Invalid Y4M file width parameter: " + param.toStdString(),
                                                 LogLevel::Error);
                return info;
            }
        } else if (param.startsWith('H')) {
            // Height parameter
            bool ok;
            info.height = param.mid(1).toInt(&ok);
            if (!ok || info.height <= 0) {
                ErrorReporter::instance().report("Invalid Y4M file height parameter: " + param.toStdString(),
                                                 LogLevel::Error);
                return info;
            }
        } else if (param.startsWith('F')) {
            // Frame rate parameter
            QString frameRateStr = param.mid(1);
            info.frameRate = parseFrameRate(frameRateStr);
            if (info.frameRate <= 0) {
                ErrorReporter::instance().report("Invalid Y4M file frame rate parameter: " + param.toStdString(),
                                                 LogLevel::Error);
                return info;
            }
        } else if (param.startsWith('I')) {
            // Interlacing parameter
            info.interlacing = param.mid(1);
        } else if (param.startsWith('A')) {
            // Aspect ratio parameter
            info.aspectRatio = param.mid(1);
        } else if (param.startsWith('C')) {
            // Color space parameter
            info.colorSpace = param.mid(1);
            info.pixelFormat = colorSpaceToPixelFormat(info.colorSpace);
        }
    }

    // Validate required parameters
    if (info.width <= 0 || info.height <= 0) {
        ErrorReporter::instance().report("Y4M file missing required width or height parameters", LogLevel::Error);
        return info;
    }

    info.isValid = true;

    debug("y4m",
          QString("Y4M parsing successful - Width: %1, Height: %2, Frame rate: %3, Color space: %4, Header size: %5")
              .arg(info.width)
              .arg(info.height)
              .arg(info.frameRate)
              .arg(info.colorSpace)
              .arg(info.headerSize));

    return info;
}

bool Y4MParser::isY4MFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    // Read first 12 bytes to check magic string
    QByteArray magicBytes = file.read(12);
    file.close();

    return magicBytes.startsWith("YUV4MPEG2");
}

AVPixelFormat Y4MParser::colorSpaceToPixelFormat(const QString& colorSpace) {
    if (colorSpace == "420" || colorSpace == "420jpeg" || colorSpace == "420paldv" || colorSpace == "420mpeg2") {
        return AV_PIX_FMT_YUV420P;
    } else if (colorSpace == "422") {
        return AV_PIX_FMT_YUV422P;
    } else if (colorSpace == "444") {
        return AV_PIX_FMT_YUV444P;
    } else if (colorSpace == "411") {
        return AV_PIX_FMT_YUV411P;
    } else if (colorSpace == "mono") {
        return AV_PIX_FMT_GRAY8;
    } else if (colorSpace == "420p10") {
        return AV_PIX_FMT_YUV420P10LE;
    } else if (colorSpace == "422p10") {
        return AV_PIX_FMT_YUV422P10LE;
    } else if (colorSpace == "444p10") {
        return AV_PIX_FMT_YUV444P10LE;
    } else if (colorSpace == "420p12") {
        return AV_PIX_FMT_YUV420P12LE;
    } else if (colorSpace == "422p12") {
        return AV_PIX_FMT_YUV422P12LE;
    } else if (colorSpace == "444p12") {
        return AV_PIX_FMT_YUV444P12LE;
    } else if (colorSpace == "420p14") {
        return AV_PIX_FMT_YUV420P14LE;
    } else if (colorSpace == "422p14") {
        return AV_PIX_FMT_YUV422P14LE;
    } else if (colorSpace == "444p14") {
        return AV_PIX_FMT_YUV444P14LE;
    } else if (colorSpace == "420p16") {
        return AV_PIX_FMT_YUV420P16LE;
    } else if (colorSpace == "422p16") {
        return AV_PIX_FMT_YUV422P16LE;
    } else if (colorSpace == "444p16") {
        return AV_PIX_FMT_YUV444P16LE;
    }

    // Default to 420P
    debug("y4m", QString("Unknown Y4M color space: %1, using default YUV420P").arg(colorSpace));
    return AV_PIX_FMT_YUV420P;
}

double Y4MParser::parseFrameRate(const QString& frameRateStr) {
    if (frameRateStr.contains(':')) {
        // Fraction format, like "25:1" or "30000:1001"
        QStringList parts = frameRateStr.split(':');
        if (parts.size() == 2) {
            bool ok1, ok2;
            double numerator = parts[0].toDouble(&ok1);
            double denominator = parts[1].toDouble(&ok2);
            if (ok1 && ok2 && denominator != 0) {
                return numerator / denominator;
            }
        }
    } else {
        // Simple number format, like "25"
        bool ok;
        double rate = frameRateStr.toDouble(&ok);
        if (ok && rate > 0) {
            return rate;
        }
    }

    debug("y4m", QString("Cannot parse frame rate: %1, using default value 25").arg(frameRateStr));
    return 25.0; // Default frame rate
}

int Y4MParser::calculateTotalFrames(const QString& filePath, const Y4MInfo& info) {
    if (!info.isValid) {
        return -1;
    }

    QFileInfo fileInfo(filePath);
    int64_t fileSize = fileInfo.size();

    // Each frame contains "FRAME\n" prefix (6 bytes) plus frame data
    int frameDataSize = calculateFrameSize(info);
    int totalFrameSize = 6 + frameDataSize; // "FRAME\n" + actual frame data

    // File size - header size = total size of all frames
    int64_t framesDataSize = fileSize - info.headerSize;

    if (framesDataSize <= 0 || totalFrameSize <= 0) {
        return -1;
    }

    int totalFrames = static_cast<int>(framesDataSize / totalFrameSize);

    debug("y4m",
          QString("Y4M total frame calculation - File size: %1, Header size: %2, Frame size: %3, Total frames: %4")
              .arg(fileSize)
              .arg(info.headerSize)
              .arg(totalFrameSize)
              .arg(totalFrames));

    return totalFrames;
}

int Y4MParser::calculateFrameSize(const Y4MInfo& info) {
    if (!info.isValid) {
        return 0;
    }

    int width = info.width;
    int height = info.height;

    // Calculate frame size based on pixel format
    switch (info.pixelFormat) {
    case AV_PIX_FMT_YUV420P:
        return width * height + 2 * ((width + 1) / 2) * ((height + 1) / 2);
    case AV_PIX_FMT_YUV422P:
        return width * height + 2 * ((width + 1) / 2) * height;
    case AV_PIX_FMT_YUV444P:
        return width * height * 3;
    case AV_PIX_FMT_YUV411P:
        return width * height + 2 * ((width + 3) / 4) * height;
    case AV_PIX_FMT_GRAY8:
        return width * height;
    case AV_PIX_FMT_YUV420P10LE:
    case AV_PIX_FMT_YUV420P10BE:
        return width * height * 2 + 2 * ((width + 1) / 2) * ((height + 1) / 2) * 2;
    case AV_PIX_FMT_YUV422P10LE:
    case AV_PIX_FMT_YUV422P10BE:
        return width * height * 2 + 2 * ((width + 1) / 2) * height * 2;
    case AV_PIX_FMT_YUV444P10LE:
    case AV_PIX_FMT_YUV444P10BE:
        return width * height * 6;
    case AV_PIX_FMT_YUV420P12LE:
    case AV_PIX_FMT_YUV420P12BE:
        return width * height * 2 + 2 * ((width + 1) / 2) * ((height + 1) / 2) * 2;
    case AV_PIX_FMT_YUV422P12LE:
    case AV_PIX_FMT_YUV422P12BE:
        return width * height * 2 + 2 * ((width + 1) / 2) * height * 2;
    case AV_PIX_FMT_YUV444P12LE:
    case AV_PIX_FMT_YUV444P12BE:
        return width * height * 6;
    case AV_PIX_FMT_YUV420P14LE:
    case AV_PIX_FMT_YUV420P14BE:
        return width * height * 2 + 2 * ((width + 1) / 2) * ((height + 1) / 2) * 2;
    case AV_PIX_FMT_YUV422P14LE:
    case AV_PIX_FMT_YUV422P14BE:
        return width * height * 2 + 2 * ((width + 1) / 2) * height * 2;
    case AV_PIX_FMT_YUV444P14LE:
    case AV_PIX_FMT_YUV444P14BE:
        return width * height * 6;
    case AV_PIX_FMT_YUV420P16LE:
    case AV_PIX_FMT_YUV420P16BE:
        return width * height * 2 + 2 * ((width + 1) / 2) * ((height + 1) / 2) * 2;
    case AV_PIX_FMT_YUV422P16LE:
    case AV_PIX_FMT_YUV422P16BE:
        return width * height * 2 + 2 * ((width + 1) / 2) * height * 2;
    case AV_PIX_FMT_YUV444P16LE:
    case AV_PIX_FMT_YUV444P16BE:
        return width * height * 6;
    default:
        // Default to 420P
        return width * height + 2 * ((width + 1) / 2) * ((height + 1) / 2);
    }
}
