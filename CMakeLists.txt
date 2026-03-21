cmake_minimum_required(VERSION 3.15)# CMake >= 3.15
project(ftcl VERSION 0.1.0 LANGUAGES CXX)
# 项目名称是ftcl,项目版本0.1.0，使用C++

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
#使用C++ 23
#必须满足，不能降级
#禁止使用编译器扩展

option(BUILD_SHARED_LIBS "Build shared library" ON)
# BUILD_SHARED_LIBS选项对INTERFACE库无效（纯头文件库不生成库文件）
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
# 测试，示例可开关控制。


# All code is header-only (inline implementations)
# Create an INTERFACE library since there are no source files
add_library(ftcl INTERFACE)

# 创建一个名为ftcl的接口库目标（纯头文件库）

target_include_directories(ftcl INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 在源码树/构建树使用时，头文件从include找
# 安装后，从安装前缀下的include找。
# 对于INTERFACE库，使用INTERFACE关键字

# For INTERFACE libraries, use INTERFACE instead of PRIVATE
if(MSVC)
    target_compile_options(ftcl INTERFACE /W4 /WX-)
else()
    target_compile_options(ftcl INTERFACE -Wall -Wextra -Wpedantic)
endif()

# MSVC开高警报/W4
#不报warning当error
#gcc/clang开常用警告。

install(TARGETS ftcl
    EXPORT ftcl-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)
# 按转库目标
#  动态库->lib
#静态库->lib
#Windows的运行时dll/exe->bin
# 头文件include路径也声明了。
install(DIRECTORY include/ DESTINATION include)

install(EXPORT ftcl-targets
    FILE ftcl-targets.cmake
    NAMESPACE ftcl::
    DESTINATION lib/cmake/ftcl
)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
