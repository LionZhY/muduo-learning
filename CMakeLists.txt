#设置最低版本和项目名称
cmake_minimum_required(VERSION 3.0)
project(muduo-learning) #锁定项目的工作目录

#设置全局的c++标准 使用C++11标准编译
set (CMAKE_CXX_STANDARD 11) 
set (CMAKE_CXX_STANDARD_REQUIRED True)


#设置库输出目录  所有生成的 .a（静态库）或 .so（动态库）文件会被输出到 <项目根目录>/lib 路径下
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

#示例静态库的输出
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib) # 设置静态库输出路径

#设置全局链接库
set(LIBS
    pthread
)

#添加子目录
add_subdirectory(src)
add_subdirectory(example)