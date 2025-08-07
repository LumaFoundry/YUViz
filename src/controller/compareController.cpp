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

        // Set timebase for proper time synchronization
        m_timebase1 = m_metadata1->timeBase();
        m_timebase2 = m_metadata2->timeBase();
        qDebug() << "CompareController::Timebase1:" << m_timebase1.num << "/" << m_timebase1.den;
        qDebug() << "CompareController::Timebase2:" << m_timebase2.num << "/" << m_timebase2.den;

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
        m_pts1 = frame->pts();
        // Calculate actual time using PTS * timebase
        m_time1 = av_mul_q(AVRational{static_cast<int>(m_pts1), 1}, m_timebase1);
        // qDebug() << "Received frame from index:" << index << "with PTS:" << m_pts1 << "time:" << m_time1.num << "/"
        // << m_time1.den;
    } else if (index == m_index2 && frame) {
        m_frame2 = std::make_unique<FrameData>(*frame);
        m_pts2 = frame->pts();
        // Calculate actual time using PTS * timebase
        m_time2 = av_mul_q(AVRational{static_cast<int>(m_pts2), 1}, m_timebase2);
        // qDebug() << "Received frame from index:" << index << "with PTS:" << m_pts2 << "time:" << m_time2.num << "/"
        // << m_time2.den;
    } else {
        qWarning() << "Received frame for unknown index:" << index;
        return;
    }

    // Only proceed if we have both frames and they have the same time (not PTS)
    if (m_frame1 && m_frame2 && av_cmp_q(m_time1, m_time2) == 0) {
        qDebug() << "Received both frames with matching time:" << m_time1.num << "/" << m_time1.den << ", diffing";
        m_psnrResult = m_compareHelper->getPSNR(m_frame1.get(), m_frame2.get(), m_metadata1.get(), m_metadata2.get());
        m_psnr = m_psnrResult.average; // Keep backward compatibility
        emit requestUpload(m_frame1.get(), m_frame2.get());
    } else if (m_frame1 && m_frame2 && av_cmp_q(m_time1, m_time2) != 0) {
        // Log when frames have different time values
        qDebug() << "Received frames with different time - frame1 time:" << m_time1.num << "/" << m_time1.den
                 << "frame2 time:" << m_time2.num << "/" << m_time2.den;

        // Clear the older frame to wait for the matching one
        if (av_cmp_q(m_time1, m_time2) < 0) {
            m_frame1.reset();
            m_pts1 = -1;
            m_time1 = AVRational{0, 1};
        } else {
            m_frame2.reset();
            m_pts2 = -1;
            m_time2 = AVRational{0, 1};
        }
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

    // Reset PTS values for next frame synchronization
    m_pts1 = -1;
    m_pts2 = -1;

    // Reset time values for next frame synchronization
    m_time1 = AVRational{0, 1};
    m_time2 = AVRational{0, 1};
}
