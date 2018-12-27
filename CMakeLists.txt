cmake_minimum_required(VERSION 3.4.1)

add_library( # Sets the name of the library.
             pdfiumjni

             # Sets the library as a shared library.
             SHARED

             src/main/cpp/jni.cpp )

include_directories(src/main/cpp)

add_definitions(-DHAVE_PTHREADS)

# one day we will be possible to replace those lines with find_library(modpdfium-lib modpdfium)
add_library(modpdfium SHARED IMPORTED)
string(REPLACE "/android-pdfium/intermediates/cmake/release/obj/" "/libmodpdfium/intermediates/jniLibs/release/" MODPDFIUM_TARGET "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libmodpdfium.so")
include_directories(libmodpdfium/src/main/cpp/include)
set_target_properties(modpdfium PROPERTIES IMPORTED_LOCATION ${MODPDFIUM_TARGET})

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

find_library( # Sets the name of the path variable.
              android-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              android )

find_library( # Sets the name of the path variable.
              jnigraphics-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              jnigraphics )

target_link_libraries( # Specifies the target library.
                       pdfiumjni

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib} ${android-lib} ${jnigraphics-lib} modpdfium)

