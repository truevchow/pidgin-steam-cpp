# pidgin-steam-cpp: Steam plugin for Pidgin

WIP. Intended to replace the now defunct https://github.com/EionRobb/pidgin-opensteamworks.

This branch consists of:
* Node.js server: `nodejs/src`, gRPC server as proxy to https://github.com/DoctorMcKay/node-steam-user/
* C++ Pidgin plugin `src`: gRPC client, currently synchronous

## Build

Dependencies:
* C++20
* CMake
* protobuf
  * protoc

Install submodules:
```shell
git submodule update --init --recursive
```

Run Node.js server:

```shell
cd nodejs
npm install
npm run protobuild
npm run start
```

Build Pidgin plugin:
```shell
which protoc  # needs to be available in PATH
cmake --build cmake-build-debug --target pidgin_steam
mv ./cmake-build-debug/libpidgin_steam.so ~/.purple/plugins/
```

Run pidgin:
```shell
/usr/bin/pidgin -d
```

## TODO

- [X] make gRPC client asynchronous, e.g. with cppcoro
- [ ] fix coroutine crashes
  - [ ] on `steam_close`
  - [ ] use multithreading for gRPC client
- [ ] More robust handling of gRPC errors
- [ ] cleanups
  - [ ] better build system?
  - [ ] reduce logging
- [ ] implement Steam protocol, eliminate Node.js server
  - [ ] CM server requests
  - [ ] websockets
  - [ ] encryption
  - [ ] Steam protobufs
  - [ ] Steam protocol
