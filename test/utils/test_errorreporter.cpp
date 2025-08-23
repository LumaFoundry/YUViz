#include <QtTest>
#include "utils/errorReporter.h"

class ErrorReporterTest : public QObject {
    Q_OBJECT
  private slots:
    void testReportQString();
    void testReportStdString();
    void testReportCharPtr();
    void testConsoleOutput();
    void testQtWarnings();
    void testGuiCallback();
    void testSignalEmission();
    void testEnableDisableConsole();
    void testEnableDisableQtWarnings();
    void testAllLogLevels();
};

void ErrorReporterTest::testReportQString() {
    ErrorReporter& reporter = ErrorReporter::instance();
    bool called = false;
    reporter.setGuiCallback(
        [&called](QString msg, LogLevel level) { called = (msg == "QString message" && level == LogLevel::Warning); });
    reporter.enableConsole(false);
    reporter.enableQtWarnings(false);
    reporter.report(QString("QString message"), LogLevel::Warning);
    QVERIFY(called);
}

void ErrorReporterTest::testReportStdString() {
    ErrorReporter& reporter = ErrorReporter::instance();
    bool called = false;
    reporter.setGuiCallback(
        [&called](QString msg, LogLevel level) { called = (msg == "StdString message" && level == LogLevel::Error); });
    reporter.enableConsole(false);
    reporter.enableQtWarnings(false);
    reporter.report(std::string("StdString message"), LogLevel::Error);
    QVERIFY(called);
}

void ErrorReporterTest::testReportCharPtr() {
    ErrorReporter& reporter = ErrorReporter::instance();
    bool called = false;
    reporter.setGuiCallback(
        [&called](QString msg, LogLevel level) { called = (msg == "CharPtr message" && level == LogLevel::Info); });
    reporter.enableConsole(false);
    reporter.enableQtWarnings(false);
    reporter.report("CharPtr message", LogLevel::Info);
    QVERIFY(called);
}

void ErrorReporterTest::testConsoleOutput() {
    ErrorReporter& reporter = ErrorReporter::instance();
    reporter.enableConsole(true);
    reporter.enableQtWarnings(false);
    reporter.setGuiCallback(nullptr);
    reporter.report("Console Info", LogLevel::Info);
    reporter.report("Console Warning", LogLevel::Warning);
    reporter.report("Console Error", LogLevel::Error);
    // Don't call Fatal, as it may abort
    reporter.enableConsole(false);
}

void ErrorReporterTest::testQtWarnings() {
    ErrorReporter& reporter = ErrorReporter::instance();
    reporter.enableConsole(false);
    reporter.enableQtWarnings(true);
    reporter.setGuiCallback(nullptr);
    reporter.report("Qt Info", LogLevel::Info);
    reporter.report("Qt Warning", LogLevel::Warning);
    reporter.report("Qt Error", LogLevel::Error);
    // Don't call Fatal, as it may abort
    reporter.enableQtWarnings(false);
}

void ErrorReporterTest::testGuiCallback() {
    ErrorReporter& reporter = ErrorReporter::instance();
    int callCount = 0;
    reporter.setGuiCallback([&callCount](QString, LogLevel) { ++callCount; });
    reporter.enableConsole(false);
    reporter.enableQtWarnings(false);
    reporter.report("Callback test", LogLevel::Warning);
    QCOMPARE(callCount, 1);
}

void ErrorReporterTest::testSignalEmission() {
    ErrorReporter& reporter = ErrorReporter::instance();
    QSignalSpy spy(&reporter, SIGNAL(errorReported(QString, LogLevel)));
    reporter.enableConsole(false);
    reporter.enableQtWarnings(false);
    reporter.setGuiCallback(nullptr);
    reporter.report("Signal test", LogLevel::Error);
    QCOMPARE(spy.count(), 1);
    QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.at(0).toString(), QString("Signal test"));
    QCOMPARE(static_cast<int>(args.at(1).value<LogLevel>()), static_cast<int>(LogLevel::Error));
}

void ErrorReporterTest::testEnableDisableConsole() {
    ErrorReporter& reporter = ErrorReporter::instance();
    reporter.enableConsole(true);
    reporter.enableConsole(false);
    reporter.report("Console toggle", LogLevel::Info);
}

void ErrorReporterTest::testEnableDisableQtWarnings() {
    ErrorReporter& reporter = ErrorReporter::instance();
    reporter.enableQtWarnings(true);
    reporter.enableQtWarnings(false);
    reporter.report("QtWarnings toggle", LogLevel::Info);
}

void ErrorReporterTest::testAllLogLevels() {
    ErrorReporter& reporter = ErrorReporter::instance();
    int callCount = 0;
    reporter.setGuiCallback([&callCount](QString, LogLevel) { ++callCount; });
    reporter.enableConsole(false);
    reporter.enableQtWarnings(false);
    reporter.report("Info", LogLevel::Info);
    reporter.report("Warning", LogLevel::Warning);
    reporter.report("Error", LogLevel::Error);
    // Don't call Fatal, as it may abort
    QCOMPARE(callCount, 3);
}

QTEST_MAIN(ErrorReporterTest)
#include "test_errorreporter.moc"
