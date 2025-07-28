#pragma once
#include <QObject>
#include "frames/frameData.h"
#include "frames/frameMeta.h"
#include "ui/videoWindow.h"
#include "utils/compareHelper.h"

class CompareController : public QObject {
    Q_OBJECT

  public:
    CompareController(QObject* parent = nullptr);

    ~CompareController() override = default;

    void setVideoIds(int id1, int id2);
    void setMetadata(std::shared_ptr<FrameMeta> meta1, std::shared_ptr<FrameMeta> meta2);
    void setDiffWindow(VideoWindow* diffWindow) { m_diffWindow = diffWindow; }

    void diff();
    FrameData* getDiff() { return m_frameDiff.get(); }

    PSNRResult getPSNRResult() const { return m_psnrResult; }
    double getPSNR() const { return m_psnr; } // For backward compatibility

  signals:
    void requestUpload(FrameData* frame);
    void requestRender();
    void psnrChanged(const QString& psnrInfo);

  public slots:
    void onReceiveFrame(FrameData* frame, int index);
    void onCompareUploaded();
    void onRequestRender(int index);
    void onCompareRendered();

  private:
    std::unique_ptr<CompareHelper> m_compareHelper = std::make_unique<CompareHelper>();

    int m_index1 = -1;
    int m_index2 = -1;

    std::unique_ptr<FrameData> m_frame1 = nullptr;
    std::unique_ptr<FrameData> m_frame2 = nullptr;

    std::shared_ptr<FrameMeta> m_metadata1 = nullptr;
    std::shared_ptr<FrameMeta> m_metadata2 = nullptr;

    bool m_ready1 = false;
    bool m_ready2 = false;

    double m_psnr = 0.0;
    PSNRResult m_psnrResult;

    std::shared_ptr<std::vector<uint8_t>> m_diffBuffer = nullptr;
    std::unique_ptr<FrameData> m_frameDiff = nullptr;

    int m_ySize = 0;
    int m_uvSize = 0;
    int m_width = 0;
    int m_height = 0;

    // For display
    VideoWindow* m_diffWindow = nullptr;
};
