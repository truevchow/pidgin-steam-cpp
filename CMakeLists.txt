cmake_minimum_required(VERSION 3.26)
project(pidgin_steam)

set(CMAKE_CXX_STANDARD 20)
#add_compile_options(-DUSE_OPENSSL_CRYPTO=1)
# add_compile_options(-I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/sysprof-4 -pthread -lglib-2.0
#        -I/usr/include/libpurple)

add_subdirectory(third_party/cppcoro)
add_subdirectory(third_party/jsoncpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPURPLE REQUIRED glib-2.0 purple zlib)
pkg_check_modules(GRPC REQUIRED IMPORTED_TARGET grpc)
pkg_check_modules(GRPCPP REQUIRED IMPORTED_TARGET grpc++)

set(CPPCORO_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cppcoro/include)

# Find the protobuf package
# find_package(protobuf CONFIG REQUIRED)
find_package(Protobuf REQUIRED)


# Generate C++ code from the .proto file
set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/protobufs")
set(PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/protobufs")
set(PROTO_IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/protobufs_src")
file(GLOB PROTO_IMPORT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/protobufs_src/comm_protobufs/*.proto")

find_program(GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin REQUIRED)
file(MAKE_DIRECTORY "${PROTO_GEN_DIR}")

file(GLOB PROTO_REL_FILES RELATIVE "${PROTO_IMPORT_DIRS}" "${PROTO_IMPORT_DIRS}/comm_protobufs/*.proto")
set(PROTO_GEN_SRCS "")
set(PROTO_GEN_HDRS "")

foreach(PROTO_FILE ${PROTO_REL_FILES})
    get_filename_component(PROTO_DIR "${PROTO_FILE}" DIRECTORY)
    get_filename_component(PROTO_BASENAME "${PROTO_FILE}" NAME_WE)

    set(GEN_PB_CC "${PROTO_GEN_DIR}/${PROTO_DIR}/${PROTO_BASENAME}.pb.cc")
    set(GEN_PB_H  "${PROTO_GEN_DIR}/${PROTO_DIR}/${PROTO_BASENAME}.pb.h")
    set(GEN_GRPC_CC "${PROTO_GEN_DIR}/${PROTO_DIR}/${PROTO_BASENAME}.grpc.pb.cc")
    set(GEN_GRPC_H  "${PROTO_GEN_DIR}/${PROTO_DIR}/${PROTO_BASENAME}.grpc.pb.h")

    list(APPEND PROTO_GEN_SRCS "${GEN_PB_CC}" "${GEN_GRPC_CC}")
    list(APPEND PROTO_GEN_HDRS "${GEN_PB_H}" "${GEN_GRPC_H}")

    add_custom_command(
            OUTPUT "${GEN_PB_CC}" "${GEN_PB_H}" "${GEN_GRPC_CC}" "${GEN_GRPC_H}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROTO_GEN_DIR}/${PROTO_DIR}"
            COMMAND "${Protobuf_PROTOC_EXECUTABLE}"
                    -I "${PROTO_IMPORT_DIRS}"
                    --cpp_out=${PROTO_GEN_DIR}
                    --grpc_out=${PROTO_GEN_DIR}
                    --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
                    "${PROTO_FILE}"
            DEPENDS "${PROTO_IMPORT_DIRS}/${PROTO_FILE}"
            COMMENT "Generating protobuf/grpc sources for ${PROTO_FILE}"
            VERBATIM
    )
endforeach()

add_custom_target(generate_protos DEPENDS ${PROTO_GEN_SRCS} ${PROTO_GEN_HDRS})

#set(Protobuf_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/protobufs_src/comm_protobufs")
#set(Protobuf_INCLUDE_DIRS "${Protobuf_INCLUDE_DIRS}/protobufs_gen")
#message("Protobuf_INCLUDE_DIRS: ${Protobuf_INCLUDE_DIRS}")
#set_source_files_properties(${PROTO_SRCS} ${PROTO_HDRS} PROPERTIES GENERATED TRUE)

add_library(grpc_wrapper
        src/grpc_client_wrapper.cpp src/grpc_client_wrapper.h
        ${PROTO_GEN_SRCS}
        ${PROTO_GEN_HDRS}
        src/grpc_client_wrapper_async.cpp
        src/grpc_client_wrapper_async.h
        ${CPPCORO_INCLUDE_DIR}
)
target_link_libraries(grpc_wrapper
        protobuf::libprotobuf
        PkgConfig::GRPC
        PkgConfig::GRPCPP
        cppcoro
)
set_target_properties(grpc_wrapper PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_dependencies(grpc_wrapper generate_protos)

target_include_directories(grpc_wrapper PUBLIC
        "$<BUILD_INTERFACE:${PROTO_GEN_DIR}>"
)

#target_link_directories(grpc_wrapper PUBLIC
#        "${PROTO_GEN_DIR}")

add_executable(grpc_experiment
        src/grpc_exp.cpp src/environ.cpp src/environ.h
        src/grpc_client_wrapper.h
        #        ${PROTO_SRCS} ${PROTO_HDRS}
)
target_link_libraries(grpc_experiment
        #        ${LIBPURPLE_LIBRARIES}
        cppcoro
        jsoncpp_lib
        grpc_wrapper
)

set_source_files_properties(${PROTO_GEN_SRCS} ${PROTO_GEN_HDRS} PROPERTIES GENERATED TRUE)
message(STATUS "Protobuf include dirs: ${Protobuf_INCLUDE_DIRS}")

# add_executable(pidgin_steam
#         src/steam_rsa.cpp src/libdummy.cpp src/libdummy.h)
add_library(pidgin_steam SHARED
        #        src/steam_rsa.cpp
        src/libdummy.cpp src/libdummy.h
        src/coro_utils.h
        src/grpc_client_wrapper.h
)

#target_compile_options(pidgin_steam PUBLIC -fsanitize=address)
#target_link_options(pidgin_steam PUBLIC -lasan)

# Print some info
message(STATUS "Libpurple libdir: ${LIBPURPLE_LIBDIR}")
message(STATUS "Plugin dir: ${PLUGIN_DIR_PURPLE}")
message(STATUS "Data root dir: ${DATA_ROOT_DIR_PURPLE}")
message(STATUS "Libpurple include dirs: ${LIBPURPLE_INCLUDE_DIRS}")
message(STATUS "Libpurple library dirs: ${LIBPURPLE_LIBRARY_DIRS}")

set(EXTRA_INCLUDES
        /usr/include/glib-2.0
        /usr/lib/glib-2.0/include
        /usr/include/sysprof-4
        /usr/include/libpurple
)

message("PROTO_SRCS: ${PROTO_SRCS}")
message("PROTO_HDRS: ${PROTO_HDRS}")

target_include_directories(pidgin_steam PRIVATE
    ${LIBPURPLE_INCLUDE_DIRS} ${EXTRA_INCLUDES}
    ${CPPCORO_INCLUDE_DIR}
#    ${PROTO_SRCS} ${PROTO_HDRS}
    ${Protobuf_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}  # needed for protobufs
)

target_link_libraries(pidgin_steam
#    ${LIBPURPLE_LIBRARY_DIRS}  # /usr/lib
    #    /usr/lib/x86_64-linux-gnu
    ${LIBPURPLE_LIBRARIES}
    cppcoro
    jsoncpp_lib
    grpc_wrapper
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

#file(GLOB PROTO_IMPORT_FILES
#        "${CMAKE_CURRENT_SOURCE_DIR}/protubufs_src/generated/**/*.proto"
#        "${CMAKE_CURRENT_SOURCE_DIR}/protubufs_src/generated/**/*.json")