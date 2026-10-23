#include "mainwindow.h"

#include <QApplication>
#include <QIcon>
#include <QLocale>
#include <QSurfaceFormat>
#include <QThread>
#include <QTranslator>
#include "api/proxymanager.h"
#include "components/mpvcontroller.h"
#include "managers/languagemanager.h"
#include "managers/logmanager.h"
#include "managers/singleapplicationmanager.h"
#include "config/config_keys.h"
#include "config/configstore.h"
#ifdef QEMBY_UI_TESTS
#include <QLineEdit>
#include <QTemporaryDir>
#include <QTimer>
#include <qembycore.h>
#include <services/manager/servermanager.h>
#include "playback_ui_probe.h"
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif
int main(int argc, char *argv[]) {
#ifdef QEMBY_UI_TESTS
  bool testUiLifecycle = false;
  QString testMediaPath;
  bool testMediaFailure = false;
  bool testDelayedStart = false;
  bool testNaturalEnd = false;
  for (int i = 1; i < argc; ++i) {
    testUiLifecycle |= QByteArray(argv[i]) == "--test-ui-lifecycle";
    const QByteArray arg(argv[i]);
    if (arg == "--test-video-render" || arg == "--test-video-failure" || arg == "--test-video-start-race" || arg == "--test-video-eof") {
      if (i + 1 >= argc)
        return 2;
      testMediaPath = QString::fromLocal8Bit(argv[++i]);
      testMediaFailure = arg == "--test-video-failure";
      testDelayedStart = arg == "--test-video-start-race";
      testNaturalEnd = arg == "--test-video-eof";
    }
  }
  QTemporaryDir testHome;
  if (testUiLifecycle || !testMediaPath.isEmpty()) {
    if (!testHome.isValid())
      return 1;
    qputenv("XDG_CONFIG_HOME", testHome.path().toUtf8());
    qputenv("XDG_DATA_HOME", testHome.path().toUtf8());
    qputenv("XDG_CACHE_HOME", testHome.path().toUtf8());
    qputenv("DBUS_SESSION_BUS_ADDRESS", "unix:path=/nonexistent-qemby-test-bus");
  }
#endif
#ifdef Q_OS_WIN
  bool isRDP = GetSystemMetrics(SM_REMOTESESSION) != 0;
  if (isRDP) {
    
    
    qputenv("QT_OPENGL", "software");
  }
#endif

  
  QSurfaceFormat format;
  // A desktop core profile must explicitly select desktop OpenGL. On Qt
  // Wayland/NVIDIA, DefaultRenderableType can select GLES and EGL rejects
  // desktop core-profile context attributes with EGL_BAD_MATCH.
  format.setRenderableType(QSurfaceFormat::OpenGL);
  format.setVersion(3, 3); 
  format.setProfile(QSurfaceFormat::CoreProfile);
  format.setDepthBufferSize(24);  
  format.setStencilBufferSize(8); 
  format.setSwapInterval(1);      
  format.setSwapBehavior(QSurfaceFormat::DoubleBuffer); 
  QSurfaceFormat::setDefaultFormat(format);

  QGuiApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
  QApplication a(argc, argv);
  a.setApplicationName(APP_NAME);
  a.setApplicationVersion(APP_VERSION);
  a.setOrganizationName("AlanHJ");
  a.setOrganizationDomain("github.com/AlanHJ/qEmby");

  LogManager::instance()->init();

  SingleApplicationManager singleApplication;
  const auto singleApplicationResult = singleApplication.start(
      ConfigStore::instance()->get<bool>(ConfigKeys::SingleApplication, false));
  if (singleApplicationResult ==
      SingleApplicationManager::StartResult::SecondaryInstance) {
    return 0;
  }
#if !defined(Q_OS_MACOS) && !defined(Q_OS_MAC)
  QGuiApplication::setDesktopFileName(QStringLiteral("qemby"));
  const QIcon appIcon(QStringLiteral(":/svg/qemby_logo.svg"));
  a.setWindowIcon(appIcon);
#endif

  LanguageManager::instance()->init();

  
  
  ProxyManager::installApplicationFactory();

  MainWindow w;
  QObject::connect(&singleApplication,
                   &SingleApplicationManager::activationRequested, &w, [&w]() {
                     if (w.isMinimized()) {
                       w.setWindowState(w.windowState() & ~Qt::WindowMinimized);
                     }
                     w.show();
                     w.raise();
                     w.activateWindow();
                   });
#if !defined(Q_OS_MACOS) && !defined(Q_OS_MAC)
  w.setWindowIcon(appIcon);
#endif
  w.show();

#ifdef QEMBY_UI_TESTS
  if (!testMediaPath.isEmpty()) {
    QTimer::singleShot(200, &w, [&w, testMediaPath, testMediaFailure, testDelayedStart, testNaturalEnd] {
      startPlaybackUiProbe(w, testMediaPath, testMediaFailure, testDelayedStart, testNaturalEnd);
    });
    return a.exec();
  }
  if (testUiLifecycle) {
    QTimer::singleShot(500, &w, [&w] {
      auto *search = w.findChild<QLineEdit *>("titlebar-search");
      auto *core = w.findChild<QEmbyCore *>();
      if (!search || !core) {
        qCritical("Title bar/search destroyed during KDE style polish");
        qApp->exit(1);
        return;
      }
      core->serverManager()->clearActiveSession();
      search->setText("search-after-server-change");
      if (search->text() != "search-after-server-change") {
        qApp->exit(1);
        return;
      }
      qInfo("KDE title-bar lifetime and active-server-change regression: PASS");
      qApp->exit(0);
    });
    return a.exec();
  }
#endif

  
  
  
  
  
  
  
  
  
  
  
  
  
  MpvController::warmupOnce();

  int ret = a.exec();
  
  return ret;
}
