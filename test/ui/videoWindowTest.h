#pragma once

#include <QObject>
#include <QTest>
#include "ui/videoWindow.h"

class VideoWindowTest : public QObject {
    Q_OBJECT

    void testConstructor();
    void testWindowOperations();
    void testErrorHandling();
};

