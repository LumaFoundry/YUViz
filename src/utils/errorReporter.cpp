#include "errorReporter.h"

// Singleton
ErrorReporter& ErrorReporter::instance() {
    static ErrorReporter instance;
    return instance;
}

void ErrorReporter::report(const QString& msg, LogLevel level) {
    // Console output
    if (m_consoleEnabled) {
        std::cerr << "[ErrorReporter] ";
        switch (level) {
            case LogLevel::Info:    std::cerr << "[Info] "; break;
            case LogLevel::Warning: std::cerr << "[Warning] "; break;
            case LogLevel::Error:   std::cerr << "[Error] "; break;
            case LogLevel::Fatal:   std::cerr << "[Fatal] "; break;
        }
        std::cerr << msg.toStdString() << std::endl;
    }

    // Qt warnings
    if (m_qtWarningEnabled) {
        switch (level) {
            case LogLevel::Info:    qInfo().noquote() << msg; break;
            case LogLevel::Warning: qWarning().noquote() << msg; break;
            case LogLevel::Error:   qCritical().noquote() << msg; break;
            case LogLevel::Fatal:   qFatal("%s", msg.toStdString().c_str()); break;
        }
    }

    // GUI callback
    if (m_guiCallback) {
        m_guiCallback(msg, level);
    }

    // Emit signal
    emit errorReported(msg, level);
}

void ErrorReporter::enableConsole(bool on) {
    m_consoleEnabled = on;
}

void ErrorReporter::enableQtWarnings(bool on) {
    m_qtWarningEnabled = on;
}

void ErrorReporter::setGuiCallback(std::function<void(QString, LogLevel)> callback) {
    m_guiCallback = std::move(callback);
}

