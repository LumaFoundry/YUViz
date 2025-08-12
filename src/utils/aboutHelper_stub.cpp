#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlEngine>
#include <QtQml/qqml.h>

class AboutHelper : public QObject {
    Q_OBJECT
  public:
    using QObject::QObject;
    Q_INVOKABLE void showNativeAbout(const QString&, const QString&, const QString&) {
        // No-op on non-macOS
    }
};

static void registerAboutHelper() {
    qmlRegisterSingletonType<AboutHelper>(
        "NativeAbout", 1, 0, "NativeAbout", [](QQmlEngine* engine, QJSEngine*) -> QObject* {
            return new AboutHelper(engine);
        });
}

Q_COREAPP_STARTUP_FUNCTION(registerAboutHelper)

#include "aboutHelper_stub.moc"
