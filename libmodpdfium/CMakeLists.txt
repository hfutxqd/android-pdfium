cmake_minimum_required(VERSION 3.4.1)

add_library( modpdfium SHARED IMPORTED )

set_target_properties( modpdfium PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/src/main/jniLibs/${ANDROID_ABI}/libmodpdfium.so )
