project(meta_editor)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
file(GLOB srcs "src/*.cpp")
add_executable(meta_editor ${srcs})
target_include_directories(meta_editor PRIVATE
    "include/"
    "spdlog/include/"
    )
