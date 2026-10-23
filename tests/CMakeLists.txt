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
    TIMEOUT 30)

# Manual, opt-in check of the actual desktop wallet across separate processes.
qt_add_executable(wallet-probe wallet_probe.cpp)
target_link_libraries(wallet-probe PRIVATE qEmbyCore Qt6::Core)
