#pragma once

#include <QObject>
#include <QSignalSpy>
#include <QTest>

class DiffRendererTest : public QObject {
    Q_OBJECT

  private slots:
    void testConstructor();
    void testSetDiffConfig();
    void testSetZoomAndOffset();
    void testInitialization();
};
