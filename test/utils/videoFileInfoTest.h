#pragma once

#include <QObject>
#include <QTest>

class VideoFileInfoTest : public QObject {
    Q_OBJECT

  private slots:
    void testDefaultConstructor();
    void testPropertyAssignment();
    void testFilenameProperty();
    void testDimensions();
    void testFramerate();
    void testPixelFormat();
    void testGraphicsApi();
    void testForceSoftwareDecoding();
    void testCompleteVideoFileInfo();
};

