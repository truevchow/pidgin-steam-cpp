cmake_minimum_required(VERSION 3.26)
project(pidgin_steam)

set(CMAKE_CXX_STANDARD 17)
#add_compile_options(-DUSE_OPENSSL_CRYPTO=1)
# add_compile_options(-I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/sysprof-4 -pthread -lglib-2.0
#        -I/usr/include/libpurple)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPURPLE REQUIRED glib-2.0 json-glib-1.0 purple libsecret-1 zlib)

# add_executable(pidgin_steam
#         src/steam_rsa.cpp src/libdummy.cpp src/libdummy.h)
add_library(pidgin_steam SHARED
        #        src/steam_rsa.cpp
        src/libdummy.cpp src/libdummy.h)

# Print some info
message(STATUS "Libpurple libdir: ${LIBPURPLE_LIBDIR}")
message(STATUS "Plugin dir: ${PLUGIN_DIR_PURPLE}")
message(STATUS "Data root dir: ${DATA_ROOT_DIR_PURPLE}")
message(STATUS "Libpurple include dirs: ${LIBPURPLE_INCLUDE_DIRS}")
message(STATUS "Libpurple library dirs: ${LIBPURPLE_LIBRARY_DIRS}")

target_include_directories(pidgin_steam PRIVATE
    ${LIBPURPLE_INCLUDE_DIRS}
    /usr/include/glib-2.0
    /usr/lib/glib-2.0/include
    /usr/include/sysprof-4
    /usr/include/libpurple
)

target_link_libraries(pidgin_steam
#    ${LIBPURPLE_LIBRARY_DIRS}  # /usr/lib
    #    /usr/lib/x86_64-linux-gnu
    ${LIBPURPLE_LIBRARIES}
#    glib-2.0
#    json-glib-1.0
#    secret-1
#    z
)


#link_directories(${LIBPURPLE_LIBRARY_DIRS})

#set(PLUGIN_DIR_PURPLE ${LIBPURPLE_LIBDIR}/purple-2)
#set(DATA_ROOT_DIR_PURPLE ${LIBPURPLE_DATADIR})

#include_directories(src)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

#file(GLOB PROTOBUF_FILES
#        "${CMAKE_CURRENT_SOURCE_DIR}/protubufs_src/generated/**/*.proto"
#        "${CMAKE_CURRENT_SOURCE_DIR}/protubufs_src/generated/**/*.json")