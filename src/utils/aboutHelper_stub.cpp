#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlEngine>
#include <QtQml/qqml.h>
#include "aboutHelper.h"

void AboutHelper::showNativeAbout(const QString&, const QString&, const QString&) {
    // No-op on non-macOS
}

void registerAboutHelper() {
    qmlRegisterSingletonType<AboutHelper>(
        "NativeAbout", 1, 0, "NativeAbout", [](QQmlEngine* engine, QJSEngine*) -> QObject* {
            return new AboutHelper(engine);
        });
}
