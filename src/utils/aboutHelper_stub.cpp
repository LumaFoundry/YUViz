#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlEngine>
#include <QtQml/qqml.h>

// Dummy QObject for QML singleton registration
class NativeAboutStub : public QObject {
  public:
    explicit NativeAboutStub(QObject* parent = nullptr) :
        QObject(parent) {}
    Q_INVOKABLE void showNativeAbout(const QString&, const QString&, const QString&) {
        // No-op on non-macOS
    }
};

static QObject* nativeAbout_stub_singletontype_provider(QQmlEngine*, QJSEngine*) {
    return new NativeAboutStub();
}

void registerAboutHelper() {
    qmlRegisterSingletonType<NativeAboutStub>(
        "NativeAbout", 1, 0, "NativeAbout", nativeAbout_stub_singletontype_provider);
}