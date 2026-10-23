#include "playback_ui_probe.h"
#include "mainwindow.h"
#include "components/mpvwidget.h"
#include "views/media/playerview.h"
#include "models/media/playerlaunchcontext.h"
#include <qembycore.h>
#include <services/manager/servermanager.h>
#include <QApplication>
#include <QElapsedTimer>
#include <QLayout>
#include <QPointer>
#include <QSharedPointer>
#include <QStackedWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

namespace {
struct ProbeState {
    int starts = 0;
    int stops = 0;
    bool prematureStart = false;
    bool error = false;
    bool requestedStop = false;
    double position = 0.0;
    QElapsedTimer elapsed;
    QPointer<PlayerView> player;
    QPointer<MpvWidget> video;
};
}

void startPlaybackUiProbe(MainWindow &window, const QString &mediaPath,
                          bool expectFailure, bool delayStartReply, bool finishNaturally)
{
    auto state = QSharedPointer<ProbeState>::create();
    state->elapsed.start();
    auto *server = new QTcpServer(&window);
    if (!server->listen(QHostAddress::LocalHost)) {
        qCritical("Playback probe could not start its local mock server");
        qApp->exit(1);
        return;
    }
    QObject::connect(server, &QTcpServer::newConnection, &window, [server, state, delayStartReply] {
        auto *socket = server->nextPendingConnection();
        QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, state, delayStartReply] {
            const QByteArray data = socket->property("buffer").toByteArray() + socket->readAll();
            socket->setProperty("buffer", data);
            const int end = data.indexOf("\r\n\r\n");
            if (end < 0 || socket->property("answered").toBool())
                return;
            const auto lines = data.left(end).split('\n');
            int length = 0;
            for (const auto &line : lines)
                if (line.toLower().startsWith("content-length:"))
                    length = line.mid(line.indexOf(':') + 1).trimmed().toInt();
            if (data.size() < end + 4 + length)
                return;
            socket->setProperty("answered", true);
            const QByteArray path = lines.first().split(' ').value(1).split('?').first();
            bool delay = false;
            if (path == "/Sessions/Playing") {
                ++state->starts;
                state->prematureStart |= !state->video || !state->video->hasPresentedVideo();
                delay = delayStartReply;
            } else if (path == "/Sessions/Playing/Stopped") {
                ++state->stops;
            }
            QByteArray body = "{\"Items\":[],\"TotalRecordCount\":0}";
            if (path.endsWith("/Items/render-test"))
                body = "{\"Id\":\"render-test\",\"Name\":\"Local playback probe\",\"Type\":\"Movie\"}";
            auto reply = [socket, body] {
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                              QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            };
            if (delay)
                QTimer::singleShot(700, socket, reply);
            else
                reply();
        });
    });

    auto *core = window.findChild<QEmbyCore *>();
    ServerProfile profile;
    profile.id = "playback-ui-probe";
    profile.url = QString("http://127.0.0.1:%1").arg(server->serverPort());
    profile.userId = "probe";
    profile.accessToken = "disposable-local-probe-token";
    profile.deviceId = "local-probe-device";
    core->serverManager()->addServer(profile);
    core->serverManager()->setActiveServer(profile.id);

    if (auto *stack = window.findChild<QStackedWidget *>("main-view-stack"))
        stack->hide();
    auto *player = new PlayerView(core, window.centralWidget());
    player->setMinimumSize(640, 360);
    window.centralWidget()->layout()->addWidget(player);
    state->player = player;
    state->video = player->findChild<MpvWidget *>();
    state->video->controller()->setProperty("mute", true);
    QObject::connect(state->video, &MpvWidget::positionChanged, player, [state](double position) {
        state->position = position;
    });
    QObject::connect(state->video, &MpvWidget::errorOccurred, player, [state](const QString &) {
        state->error = true;
    });
    PlayerLaunchContext context;
    context.mediaItem.id = "render-test";
    context.mediaItem.name = "Local playback probe";
    context.mediaItem.type = "Movie";
    player->show();
    window.showMaximized();
    player->playMedia("render-test", "Local playback probe", QUrl::fromLocalFile(mediaPath).toString(),
                      20000000LL, QVariant::fromValue(context));

    auto *poll = new QTimer(&window);
    QObject::connect(poll, &QTimer::timeout, &window, [state, &window, expectFailure, delayStartReply, finishNaturally] {
        if (!state->player || !state->video || !window.isVisible() || state->prematureStart ||
            state->starts > 1 || state->stops > 1 || state->elapsed.elapsed() > 15000) {
            qCritical() << "Playback UI probe failed" << "starts:" << state->starts
                        << "stops:" << state->stops << "prematureStart:" << state->prematureStart;
            if (state->player)
                state->player->stopAndReport();
            qApp->exit(1);
            return;
        }
        if (expectFailure) {
            if (state->error && state->elapsed.elapsed() > 1200) {
                const bool passed = state->starts == 0 && state->stops == 0;
                qInfo() << "Failed-media ghost-session regression:" << (passed ? "PASS" : "FAIL");
                qApp->exit(passed ? 0 : 1);
            }
            return;
        }
        if (!finishNaturally && !state->requestedStop && state->video->hasPresentedVideo() && state->starts == 1 &&
            (delayStartReply || state->position > 2.2)) {
            state->requestedStop = true;
            state->player->stopAndReport();
        }
        if ((state->requestedStop || finishNaturally) && state->stops == 1) {
            qInfo() << (finishNaturally ? "Natural end-of-file session cleanup:" :
                        delayStartReply ? "In-flight playback-start cleanup:" : "Video rendering, resume seek and session cleanup:") << "PASS";
            qApp->exit(0);
        }
    });
    poll->start(100);
}
