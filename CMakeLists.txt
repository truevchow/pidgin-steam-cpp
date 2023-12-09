cmake_minimum_required(VERSION 3.26)
project(pidgin_steam)

set(CMAKE_CXX_STANDARD 20)
#add_compile_options(-DUSE_OPENSSL_CRYPTO=1)
# add_compile_options(-I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/sysprof-4 -pthread -lglib-2.0
#        -I/usr/include/libpurple)

add_subdirectory(third_party/cppcoro)
add_subdirectory(third_party/jsoncpp)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPURPLE REQUIRED glib-2.0 json-glib-1.0 purple libsecret-1 zlib)

set(CPPCORO_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cppcoro/include)

# Find the protobuf package
# find_package(protobuf CONFIG REQUIRED)
#find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# Generate C++ code from the .proto file
set(PROTO_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/protobufs")
set(PROTO_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/protobufs")
set(PROTO_IMPORT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/protobufs_src")
file(GLOB PROTO_IMPORT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/protobufs_src/comm_protobufs/*.proto")

#set(Protobuf_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/protobufs_src/comm_protobufs")
#set(Protobuf_INCLUDE_DIRS "${Protobuf_INCLUDE_DIRS}/protobufs_gen")
#message("Protobuf_INCLUDE_DIRS: ${Protobuf_INCLUDE_DIRS}")
#set_source_files_properties(${PROTO_SRCS} ${PROTO_HDRS} PROPERTIES GENERATED TRUE)

add_library(grpc_wrapper
        ${PROTO_IMPORT_FILES}
        src/grpc_client_wrapper.cpp src/grpc_client_wrapper.h
        ${PROTO_GEN_FILES}
)
target_link_libraries(grpc_wrapper
        ${Protobuf_LIBRARIES}
        gRPC::grpc++
)
set_target_properties(grpc_wrapper PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(grpc_wrapper PUBLIC
        "$<BUILD_INTERFACE:${PROTO_GEN_DIR}>"
)

#target_link_directories(grpc_wrapper PUBLIC
#        "${PROTO_GEN_DIR}")

add_executable(grpc_experiment
        src/grpc_exp.cpp
        src/grpc_client_wrapper.h
        #        ${PROTO_SRCS} ${PROTO_HDRS}
)
target_link_libraries(grpc_experiment
        #        ${LIBPURPLE_LIBRARIES}
        cppcoro
        grpc_wrapper
)

#protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_IMPORT_FILES})
#protobuf_generate_grpc_cpp(PROTO_GRPC_SRCS PROTO_GRPC_HDRS ${PROTO_IMPORT_FILES})
get_target_property(grpc_cpp_plugin_location gRPC::grpc_cpp_plugin LOCATION)

protobuf_generate(
        TARGET grpc_wrapper
        IMPORT_DIRS ${PROTO_IMPORT_DIRS}
        PROTOC_OUT_DIR "${PROTO_GEN_DIR}"
)
protobuf_generate(
    TARGET grpc_wrapper
    LANGUAGE grpc
    GENERATE_EXTENSIONS .grpc.pb.h .grpc.pb.cc
    #        PLUGIN "protoc-gen-grpc=\$<TARGET_FILE:gRPC::grpc_cpp_plugin>"
    PLUGIN "protoc-gen-grpc=${grpc_cpp_plugin_location}"
    IMPORT_DIRS ${PROTO_IMPORT_DIRS}
    PROTOC_OUT_DIR "${PROTO_GEN_DIR}"
)

#file(GLOB_RECURSE PROTO_SRCS CONFIGURE_DEPENDS "${PROTO_IMPORT_DIRS}/*.proto")
#add_custom_target(protobuf_generation ALL
#    COMMAND ${CMAKE_COMMAND} -E echo "Generating protobuf files"
#    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROTO_GEN_DIR}"
#    COMMAND protoc --proto_path=${PROTO_IMPORT_DIRS} --cpp_out=${PROTO_GEN_DIR} --grpc_out=${PROTO_GEN_DIR} --plugin=protoc-gen-grpc=${grpc_cpp_plugin_location} ${PROTO_SRCS}
#    DEPENDS ${PROTO_SRCS}
#)
# add_dependencies(grpc_wrapper protobuf_generation)

file(GLOB PROTO_GEN_SRCS "${PROTO_GEN_DIRS}/*.pb.cc")
file(GLOB PROTO_GEN_HDRS "${PROTO_GEN_DIRS}/*.pb.h")

#protobuf_generate(TARGET pidgin_steam LANGUAGE cpp)
#protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/protobufs/comm_protobufs" ${PROTO_IMPORT_FILES})
set_source_files_properties(${PROTO_GEN_SRCS} ${PROTO_GEN_HDRS} PROPERTIES GENERATED TRUE)
message("Protobuf_INCLUDE_DIRS: ${Protobuf_INCLUDE_DIRS}")

# add_executable(pidgin_steam
#         src/steam_rsa.cpp src/libdummy.cpp src/libdummy.h)
add_library(pidgin_steam SHARED
        #        src/steam_rsa.cpp
        src/libdummy.cpp src/libdummy.h
        src/coro_utils.h
        ${Protobuf_LIBRARIES}
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