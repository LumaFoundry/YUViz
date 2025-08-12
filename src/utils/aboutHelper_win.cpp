#ifdef _WIN32
#include <QtCore/QObject>
#include <QtQml/QQmlEngine>
#include <QtQml/qqml.h>
#include <shellapi.h>
#include <string>
#include <windows.h>

class AboutHelperWin : public QObject {
    Q_OBJECT
  public:
    explicit AboutHelperWin(QObject* parent = nullptr) :
        QObject(parent) {}

    Q_INVOKABLE void showNativeAbout(const QString& appName, const QString& version, const QString& buildDate) {
        QString ver = version.isEmpty() ? QStringLiteral("dev") : version;
        QString msg =
            QString("%1\nVersion: %2\nBuilt: %3\nSource: GitHub Repository\n\nClick OK to open the repository.")
                .arg(appName, ver, buildDate);

        int ret = MessageBoxW(
            nullptr, (LPCWSTR)msg.utf16(), (LPCWSTR)appName.utf16(), MB_OKCANCEL | MB_ICONINFORMATION | MB_TOPMOST);
        if (ret == IDOK) {
            ShellExecuteW(
                nullptr, L"open", L"https://github.com/LokiW-03/qt6-videoplayer", nullptr, nullptr, SW_SHOWNORMAL);
        }
    }
};

static void registerAboutHelperWin() {
    qmlRegisterSingletonType<AboutHelperWin>(
        "NativeAbout", 1, 0, "NativeAbout", [](QQmlEngine* engine, QJSEngine*) { return new AboutHelperWin(engine); });
}

Q_COREAPP_STARTUP_FUNCTION(registerAboutHelperWin)

#include "aboutHelper_win.moc"
#endif
