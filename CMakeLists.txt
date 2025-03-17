cmake_minimum_required(VERSION 3.30)
project(fnostudyqr)

set(CMAKE_CXX_STANDARD 20)

find_package(fmt REQUIRED)
find_package(DCMTK REQUIRED)
add_executable(${PROJECT_NAME} main.cpp
        PatientRecord.cpp
        PatientRecord.hpp
        StudyQueryRetriever.cpp
        StudyQueryRetriever.hpp
        Callbacks.cpp
        Callbacks.hpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt DCMTK::DCMTK)