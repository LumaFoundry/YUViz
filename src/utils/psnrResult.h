#pragma once
#include <QString>

struct PSNRResult {
    double average = -1.0;
    double y = -1.0;
    double u = -1.0;
    double v = -1.0;

    PSNRResult() = default;

    PSNRResult(double avg, double yValue, double uValue, double vValue) :
        average(avg),
        y(yValue),
        u(uValue),
        v(vValue) {}

    // Check if the PSNR calculation was successful
    bool isValid() const { return average >= 0.0 && y >= 0.0 && u >= 0.0 && v >= 0.0; }

    // For debugging
    QString toString() const { return QString("PSNR(avg: %1, Y: %2, U: %3, V: %4)").arg(average).arg(y).arg(u).arg(v); }
};
