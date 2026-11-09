#include "KeyNames.h"

#include <QHash>
#include <QJSEngine>
#include <QVariantMap>

namespace flux {

namespace {

// HID usage (index) -> Linux evdev keycode, from drivers/hid/hid-input.c.
constexpr int kHidToEvdev[0xE8] = {
    0,   0,   0,   0,   30,  48,  46,  32,  18,  33,  34,  35,  23,  36,  37,  38,  // 0x00
    50,  49,  24,  25,  16,  19,  31,  20,  22,  47,  17,  45,  21,  44,  2,   3,   // 0x10
    4,   5,   6,   7,   8,   9,   10,  11,  28,  1,   14,  15,  57,  12,  13,  26,  // 0x20
    27,  43,  43,  39,  40,  41,  51,  52,  53,  58,  59,  60,  61,  62,  63,  64,  // 0x30
    65,  66,  67,  68,  87,  88,  99,  70,  119, 110, 102, 104, 111, 107, 109, 106, // 0x40
    105, 108, 103, 69,  98,  55,  74,  78,  96,  79,  80,  81,  75,  76,  77,  71,  // 0x50
    72,  73,  82,  83,  86,  127, 116, 117, 183, 184, 185, 186, 187, 188, 189, 190, // 0x60
    191, 192, 193, 194, 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x70
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x80
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0x90
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xA0
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xB0
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xC0
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   // 0xD0
    29,  42,  56,  125, 97,  54,  100, 126,                                          // 0xE0
};

struct KeyDef {
    int usage;
    const char *name;
    const char *shortName;
    const char *group;
};

const KeyDef kKeys[] = {
    {0x04, "A", "A", "Letters"}, {0x05, "B", "B", "Letters"}, {0x06, "C", "C", "Letters"}, {0x07, "D", "D", "Letters"},
    {0x08, "E", "E", "Letters"}, {0x09, "F", "F", "Letters"}, {0x0A, "G", "G", "Letters"}, {0x0B, "H", "H", "Letters"},
    {0x0C, "I", "I", "Letters"}, {0x0D, "J", "J", "Letters"}, {0x0E, "K", "K", "Letters"}, {0x0F, "L", "L", "Letters"},
    {0x10, "M", "M", "Letters"}, {0x11, "N", "N", "Letters"}, {0x12, "O", "O", "Letters"}, {0x13, "P", "P", "Letters"},
    {0x14, "Q", "Q", "Letters"}, {0x15, "R", "R", "Letters"}, {0x16, "S", "S", "Letters"}, {0x17, "T", "T", "Letters"},
    {0x18, "U", "U", "Letters"}, {0x19, "V", "V", "Letters"}, {0x1A, "W", "W", "Letters"}, {0x1B, "X", "X", "Letters"},
    {0x1C, "Y", "Y", "Letters"}, {0x1D, "Z", "Z", "Letters"},
    {0x1E, "1", "1", "Numbers"}, {0x1F, "2", "2", "Numbers"}, {0x20, "3", "3", "Numbers"}, {0x21, "4", "4", "Numbers"},
    {0x22, "5", "5", "Numbers"}, {0x23, "6", "6", "Numbers"}, {0x24, "7", "7", "Numbers"}, {0x25, "8", "8", "Numbers"},
    {0x26, "9", "9", "Numbers"}, {0x27, "0", "0", "Numbers"},
    {0x28, "Enter", "Enter", "Editing"}, {0x29, "Escape", "Esc", "Editing"}, {0x2A, "Backspace", "Bksp", "Editing"},
    {0x2B, "Tab", "Tab", "Editing"}, {0x2C, "Space", "Space", "Editing"},
    {0x2D, "Minus", "-", "Symbols"}, {0x2E, "Equals", "=", "Symbols"}, {0x2F, "Left Bracket", "[", "Symbols"},
    {0x30, "Right Bracket", "]", "Symbols"}, {0x31, "Backslash", "\\", "Symbols"}, {0x32, "Non-US #", "#", "Symbols"},
    {0x33, "Semicolon", ";", "Symbols"}, {0x34, "Quote", "'", "Symbols"}, {0x35, "Grave", "`", "Symbols"},
    {0x36, "Comma", ",", "Symbols"}, {0x37, "Period", ".", "Symbols"}, {0x38, "Slash", "/", "Symbols"},
    {0x39, "Caps Lock", "Caps", "Editing"},
    {0x3A, "F1", "F1", "Function"}, {0x3B, "F2", "F2", "Function"}, {0x3C, "F3", "F3", "Function"}, {0x3D, "F4", "F4", "Function"},
    {0x3E, "F5", "F5", "Function"}, {0x3F, "F6", "F6", "Function"}, {0x40, "F7", "F7", "Function"}, {0x41, "F8", "F8", "Function"},
    {0x42, "F9", "F9", "Function"}, {0x43, "F10", "F10", "Function"}, {0x44, "F11", "F11", "Function"}, {0x45, "F12", "F12", "Function"},
    {0x46, "Print Screen", "PrtSc", "Navigation"}, {0x47, "Scroll Lock", "ScrLk", "Navigation"}, {0x48, "Pause", "Pause", "Navigation"},
    {0x49, "Insert", "Ins", "Navigation"}, {0x4A, "Home", "Home", "Navigation"}, {0x4B, "Page Up", "PgUp", "Navigation"},
    {0x4C, "Delete", "Del", "Navigation"}, {0x4D, "End", "End", "Navigation"}, {0x4E, "Page Down", "PgDn", "Navigation"},
    {0x4F, "Right Arrow", "→", "Navigation"}, {0x50, "Left Arrow", "←", "Navigation"}, {0x51, "Down Arrow", "↓", "Navigation"},
    {0x52, "Up Arrow", "↑", "Navigation"},
    {0x53, "Num Lock", "NumLk", "Keypad"}, {0x54, "Keypad /", "KP /", "Keypad"}, {0x55, "Keypad *", "KP *", "Keypad"},
    {0x56, "Keypad -", "KP -", "Keypad"}, {0x57, "Keypad +", "KP +", "Keypad"}, {0x58, "Keypad Enter", "KP ⏎", "Keypad"},
    {0x59, "Keypad 1", "KP 1", "Keypad"}, {0x5A, "Keypad 2", "KP 2", "Keypad"}, {0x5B, "Keypad 3", "KP 3", "Keypad"},
    {0x5C, "Keypad 4", "KP 4", "Keypad"}, {0x5D, "Keypad 5", "KP 5", "Keypad"}, {0x5E, "Keypad 6", "KP 6", "Keypad"},
    {0x5F, "Keypad 7", "KP 7", "Keypad"}, {0x60, "Keypad 8", "KP 8", "Keypad"}, {0x61, "Keypad 9", "KP 9", "Keypad"},
    {0x62, "Keypad 0", "KP 0", "Keypad"}, {0x63, "Keypad .", "KP .", "Keypad"},
    {0x64, "ISO Backslash", "ISO \\", "Symbols"}, {0x65, "Menu", "Menu", "Editing"},
    {0x68, "F13", "F13", "Function"}, {0x69, "F14", "F14", "Function"}, {0x6A, "F15", "F15", "Function"}, {0x6B, "F16", "F16", "Function"},
    {0x6C, "F17", "F17", "Function"}, {0x6D, "F18", "F18", "Function"}, {0x6E, "F19", "F19", "Function"}, {0x6F, "F20", "F20", "Function"},
    {0x70, "F21", "F21", "Function"}, {0x71, "F22", "F22", "Function"}, {0x72, "F23", "F23", "Function"}, {0x73, "F24", "F24", "Function"},
    {0xE0, "Left Ctrl", "Ctrl", "Modifiers"}, {0xE1, "Left Shift", "Shift", "Modifiers"}, {0xE2, "Left Alt", "Alt", "Modifiers"},
    {0xE3, "Left Super", "Super", "Modifiers"}, {0xE4, "Right Ctrl", "RCtrl", "Modifiers"}, {0xE5, "Right Shift", "RShift", "Modifiers"},
    {0xE6, "Right Alt", "RAlt", "Modifiers"}, {0xE7, "Right Super", "RSuper", "Modifiers"},
};

struct ConsumerDef {
    int usage;
    const char *name;
};

const ConsumerDef kConsumer[] = {
    {0x00CD, "Play / Pause"}, {0x00B5, "Next Track"}, {0x00B6, "Previous Track"}, {0x00B7, "Stop"},
    {0x00E2, "Mute"}, {0x00E9, "Volume Up"}, {0x00EA, "Volume Down"}, {0x0183, "Media Player"},
    {0x006F, "Brightness Up"}, {0x0070, "Brightness Down"}, {0x0192, "Calculator"}, {0x0194, "File Manager"},
    {0x018A, "Email"}, {0x0223, "Browser Home"}, {0x0224, "Browser Back"}, {0x0225, "Browser Forward"},
    {0x0227, "Browser Refresh"}, {0x0221, "Search"}, {0x00B8, "Eject"},
};

const QHash<int, const KeyDef *> &keyIndex()
{
    static const QHash<int, const KeyDef *> idx = [] {
        QHash<int, const KeyDef *> h;
        for (const auto &k : kKeys)
            h.insert(k.usage, &k);
        return h;
    }();
    return idx;
}

} // namespace

KeyNames::KeyNames(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
}

KeyNames *KeyNames::create(QQmlEngine *, QJSEngine *)
{
    if (!s_instance)
        s_instance = new KeyNames;
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

int KeyNames::evdevToHid(int evdev)
{
    if (evdev <= 0)
        return 0;
    for (int u = 0; u < int(std::size(kHidToEvdev)); ++u)
        if (kHidToEvdev[u] == evdev && u != 0x32) // prefer 0x31 for backslash
            return u;
    return 0;
}

int KeyNames::hidToEvdev(int usage)
{
    return usage >= 0 && usage < int(std::size(kHidToEvdev)) ? kHidToEvdev[usage] : 0;
}

int KeyNames::hidFromScanCode(int nativeScanCode)
{
    return evdevToHid(nativeScanCode - 8);
}

QString KeyNames::hidName(int usage)
{
    if (auto *k = keyIndex().value(usage))
        return QString::fromUtf8(k->name);
    return usage ? QStringLiteral("Key 0x%1").arg(usage, 2, 16, QLatin1Char('0')).toUpper().replace(QLatin1String("0X"), QLatin1String("0x")) : QString();
}

QString KeyNames::hidShortName(int usage)
{
    if (auto *k = keyIndex().value(usage))
        return QString::fromUtf8(k->shortName);
    return QStringLiteral("0x%1").arg(usage, 2, 16, QLatin1Char('0'));
}

QString KeyNames::modifierText(int mask)
{
    static const char *names[8] = {"Ctrl", "Shift", "Alt", "Super", "Right Ctrl", "Right Shift", "Right Alt", "Right Super"};
    QStringList parts;
    for (int i = 0; i < 8; ++i)
        if (mask & (1 << i))
            parts << QString::fromLatin1(names[i]);
    return parts.join(QStringLiteral(" + "));
}

QVariantList KeyNames::hidKeys()
{
    QVariantList out;
    for (const auto &k : kKeys)
        out << QVariantMap{{QStringLiteral("usage"), k.usage},
                           {QStringLiteral("name"), QString::fromUtf8(k.name)},
                           {QStringLiteral("short"), QString::fromUtf8(k.shortName)},
                           {QStringLiteral("group"), QString::fromUtf8(k.group)}};
    return out;
}

QString KeyNames::consumerName(int usage)
{
    for (const auto &c : kConsumer)
        if (c.usage == usage)
            return QString::fromUtf8(c.name);
    return QStringLiteral("Media 0x%1").arg(usage, 4, 16, QLatin1Char('0'));
}

QVariantList KeyNames::consumerKeys()
{
    QVariantList out;
    for (const auto &c : kConsumer)
        out << QVariantMap{{QStringLiteral("usage"), c.usage}, {QStringLiteral("name"), QString::fromUtf8(c.name)}};
    return out;
}

} // namespace flux
