if(UNIX AND NOT APPLE)
    install(FILES "${PROJECT_SOURCE_DIR}/packaging/qemby.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
    install(FILES "${PROJECT_SOURCE_DIR}/src/qEmbyApp/resources/svg/qemby_logo.svg"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
        RENAME qemby.svg)
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/qemby")
    install(FILES "${PROJECT_SOURCE_DIR}/libs/qwindowkit/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/qemby" RENAME QWindowKit-LICENSE)
    install(FILES "${qcoro_SOURCE_DIR}/LICENSES/MIT.txt"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/qemby" RENAME QCoro-LICENSE)
    install(FILES "${spdlog_SOURCE_DIR}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/qemby" RENAME spdlog-LICENSE)
    set(CPACK_GENERATOR RPM)
    set(CPACK_PACKAGE_NAME qemby)
    set(CPACK_PACKAGE_VENDOR "qEmby community security build")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Emby/Jellyfin desktop client with secure credential storage")
    set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
    set(CPACK_RPM_PACKAGE_RELEASE "2.security.fc44")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT AND Apache-2.0")
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    set(CPACK_RPM_PACKAGE_AUTOREQPROV ON)
    set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase-gui, qt6-qtwayland, qtkeychain-qt6")
    set(CPACK_RPM_EXCLUDE_FROM_AUTO_FILELIST_ADDITION
        /usr/share/applications /usr/share/icons /usr/share/icons/hicolor
        /usr/share/icons/hicolor/scalable /usr/share/icons/hicolor/scalable/apps
        /usr/share/licenses)
    include(CPack)
endif()
