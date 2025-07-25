#include "compareController.h"
#include <QDebug>

CompareController::CompareController(QObject* parent) :
    QObject(parent) {
}

void CompareController::setVideoIds(int id1, int id2) {
    m_index1 = id1;
    m_index2 = id2;
}

void CompareController::setMetadata(std::shared_ptr<FrameMeta> meta1, std::shared_ptr<FrameMeta> meta2) {
    m_metadata1 = meta1;
    m_metadata2 = meta2;
}

// When either FC requested to upload a frame
void CompareController::onReceiveFrame(FrameData* frame, int index) {

    // Make a copy of the frame data
    if (index == m_index1) {
        m_frame1 = std::make_unique<FrameData>(*frame);
        qDebug() << "Received frame from index:" << index;
    } else if (index == m_index2) {
        m_frame2 = std::make_unique<FrameData>(*frame);
        qDebug() << "Received frame from index:" << index;
    } else {
        qWarning() << "Received frame for unknown index:" << index;
        return;
    }

    if (m_frame1 && m_frame2) {
        // Emit signal to request compare and upload frames
        emit requestUpload(m_frame1.get(), m_frame2.get());
        m_psnrResult = m_compareHelper->getPSNR(m_frame1.get(), m_frame2.get(), m_metadata1.get(), m_metadata2.get());
        m_psnr = m_psnrResult.average; // Keep backward compatibility
    }
}

void CompareController::onCompareUploaded() {

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
        // Emit signal to render frames
        qDebug() << "Both frames ready";
        emit requestRender();
        qDebug() << "PSNR (Average):" << m_psnrResult.average
                 << "Y:" << m_psnrResult.y 
                 << "U:" << m_psnrResult.u 
                 << "V:" << m_psnrResult.v;
        m_ready1 = false;
        m_ready2 = false;
    }
}
