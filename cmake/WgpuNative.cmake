# wgpu-native: the WebGPU implementation of the GPU renderer (ADR 0001,
# owner decision 2026-10-18). Pinned prebuilt release archives from
# https://github.com/gfx-rs/wgpu-native/releases/tag/v29.0.1.1, checked by
# the SHA-256 GitHub publishes for each asset; linked statically.
#
# Defines the imported target wgpu::native (headers <webgpu/webgpu.h>,
# <webgpu/wgpu.h>) or, on a platform without a prebuilt archive, nothing
# (ANNO_HAVE_WGPU stays OFF).

set(ANNO_WGPU_TAG v29.0.1.1)
set(_wgpu_base https://github.com/gfx-rs/wgpu-native/releases/download/${ANNO_WGPU_TAG})
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _cpu)
set(_wgpu_asset "")
if(APPLE)
  if(_cpu MATCHES "arm64|aarch64")
    set(_wgpu_asset wgpu-macos-aarch64-release.zip)
    set(_wgpu_hash a5797a37b1adf720bcd5dcffb291edbbd5b7b14be0a3874c28e6393a655a7a3e)
  else()
    set(_wgpu_asset wgpu-macos-x86_64-release.zip)
    set(_wgpu_hash 8e2f7378548ddd0e2cf21e7d864dda46e953f0af724855a33778b85ead206d41)
  endif()
elseif(WIN32 AND MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_wgpu_asset wgpu-windows-x86_64-msvc-release.zip)
  set(_wgpu_hash 7e67d7445c42aeb85e30f88930fd8d7d83ee769e3390aeb1ada75ebf3cf78132)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  if(_cpu MATCHES "aarch64|arm64")
    set(_wgpu_asset wgpu-linux-aarch64-release.zip)
    set(_wgpu_hash 015fcdf1dbae82e614a783cc38017e5399ae0927a889fe9b69c9b664bc61b47a)
  else()
    set(_wgpu_asset wgpu-linux-x86_64-release.zip)
    set(_wgpu_hash 95a4d90c071005a98d03eab348beaa6b07e16eb00d1dcdb9f8348f75eb97ec5a)
  endif()
endif()

set(ANNO_HAVE_WGPU OFF)
if(_wgpu_asset STREQUAL "")
  message(STATUS "wgpu-native: no prebuilt archive for this platform; GPU renderer disabled")
  return()
endif()

FetchContent_Declare(wgpu_native
  URL ${_wgpu_base}/${_wgpu_asset}
  URL_HASH SHA256=${_wgpu_hash})
FetchContent_MakeAvailable(wgpu_native)

add_library(wgpu::native STATIC IMPORTED GLOBAL)
if(WIN32)
  set(_wgpu_lib ${wgpu_native_SOURCE_DIR}/lib/wgpu_native.lib)
else()
  set(_wgpu_lib ${wgpu_native_SOURCE_DIR}/lib/libwgpu_native.a)
endif()
set_target_properties(wgpu::native PROPERTIES IMPORTED_LOCATION ${_wgpu_lib})
target_include_directories(wgpu::native SYSTEM INTERFACE ${wgpu_native_SOURCE_DIR}/include)
# System libraries the static Rust library needs (wgpu-native's own build
# files list these per platform).
if(APPLE)
  target_link_libraries(wgpu::native INTERFACE
    "-framework Metal" "-framework QuartzCore" "-framework Foundation"
    "-framework CoreGraphics" "-framework IOKit" "-framework IOSurface" objc)
elseif(WIN32)
  target_link_libraries(wgpu::native INTERFACE
    d3dcompiler ws2_32 userenv ntdll bcrypt opengl32 propsys runtimeobject)
else()
  target_link_libraries(wgpu::native INTERFACE dl pthread m)
endif()
set(ANNO_HAVE_WGPU ON)
