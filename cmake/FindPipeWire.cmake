find_package(PkgConfig QUIET)
pkg_check_modules(PIPEWIRE QUIET libpipewire-0.3)

if(NOT PIPEWIRE_FOUND)
    find_path(PIPEWIRE_INCLUDE_DIRS pipewire/pipewire.h
        PATH_SUFFIXES pipewire-0.3)
    find_library(PIPEWIRE_LIBRARIES pipewire-0.3)

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(PipeWire
        REQUIRED_VARS PIPEWIRE_LIBRARIES PIPEWIRE_INCLUDE_DIRS)
else()
    set(PIPEWIRE_FOUND TRUE)
endif()

mark_as_advanced(PIPEWIRE_INCLUDE_DIRS PIPEWIRE_LIBRARIES)
