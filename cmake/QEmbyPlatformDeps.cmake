include_guard(GLOBAL)

function(qemby_append_default_qt_prefixes)
    if(QEMBY_QT_ROOT)
        list(PREPEND CMAKE_PREFIX_PATH "${QEMBY_QT_ROOT}")
        set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH}" PARENT_SCOPE)
    endif()
endfunction()

function(qemby_link_libmpv target)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)
    target_link_libraries(${target} PRIVATE PkgConfig::MPV)
endfunction()
