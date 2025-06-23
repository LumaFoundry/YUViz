#include <QtTest>
#include <QSignalSpy>
#include <QThread>
#include <iostream>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "controller/frameController.h"
#include "controller/playBackWorker.h"
#include "frames/frameMeta.h"
#include "frames/frameQueue.h"
#include "rendering/videoRenderer.h"
#include "mock/mock_decoder.h"
#include "mock/mock_renderer.h"

using ::testing::_;
using ::testing::Return;


class FrameControllerTest : public QObject {
    Q_OBJECT

private:
    FrameMeta meta;
    std::shared_ptr<PlaybackWorker> pbw = nullptr;
    QThread pbwThread;

private slots:
    void initTestCase(){
        meta.setYWidth(2);
        meta.setYHeight(2);
        meta.setUVWidth(1);
        meta.setUVHeight(1);

        // Set up PlaybackWorker and thread
        pbw = std::make_shared<PlaybackWorker>();
        pbw->moveToThread(&pbwThread);
        pbwThread.start();
        pbw->start();
    }

    void testFCStart();

    void cleanupTestCase() {
        pbw->stop();
        pbwThread.quit();
        pbwThread.wait();
    }

};

void FrameControllerTest::testFCStart() {

    std::cout << "Creating mock decoder..." << std::endl;

    auto decoder = std::make_unique<MockDecoder>();
    
    EXPECT_CALL(*decoder, getMetaData()).WillOnce(Return(meta));

    int fakePts = 0;

    EXPECT_CALL(*decoder, loadFrame(_))
        .WillRepeatedly([&](FrameData* frame) {
            memset(frame->yPtr(), 128, meta.ySize()); 
            memset(frame->uPtr(), 128, meta.uvSize());
            memset(frame->vPtr(), 128, meta.uvSize());
            frame->setPts(fakePts++);
            emit decoder->frameLoaded(true);
        });
        
    std::cout << "Creating mock renderer..." << std::endl;
    auto renderer = std::make_unique<MockRenderer>(nullptr, nullptr, nullptr);
    EXPECT_CALL(*renderer, uploadFrame(_)).WillRepeatedly([&](FrameData* f){
        emit renderer->batchUploaded(true);
    });

    std::cout << "Creating FrameController..." << std::endl;
    FrameController* controller = new FrameController(nullptr, decoder.get(), renderer.get(), pbw, 0);
    
    //TODO: For some reason signals sent from mock objects cannot trigger slots in FC

    // connect(decoder, &MockDecoder::frameLoaded, controller, &FrameController::onFrameDecoded);
    // connect(renderer, &MockRenderer::frameUploaded, controller, &FrameController::onFrameUploaded);
    connect(controller, &FrameController::currentDelta, pbw.get(), &PlaybackWorker::scheduleNext);

    std::cout << "Setting up QSignalSpy..." << std::endl;
    QSignalSpy fcSpyRequestDecode(controller, SIGNAL(requestDecode(FrameData*)));
    QSignalSpy fcSpyRequestUpload(controller, SIGNAL(requestUpload(FrameData*)));

    // Start the controller
    std::cout << "Starting controller..." << std::endl;
    controller->start();
    std::cout << "Controller started..." << std::endl;

    std::cout << "Checking request decode signal counts..." << std::endl;

    QTRY_COMPARE(fcSpyRequestDecode.count(), 1);
    // std::cout << "Checking request upload signal counts..." << std::endl;
    // QTRY_COMPARE(fcSpyRequestUpload.count(), 1);

    decoder.reset();
    renderer.reset();
}

QTEST_MAIN(FrameControllerTest)
#include "test_framecontroller.moc"