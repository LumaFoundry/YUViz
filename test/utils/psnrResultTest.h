#pragma once

#include <QObject>
#include <QTest>

class PSNRResultTest : public QObject {
    Q_OBJECT

  private slots:
    void testDefaultConstructor();
    void testParameterizedConstructor();
    void testIsValid();
    void testToString();
    void testValidPSNRValues();
    void testInvalidPSNRValues();
    void testEdgeCases();
};

