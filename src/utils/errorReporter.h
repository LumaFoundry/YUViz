#pragma once

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QObject>
#include <QString>
#include <functional>
#include <iostream>

enum class LogLevel {
    Info,
    Warning,
    Error,
    Fatal
};

class ErrorReporter : public QObject {
    Q_OBJECT

  public:
    // Singleton instance - IMPORTANT: Only instantiate in main!
    static ErrorReporter& instance();

    void report(const QString& msg, LogLevel level = LogLevel::Warning);

    // Overloaded function to accept std::string
    void report(const std::string& msg, LogLevel level = LogLevel::Warning) {
        report(QString::fromStdString(msg), level);
    };

    // Overloaded function to accept const char*
    void report(const char* msg, LogLevel level = LogLevel::Warning) { report(QString::fromUtf8(msg), level); }

    // Optional config
    void enableConsole(bool on);
    void enableQtWarnings(bool on);
    void setGuiCallback(std::function<void(QString, LogLevel)> callback);

  signals:
    void errorReported(QString msg, LogLevel level);

  private:
    ErrorReporter() = default;

    bool m_consoleEnabled = true;
    bool m_qtWarningEnabled = true;
    std::function<void(QString, LogLevel)> m_guiCallback;
};
