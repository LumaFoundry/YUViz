#pragma once

#include <QList>
#include <QObject>
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

class VideoFormatUtils : public QObject {
    Q_OBJECT

  public:
    static const QList<VideoFormat>& getSupportedFormats();
    static AVPixelFormat stringToPixelFormat(const QString& formatString);
    static QString pixelFormatToString(AVPixelFormat format);
    Q_INVOKABLE static QStringList getFormatIdentifiers();
    Q_INVOKABLE static QStringList getDisplayNames();
    Q_INVOKABLE static bool isValidFormat(const QString& formatString);
    static VideoFormat getFormatByIdentifier(const QString& identifier);
    Q_INVOKABLE static bool isCompressedFormat(const QString& formatString);
    static FormatType getFormatType(const QString& formatString);
    Q_INVOKABLE static QString detectFormatFromExtension(const QString& filename);
    Q_INVOKABLE static const QStringList& getRawVideoExtensions();

  private:
    static QList<VideoFormat> s_formats;
    static bool s_initialized;
    static void initializeFormats();
    static QStringList s_rawVideoExtensions;
};