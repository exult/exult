# Common install directory for dependencies.
set(DEPENDENCIES_INSTALL_DIR "${CMAKE_BINARY_DIR}/dependencies/install")

# Map LLVM triple to the NDK's toolchain prefix triple.
if(${ANDROID_LLVM_TRIPLE} STREQUAL armv7-none-linux-androideabi${ANDROID_NATIVE_API_LEVEL})
    set(TRIPLE armv7a-linux-androideabi${ANDROID_NATIVE_API_LEVEL})
elseif(${ANDROID_LLVM_TRIPLE} STREQUAL aarch64-none-linux-android${ANDROID_NATIVE_API_LEVEL})
    set(TRIPLE aarch64-linux-android${ANDROID_NATIVE_API_LEVEL})
elseif(${ANDROID_LLVM_TRIPLE} STREQUAL i686-none-linux-android${ANDROID_NATIVE_API_LEVEL})
    set(TRIPLE i686-linux-android${ANDROID_NATIVE_API_LEVEL})
elseif(${ANDROID_LLVM_TRIPLE} STREQUAL x86_64-none-linux-android${ANDROID_NATIVE_API_LEVEL})
    set(TRIPLE x86_64-linux-android${ANDROID_NATIVE_API_LEVEL})
else()
    set(TRIPLE ${ANDROID_LLVM_TRIPLE})
endif()

# Build up linker flags to find libraries at build and runtime.
foreach(IMPLICIT_LINK_DIRECTORY ${CMAKE_CXX_IMPLICIT_LINK_DIRECTORIES})
  set(LDFLAGS "${LDFLAGS} -L${IMPLICIT_LINK_DIRECTORY} -Wl,-rpath,${IMPLICIT_LINK_DIRECTORY}")
endforeach()

# Construct a shell script that sets autotools-friendly environment variables for the NDK build.
set(ENVFILE "${CMAKE_CURRENT_BINARY_DIR}/env.sh")
file(WRITE ${ENVFILE} "#!/bin/sh\n")
file(APPEND ${ENVFILE} "export CC=${ANDROID_TOOLCHAIN_ROOT}/bin/${TRIPLE}-clang\n")
file(APPEND ${ENVFILE} "export CXX=${ANDROID_TOOLCHAIN_ROOT}/bin/${TRIPLE}-clang++\n")
file(APPEND ${ENVFILE} "export AR=${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-ar\n")
file(APPEND ${ENVFILE} "export RANLIB=${ANDROID_TOOLCHAIN_ROOT}/bin/llvm-ranlib\n")
file(APPEND ${ENVFILE} "export PKG_CONFIG_PATH=${DEPENDENCIES_INSTALL_DIR}/lib/pkgconfig:$PKG_CONFIG_PATH\n")
file(APPEND ${ENVFILE} "export LDFLAGS=\"${ANDROID_LINKER_FLAGS} ${LDFLAGS} $($CC -print-libgcc-file-name)\"\n")

string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
file(APPEND ${ENVFILE} "export CFLAGS=\"${CMAKE_C_FLAGS_${BUILD_TYPE}}\"\n")
file(APPEND ${ENVFILE} "export CXXFLAGS=\"${CMAKE_CXX_FLAGS_${BUILD_TYPE}}\"\n")

# CMake flags to forward to cmake-based dependency builds
set(DEPENDENCIES_CMAKE_DEFAULT_ARGS
  -DANDROID_ABI:STRING=${ANDROID_ABI}
  -DANDROID_NATIVE_API_LEVEL:STRING=${ANDROID_NATIVE_API_LEVEL}
  -DANDROID_NDK:STRING=${ANDROID_NDK}
  -DANDROID_PLATFORM:STRING=${ANDROID_PLATFORM}
  -DANDROID_STL:STRING=${ANDROID_STL}
  -DANDROID_TOOLCHAIN:STRING=${ANDROID_TOOLCHAIN}
  -DBUILD_SHARED_LIBS:BOOL=${BUILD_SHARED_LIBS}
  -DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}
  -DCMAKE_C_COMPILER:STRING=${CMAKE_C_COMPILER}
  -DCMAKE_CXX_COMPILER:STRING=${CMAKE_CXX_COMPILER}
  -DCMAKE_GENERATOR:STRING=${CMAKE_GENERATOR}
  -DCMAKE_MAKE_PROGRAM:FILEPATH=${CMAKE_MAKE_PROGRAM}
  -DCMAKE_TOOLCHAIN_FILE:FILEPATH=${CMAKE_TOOLCHAIN_FILE})

# Get the CPU count to use for parallel builds
include(ProcessorCount)
ProcessorCount(NCPU)

# Pull in dependencies
include(ExternalProject)
#add_subdirectory("fluidlite")
add_subdirectory("ogg")
add_subdirectory("vorbis")
add_subdirectory("fluidsynth")
add_subdirectory("mt32emu")
add_subdirectory("sdl")
add_subdirectory("exult")
