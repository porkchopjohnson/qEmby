find_package(Qt6 REQUIRED COMPONENTS Test)
qt_add_executable(security-tests security_tests.cpp)
target_link_libraries(security-tests PRIVATE qEmbyCore Qt6::Test)
add_test(NAME security-regressions COMMAND security-tests)
# Exercise denied/unavailable-wallet behavior without opening the user's wallet.
set_tests_properties(security-regressions PROPERTIES
    ENVIRONMENT "DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent-qemby-test-bus"
    TIMEOUT 60)

add_test(NAME kde-titlebar-lifetime COMMAND qEmbyApp --test-ui-lifecycle)
set_tests_properties(kde-titlebar-lifetime PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_STYLE_OVERRIDE=breeze;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent-qemby-test-bus"
    PASS_REGULAR_EXPRESSION "KDE title-bar lifetime and active-server-change regression: PASS"
    TIMEOUT 30)

add_test(NAME kde-fullscreen-chrome COMMAND qEmbyApp --test-fullscreen-chrome)
set_tests_properties(kde-fullscreen-chrome PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_STYLE_OVERRIDE=breeze"
    PASS_REGULAR_EXPRESSION "Fullscreen title-bar hiding and restore regression: PASS"
    TIMEOUT 15)

set(QEMBY_TEST_MEDIA_FILE "" CACHE FILEPATH "Optional local video fixture for real Wayland playback tests")
if(QEMBY_TEST_MEDIA_FILE)
    add_test(NAME wayland-video-resume COMMAND qEmbyApp --test-video-render "${QEMBY_TEST_MEDIA_FILE}")
    add_test(NAME wayland-playback-start-race COMMAND qEmbyApp --test-video-start-race "${QEMBY_TEST_MEDIA_FILE}")
    add_test(NAME wayland-playback-failure COMMAND qEmbyApp --test-video-failure "${QEMBY_TEST_MEDIA_FILE}.does-not-exist")
    add_test(NAME wayland-playback-eof COMMAND qEmbyApp --test-video-eof "${QEMBY_TEST_MEDIA_FILE}")
    set_tests_properties(wayland-video-resume wayland-playback-start-race wayland-playback-failure wayland-playback-eof PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=wayland;QT_STYLE_OVERRIDE=breeze"
        TIMEOUT 25)
    set_tests_properties(wayland-video-resume PROPERTIES
        PASS_REGULAR_EXPRESSION "Video rendering, resume seek and session cleanup: PASS")
    set_tests_properties(wayland-playback-start-race PROPERTIES
        PASS_REGULAR_EXPRESSION "In-flight playback-start cleanup: PASS")
    set_tests_properties(wayland-playback-failure PROPERTIES
        PASS_REGULAR_EXPRESSION "Failed-media ghost-session regression: PASS")
    set_tests_properties(wayland-playback-eof PROPERTIES
        PASS_REGULAR_EXPRESSION "Natural end-of-file session cleanup: PASS")
endif()

# Manual, opt-in check of the actual desktop wallet across separate processes.
qt_add_executable(wallet-probe wallet_probe.cpp)
target_link_libraries(wallet-probe PRIVATE qEmbyCore Qt6::Core)
