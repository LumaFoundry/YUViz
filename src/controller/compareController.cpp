#include "compareController.h"
#include <QDebug>

CompareController::CompareController(QObject* parent) :
    QObject(parent) {
    m_diffBuffer = std::make_shared<std::vector<uint8_t>>();
}

void CompareController::setVideoIds(int id1, int id2) {
    m_index1 = id1;
    m_index2 = id2;
}

void CompareController::setMetadata(std::shared_ptr<FrameMeta> meta1, std::shared_ptr<FrameMeta> meta2) {

    if (m_index1 < 0 || m_index2 < 0) {
        qWarning() << "CompareController::setMetadata - Video IDs are not set";
        return;
    }

    m_metadata1 = meta1;
    m_metadata2 = meta2;
    if (m_metadata1 && m_metadata2 && m_metadata1->yWidth() == m_metadata2->yWidth() &&
        m_metadata1->yHeight() == m_metadata2->yHeight()) {

        qDebug() << "CompareController::Meta is set";

        if (m_diffWindow) {
            m_diffWindow->initialize(m_metadata1); // optional if already done in QML
            connect(this, &CompareController::requestUpload, m_diffWindow, &VideoWindow::uploadFrame);
            connect(m_diffWindow->m_renderer, &VideoRenderer::batchIsFull, this, &CompareController::onCompareUploaded);
            connect(this, &CompareController::requestRender, m_diffWindow, &VideoWindow::renderFrame);
            connect(
                m_diffWindow->m_renderer, &VideoRenderer::batchIsEmpty, this, &CompareController::onCompareRendered);
        } else {
            qWarning() << "CompareController::setMetadata - m_diffWindow is not initialized";
        }

        // Calculate buffer size based on metadata
        m_width = m_metadata1->yWidth();
        m_height = m_metadata1->yHeight();
        m_ySize = m_width * m_height;
        int uvWidth = m_metadata1->uvWidth();
        int uvHeight = m_metadata1->uvHeight();
        m_uvSize = uvWidth * uvHeight;
        int totalSize = m_ySize + 2 * m_uvSize;

        // Resize the buffer if needed
        if (m_diffBuffer->size() < totalSize) {
            m_diffBuffer->resize(totalSize);
            // Recreate the FrameData with the proper dimensions
            m_frameDiff = std::make_unique<FrameData>(m_ySize, m_uvSize, m_diffBuffer, 0);
        }
    }
}

// When either FC requested to upload a frame
void CompareController::onReceiveFrame(FrameData* frame, int index) {

    // Make a copy of the frame data
    if (index == m_index1) {
        m_frame1 = std::make_unique<FrameData>(*frame);
        // qDebug() << "Received frame from index:" << index;
    } else if (index == m_index2) {
        m_frame2 = std::make_unique<FrameData>(*frame);
        // qDebug() << "Received frame from index:" << index;
    } else {
        qWarning() << "Received frame for unknown index:" << index;
        return;
    }

    if (m_frame1 && m_frame2) {
        qDebug() << "Received both frames, diffing";
        diff();
        m_psnrResult = m_compareHelper->getPSNR(m_frame1.get(), m_frame2.get(), m_metadata1.get(), m_metadata2.get());
        m_psnr = m_psnrResult.average; // Keep backward compatibility
        emit requestUpload(m_frameDiff.get());
    }
}

void CompareController::onCompareUploaded() {

    qDebug() << "CompareController::onCompareUploaded";
    // Clear cache to make sure we don't compare stale frames
    if (m_frame1) {
        m_frame1.reset();
    }
    if (m_frame2) {
        m_frame2.reset();
    }
}

// when either FC requested to render frames
void CompareController::onRequestRender(int index) {

    if (index == m_index1) {
        m_ready1 = true;
    } else if (index == m_index2) {
        m_ready2 = true;
    }

    if (m_ready1 && m_ready2) {
        qDebug() << "Both frames are ready for rendering";
        // Emit signal to render frames
        emit requestRender();
    }
}

void CompareController::onCompareRendered() {
    qDebug() << "CompareController::onCompareRendered";
    if (m_ready1) {
        m_ready1 = false;
    }

    if (m_ready2) {
        m_ready2 = false;
    }

    qDebug() << "PSNR (Average):" << m_psnrResult.average << "Y:" << m_psnrResult.y << "U:" << m_psnrResult.u
             << "V:" << m_psnrResult.v;
}

void CompareController::diff() {
    if (!m_frame1 || !m_frame2 || !m_metadata1) {
        return;
    }

    // Calculate difference for Y plane
    uint8_t* y1 = m_frame1->yPtr();
    uint8_t* y2 = m_frame2->yPtr();
    uint8_t* yDiff = m_frameDiff->yPtr();

    for (int i = 0; i < m_ySize; ++i) {
        int diff = static_cast<int>(y2[i]) - static_cast<int>(y1[i]);
        int scaled = 4 * diff + 128;
        yDiff[i] = static_cast<uint8_t>(qBound(0, scaled, 255));
    }

    uint8_t* uDiff = m_frameDiff->uPtr();
    uint8_t* vDiff = m_frameDiff->vPtr();
    memset(uDiff, 128, m_uvSize);
    memset(vDiff, 128, m_uvSize);
}
