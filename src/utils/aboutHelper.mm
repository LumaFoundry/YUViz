#ifdef __APPLE__
#include "aboutHelper.h"
#include <AppKit/AppKit.h>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtGui/QGuiApplication>
#include <QtQml/QQmlContext>
#include <QtQml/QQmlEngine>
#include <QtQml/qqml.h>

void AboutHelper::showNativeAbout(const QString& appName, const QString& version, const QString& buildDate) {
    @autoreleasepool {
        NSApplication* app = [NSApplication sharedApplication];
        if (!app)
            return;

        NSString* appNameNS = [NSString stringWithUTF8String:appName.toUtf8().constData()];
        QString verStr = version.isEmpty() ? QStringLiteral("dev") : version;
        NSString* versionNS = [NSString stringWithUTF8String:verStr.toUtf8().constData()];
        NSString* buildNS = [NSString stringWithUTF8String:buildDate.toUtf8().constData()];

        NSString* baseString =
            [NSString stringWithFormat:@"Version: %@\nBuilt: %@\nSource: GitHub Repository", versionNS, buildNS];
        NSMutableAttributedString* credits =
            [[[NSMutableAttributedString alloc] initWithString:baseString] autorelease];
        // Center text
        NSMutableParagraphStyle* style = [[[NSMutableParagraphStyle alloc] init] autorelease];
        style.alignment = NSTextAlignmentCenter;
        [credits addAttribute:NSParagraphStyleAttributeName value:style range:NSMakeRange(0, credits.length)];
        // Make link clickable and styled
        NSString* linkText = @"GitHub Repository";
        NSRange linkRange = [baseString rangeOfString:linkText];
        if (linkRange.location != NSNotFound) {
            [credits addAttribute:NSLinkAttributeName
                            value:@"https://github.com/LokiW-03/qt6-videoplayer"
                            range:linkRange];
            [credits addAttribute:NSForegroundColorAttributeName value:[NSColor systemBlueColor] range:linkRange];
            [credits addAttribute:NSUnderlineStyleAttributeName value:@(NSUnderlineStyleSingle) range:linkRange];
        }

        NSDictionary* info = @{
            NSAboutPanelOptionApplicationName : appNameNS,
            NSAboutPanelOptionVersion : versionNS,
            NSAboutPanelOptionCredits : credits
        };
        [app orderFrontStandardAboutPanelWithOptions:info];
    }
}

void registerAboutHelper() {
    qmlRegisterSingletonType<AboutHelper>(
        "NativeAbout", 1, 0, "NativeAbout", [](QQmlEngine* engine, QJSEngine*) -> QObject* {
            auto* helper = new AboutHelper(engine);
            return helper;
        });
}

#endif
