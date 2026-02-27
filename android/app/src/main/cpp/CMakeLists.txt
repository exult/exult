cmake_minimum_required(VERSION 3.10.2)
project("ExultAndroidWrapper")

add_library( exult-android-wrapper
             SHARED

             exult_android_main.cc
             android_log_streambuf.cc )

add_subdirectory("dependencies")

find_library( log-lib log )

target_link_libraries( exult-android-wrapper
                       ${log-lib}
                       exult-${EXULT_VERSION} )
