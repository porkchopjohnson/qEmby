#ifndef PLAYBACK_UI_PROBE_H
#define PLAYBACK_UI_PROBE_H

#include <QString>
class MainWindow;
void startWindowChromeProbe(MainWindow &window);
void startPlaybackUiProbe(MainWindow &window, const QString &mediaPath,
                          bool expectFailure, bool delayStartReply, bool finishNaturally = false);

#endif
