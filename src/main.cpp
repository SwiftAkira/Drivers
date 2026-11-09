#include "app/AppController.h"
#include "app/IconProvider.h"
#include "app/ProfileStore.h"
#include "core/DeviceBase.h"
#include "core/DeviceManager.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFontDatabase>
#include <QTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSurfaceFormat>

#include <unistd.h>

using namespace flux;

static QString instanceSocketName()
{
    return QStringLiteral("flux-%1").arg(::getuid());
}

int main(int argc, char *argv[])
{
    QApplication::setApplicationName(QStringLiteral("flux"));
    QApplication::setOrganizationName(QStringLiteral("flux"));
    QApplication::setApplicationDisplayName(QStringLiteral("Flux"));
    QApplication::setApplicationVersion(QStringLiteral(FLUX_VERSION));
    QApplication::setDesktopFileName(QStringLiteral("dev.flux.Flux"));

    // Transparent, rounded, frameless window.
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setAlphaBufferSize(8);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon::fromTheme(QStringLiteral("dev.flux.Flux"), QIcon(QStringLiteral(":/qt/qml/Flux/resources/flux.svg"))));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Settings for LAMZU mice and MADLIONS HE keyboards"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption hiddenOpt(QStringLiteral("hidden"), QStringLiteral("Start in the tray without showing the window."));
    parser.addOption(hiddenOpt);
    QCommandLineOption demoOpt(QStringLiteral("demo"), QStringLiteral("Show simulated devices (no hardware access)."));
    parser.addOption(demoOpt);
    QCommandLineOption shotOpt(QStringLiteral("screenshot"), QStringLiteral("Render every page to PNGs in <dir> and exit (add --demo for simulated devices)."),
                               QStringLiteral("dir"));
    parser.addOption(shotOpt);
    parser.process(app);

    // Single instance: a second launch just raises the existing window.
    if (!parser.isSet(shotOpt)) {
        QLocalSocket probe;
        probe.connectToServer(instanceSocketName());
        if (probe.waitForConnected(300)) {
            probe.write("show\n");
            probe.waitForBytesWritten(300);
            return 0;
        }
    }
    QLocalServer::removeServer(instanceSocketName());
    QLocalServer server;
    server.listen(instanceSocketName());

    QFontDatabase::addApplicationFont(QStringLiteral(":/qt/qml/Flux/resources/fonts/InterVariable.ttf"));
    QFont uiFont(QStringLiteral("Inter Variable"));
    uiFont.setPixelSize(14);
    uiFont.setHintingPreference(QFont::PreferNoHinting);
    app.setFont(uiFont);

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    if (parser.isSet(demoOpt) || parser.isSet(shotOpt))
        flux::g_demoSettings = true;
    DeviceManager devices(nullptr);
    const bool shots = parser.isSet(shotOpt);
    if (parser.isSet(demoOpt))
        devices.enableDemo();
    ProfileStore profiles(nullptr);
    AppController controller(&devices);

    QObject::connect(&server, &QLocalServer::newConnection, &controller, [&] {
        while (QLocalSocket *s = server.nextPendingConnection()) {
            QObject::connect(s, &QLocalSocket::readyRead, s, [s, &controller] {
                if (s->readAll().startsWith("show"))
                    controller.showWindow();
                s->deleteLater();
            });
        }
    });

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.setInitialProperties({{QStringLiteral("startHidden"), parser.isSet(hiddenOpt)}});
    engine.loadFromModule("Flux", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    if (auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst()))
        controller.setMainWindow(win);

    if (shots) {
        auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const QString dir = parser.value(shotOpt);
        QDir().mkpath(dir);
        static const QStringList pages = {
            "mouse/overview", "mouse/performance", "mouse/sensor", "mouse/buttons", "mouse/macros", "mouse/power",
            "keyboard/overview", "keyboard/actuation", "keyboard/advanced", "keyboard/keymap", "keyboard/macros",
            "keyboard/lighting", "keyboard/travel", "keyboard/settings", "profiles", "settings"};
        win->setProperty("visible", true);
        win->resize(1100, qEnvironmentVariableIntValue("FLUX_SHOT_HEIGHT") > 0 ? qEnvironmentVariableIntValue("FLUX_SHOT_HEIGHT") : 740);
        // Only the simulated keyboard gets a lighting effect; never write to real hardware here.
        if (parser.isSet(demoOpt))
            if (QObject *kb = devices.keyboard())
                QMetaObject::invokeMethod(kb, "startFluxEffect", Q_ARG(QString, QStringLiteral("ripple")));
        auto idx = std::make_shared<int>(0);
        auto timer = new QTimer(&app);
        timer->setInterval(2500);
        QObject::connect(timer, &QTimer::timeout, &app, [=, &app] {
            if (*idx > 0) {
                const QString name = pages[*idx - 1];
                win->grabWindow().save(dir + "/" + QString(name).replace('/', '-') + ".png");
            }
            if (*idx >= pages.size()) {
                app.quit();
                return;
            }
            QMetaObject::invokeMethod(win, "go", Q_ARG(QVariant, pages[*idx]));
            if (pages[*idx] == QLatin1String("keyboard/keymap") && qEnvironmentVariableIsSet("FLUX_SHOT_PICKER"))
                QTimer::singleShot(900, win, [win] {
                    if (QObject *p = win->findChild<QObject *>(QStringLiteral("keycodePicker")))
                        QMetaObject::invokeMethod(p, "open");
                });
            ++*idx;
        });
        timer->start();
    }

    return app.exec();
}
