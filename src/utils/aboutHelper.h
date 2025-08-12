#pragma once
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtQml/QQmlEngine>
#include <QtQml/qqml.h>

class AboutHelper : public QObject {
    Q_OBJECT
  public:
    explicit AboutHelper(QObject* parent = nullptr) :
        QObject(parent) {}
    Q_INVOKABLE void showNativeAbout(const QString& appName, const QString& version, const QString& buildDate);
};

void registerAboutHelper();
