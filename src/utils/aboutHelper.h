#pragma once
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQml/QQmlEngine>
#include <QtQml/qqml.h>


class AboutHelper : public QObject {
    Q_OBJECT
  public:
    using QObject::QObject;
    Q_INVOKABLE void showNativeAbout(const QString& appName, const QString& version, const QString& buildDate);
};

void registerAboutHelper();
