#pragma once

#include <QFileInfo>
#include <QString>
#include <QTextStream>
#include <fstream>
#include <string>

extern "C" {
#include <libavutil/pixfmt.h>
}

/**
 * @brief Y4M file format information structure
 */
struct Y4MInfo {
    int width = 0;
    int height = 0;
    double frameRate = 25.0;
    AVPixelFormat pixelFormat = AV_PIX_FMT_YUV420P;
    int headerSize = 0; // Y4M header size in bytes
    bool isValid = false;

    // Y4M optional parameters
    QString aspectRatio = "1:1";
    QString interlacing = "p"; // p=progressive, t=top first, b=bottom first
    QString colorSpace = "420";
};

/**
 * @brief Y4M file format parser
 *
 * Y4M format description:
 * - File header: YUV4MPEG2 W<width> H<height> F<framerate> I<interlacing> A<aspect> C<colorspace>
 * - Frame prefix: FRAME
 */
class Y4MParser {
  public:
    /**
     * @brief Parse Y4M file header information
     * @param filePath Y4M file path
     * @return Y4M format information structure
     */
    static Y4MInfo parseHeader(const QString& filePath);

    /**
     * @brief Check if file is Y4M format
     * @param filePath File path
     * @return true if it's Y4M format
     */
    static bool isY4MFile(const QString& filePath);

    /**
     * @brief Convert from Y4M color space string to AVPixelFormat
     * @param colorSpace Y4M color space identifier (like "420", "422", "444")
     * @return Corresponding AVPixelFormat
     */
    static AVPixelFormat colorSpaceToPixelFormat(const QString& colorSpace);

    /**
     * @brief Parse Y4M frame rate string
     * @param frameRateStr Frame rate string (like "25:1", "30000:1001")
     * @return Frame rate value
     */
    static double parseFrameRate(const QString& frameRateStr);

    /**
     * @brief Calculate total frames in Y4M file
     * @param filePath File path
     * @param info Y4M format information
     * @return Total frame count
     */
    static int calculateTotalFrames(const QString& filePath, const Y4MInfo& info);

    /**
     * @brief Calculate frame data size (excluding FRAME prefix)
     * @param info Y4M format information
     * @return Frame size in bytes
     */
    static int calculateFrameSize(const Y4MInfo& info);

  private:
};
