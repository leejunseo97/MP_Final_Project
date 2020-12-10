cmake_minimum_required(VERSION 3.10.2)

project("projectmp")

set(pathOPENCV /Users/leejunseo/AndroidStudioProjects/ProjectMP/sdk)

set(pathLIBOPENCV ${pathOPENCV}/native/libs/${ANDROID_ABI}/libopencv_java3.so)

include_directories(${pathOPENCV}/native/jni/include)

add_library(lib_opencv SHARED IMPORTED)

set_target_properties(lib_opencv PROPERTIES IMPORTED_LOCATION ${pathLIBOPENCV})


add_library( # Sets the name of the library.
        JNIDriver

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        JNIDriver.c )

add_library( # Sets the name of the library.
        OpenCV

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        OpenCV.cpp )

add_library( # Sets the name of the library.
        OpenCLDriver

        # Sets the library as a shared library.
        SHARED

        # Provides a relative path to your source file(s).
        OpenCLDriver.cpp)

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )
find_library( # Sets the name of the path variable.
        jnigraphics-lib

        # Specifies the name of the NDK library that
        # you want CMake to locate.
        jnigraphics )

target_link_libraries( # Specifies the target library.
        JNIDriver
        # Links the target library to the log library
        # included in the NDK.
        ${log-lib} )

target_link_libraries( # Specifies the target library.
        OpenCLDriver

        # Links the target librar toㄹ the log library
        # included in the NDK.
        /Users/leejunseo/Desktop/in_Mac/MP_Class/OpenCL_lib_and_inlclude/lib/libGLES_mali.so ${log-lib} ${jnigraphics-lib} )

target_link_libraries(OpenCV lib_opencv)

target_include_directories(OpenCLDriver PRIVATE src/main/cpp)



