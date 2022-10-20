cmake_minimum_required(VERSION 3.4.1)

# build native_app_glue as a static lib
set(APP_GLUE_DIR ${ANDROID_NDK}/sources/android/native_app_glue)
include_directories(${APP_GLUE_DIR})
add_library( app-glue STATIC ${APP_GLUE_DIR}/android_native_app_glue.c)

set(SRC_DIR ${CMAKE_SOURCE_DIR})
get_filename_component(REPO_ROOT_DIR
        ${CMAKE_SOURCE_DIR}/../../../../..  ABSOLUTE)
set(COMMON_DIR ${REPO_ROOT_DIR}/common)
set(THIRD_PARTY_DIR ${REPO_ROOT_DIR}/third_party)


add_library(vktuts SHARED
        VulkanMain.cpp
        AndroidMain.cpp
        CreateShaderModule.cpp
        vulkan_wrapper.cpp
        )

target_include_directories(vktuts PRIVATE
        ${COMMON_DIR}/vulkan_wrapper
        ${COMMON_DIR}/src
        ${THIRD_PARTY_DIR}
        ${THIRD_PARTY_DIR}/shaderc/include
        ${ANDROID_NDK}/sources/android/native_app_glue
        )

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror -std=c++1z \
                     -DVK_USE_PLATFORM_ANDROID_KHR")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -u ANativeActivity_onCreate")

target_link_libraries(vktuts
        app-glue
        ${THIRD_PARTY_DIR}/shaderc/lib/${ANDROID_ABI}/libshaderc.a
        log
        android)