#include "compareController.h"
#include <QDebug>

CompareController::CompareController(QObject* parent) :
    QObject(parent) {
}

void CompareController::setVideoIds(int id1, int id2) {
    m_index1 = id1;
    m_index2 = id2;
}

void CompareController::setMetadata(std::shared_ptr<FrameMeta> meta1,
                                    std::shared_ptr<FrameMeta> meta2,
                                    std::shared_ptr<FrameQueue> queue1,
                                    std::shared_ptr<FrameQueue> queue2) {

    m_metadata1 = meta1;
    m_metadata2 = meta2;
    if (m_metadata1 && m_metadata2 && m_metadata1->yWidth() == m_metadata2->yWidth() &&
        m_metadata1->yHeight() == m_metadata2->yHeight()) {

        qDebug() << "CompareController::Meta is set";

        if (m_diffWindow) {
            m_diffWindow->initialize(m_metadata1, queue1, queue2); // optional if already done in QML
            connect(
                this, &CompareController::requestUpload, m_diffWindow, &DiffWindow::uploadFrame, Qt::DirectConnection);
            connect(
                this, &CompareController::requestRender, m_diffWindow, &DiffWindow::renderFrame, Qt::DirectConnection);
            connect(m_diffWindow->m_renderer,
                    &DiffRenderer::batchIsEmpty,
                    this,
                    &CompareController::onCompareRendered,
                    Qt::DirectConnection);
        } else {
            qWarning() << "CompareController::setMetadata - m_diffWindow is not initialized";
        }
    }
}

// When either FC requested to upload a frame
void CompareController::onReceiveFrame(FrameData* frame, int index) {

    // Make a copy of the frame data
    if (index == m_index1 && frame) {
        m_frame1 = std::make_unique<FrameData>(*frame);
        // qDebug() << "Received frame from index:" << index;
    } else if (index == m_index2 && frame) {
        m_frame2 = std::make_unique<FrameData>(*frame);
        // qDebug() << "Received frame from index:" << index;
    } else {
        qWarning() << "Received frame for unknown index:" << index;
        return;
    }

    if (m_frame1 && m_frame2) {
        qDebug() << "Received both frames, diffing";
        m_psnrResult = m_compareHelper->getPSNR(m_frame1.get(), m_frame2.get(), m_metadata1.get(), m_metadata2.get());
        m_psnr = m_psnrResult.average; // Keep backward compatibility
        emit requestUpload(m_frame1.get(), m_frame2.get());
    }
}

void CompareController::onCompareUploaded() {
    qDebug() << "CompareController::onCompareUploaded";
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

    m_psnrInfo = "PSNR (Average): " + QString::number(m_psnrResult.average) +
                 "\nY: " + QString::number(m_psnrResult.y) + "\nU: " + QString::number(m_psnrResult.u) +
                 "\nV: " + QString::number(m_psnrResult.v);

    emit psnrChanged();

    qDebug() << m_psnrInfo;

    // Clear cache to make sure we don't compare stale frames
    if (m_frame1) {
        m_frame1.reset();
    }
    if (m_frame2) {
        m_frame2.reset();
    }
}
