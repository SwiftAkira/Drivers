#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

namespace flux {

// HID usage tables shared by the mouse and keyboard UIs, plus translation
// from Qt's nativeScanCode (evdev + 8 on Linux) to HID usages so the UI can
// "press a key to bind it" and record macros.
class KeyNames : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(HidKeys)
    QML_SINGLETON

public:
    explicit KeyNames(QObject *parent = nullptr);
    static KeyNames *create(QQmlEngine *, QJSEngine *);

    // Keyboard page (0x07)
    Q_INVOKABLE static QString hidName(int usage);
    Q_INVOKABLE static QString hidShortName(int usage);
    Q_INVOKABLE static int hidFromScanCode(int nativeScanCode);
    Q_INVOKABLE static bool isModifierUsage(int usage) { return usage >= 0xE0 && usage <= 0xE7; }
    Q_INVOKABLE static int modifierBit(int usage) { return isModifierUsage(usage) ? 1 << (usage - 0xE0) : 0; }
    Q_INVOKABLE static QString modifierText(int mask);
    Q_INVOKABLE static QVariantList hidKeys();       // [{usage, name, group}]

    // Consumer page (0x0C)
    Q_INVOKABLE static QString consumerName(int usage);
    Q_INVOKABLE static QVariantList consumerKeys();  // [{usage, name}]

    static int evdevToHid(int evdev);
    static int hidToEvdev(int usage);

private:
    static inline KeyNames *s_instance = nullptr;
};

} // namespace flux
