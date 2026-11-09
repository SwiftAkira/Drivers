#include "app/AppController.h"
#include "app/IconProvider.h"
#include "app/ProfileStore.h"
#include "core/DeviceBase.h"
#include "core/DeviceManager.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
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
    QCommandLineOption recordOpt(QStringLiteral("record"), QStringLiteral("Record a scripted demo scene as PNG frames into <dir> (implies --demo)."),
                                 QStringLiteral("dir"));
    parser.addOption(recordOpt);
    QCommandLineOption sceneOpt(QStringLiteral("scene"), QStringLiteral("Scene for --record: tour | effects."), QStringLiteral("name"), QStringLiteral("tour"));
    parser.addOption(sceneOpt);
    parser.process(app);

    // Single instance: a second launch just raises the existing window.
    if (!parser.isSet(shotOpt) && !parser.isSet(recordOpt)) {
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

    if (parser.isSet(demoOpt) || parser.isSet(shotOpt) || parser.isSet(recordOpt))
        flux::g_demoSettings = true;
    DeviceManager devices(nullptr);
    const bool shots = parser.isSet(shotOpt);
    const bool recording = parser.isSet(recordOpt);
    if (parser.isSet(demoOpt) || recording)
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

    if (recording) {
        // Scripted scene → numbered PNG frames + frames.txt with timestamps (ms).
        auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        const QString dir = parser.value(recordOpt);
        QDir().mkpath(dir);
        win->setProperty("visible", true);
        win->resize(1100, 740);
        struct Step { int at; QString page; QString effect; };
        QList<Step> steps;
        int total = 0;
        if (parser.value(sceneOpt) == QLatin1String("effects")) {
            const QStringList fx = {"ripple", "lightning", "water", "soundwave", "flutter", "magnet", "aurora", "spectrum"};
            steps << Step{0, "keyboard/lighting", fx.first()};
            for (int i = 1; i < fx.size(); ++i)
                steps << Step{i * 2600, QString(), fx[i]};
            total = int(fx.size()) * 2600;
        } else {
            const QStringList pages = {"mouse/overview", "mouse/performance", "keyboard/actuation", "keyboard/lighting", "mouse/buttons", "profiles"};
            for (int i = 0; i < pages.size(); ++i)
                steps << Step{i * 2000, pages[i], i == 0 ? QStringLiteral("aurora") : QString()};
            total = int(pages.size()) * 2000;
        }
        auto clock = std::make_shared<QElapsedTimer>();
        auto frame = std::make_shared<int>(0);
        auto next = std::make_shared<int>(0);
        auto log = std::make_shared<QFile>(dir + "/frames.txt");
        log->open(QIODevice::WriteOnly | QIODevice::Truncate);
        // Let the demo devices settle before the clock starts.
        QTimer::singleShot(1200, &app, [=, &app, &devices] {
            clock->start();
            auto timer = new QTimer(&app);
            timer->setInterval(50);
            QObject::connect(timer, &QTimer::timeout, &app, [=, &app, &devices] {
                const qint64 t = clock->elapsed();
                while (*next < steps.size() && steps[*next].at <= t) {
                    const Step &s = steps[*next];
                    if (!s.page.isEmpty())
                        QMetaObject::invokeMethod(win, "go", Q_ARG(QVariant, s.page));
                    if (!s.effect.isEmpty())
                        if (QObject *kb = devices.keyboard())
                            QMetaObject::invokeMethod(kb, "startFluxEffect", Q_ARG(QString, s.effect));
                    ++*next;
                }
                win->grabWindow().save(dir + QStringLiteral("/f%1.png").arg(*frame, 5, 10, QLatin1Char('0')));
                log->write(QByteArray::number(t) + '\n');
                log->flush();
                ++*frame;
                if (t > total)
                    app.quit();
            });
            timer->start();
        });
    }

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
