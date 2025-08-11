#pragma once

#include <QList>
#include <QString>
#include <QStringList>

extern "C" {
#include <libavutil/pixfmt.h>
}

enum class FormatType {
    RAW_YUV,
    COMPRESSED
};

struct VideoFormat {
    QString identifier;        // "420P", "422P", "MP4", etc.
    QString displayName;       // "420P - YUV420P (Planar)", "MP4 - Compressed Video"
    AVPixelFormat pixelFormat; // AV_PIX_FMT_YUV420P
    FormatType type;           // RAW_YUV or COMPRESSED
};

class VideoFormatUtils {
  public:
    static const QList<VideoFormat>& getSupportedFormats();
    static AVPixelFormat stringToPixelFormat(const QString& formatString);
    static QString pixelFormatToString(AVPixelFormat format);
    static QStringList getFormatIdentifiers();
    static QStringList getDisplayNames();
    static bool isValidFormat(const QString& formatString);
    static VideoFormat getFormatByIdentifier(const QString& identifier);
    static bool isCompressedFormat(const QString& formatString);
    static FormatType getFormatType(const QString& formatString);
    static QString detectFormatFromExtension(const QString& filename);

  private:
    static QList<VideoFormat> s_formats;
    static bool s_initialized;
    static void initializeFormats();
};