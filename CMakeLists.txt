cmake_minimum_required(VERSION 3.30)
project(fnostudyqr)

set(CMAKE_CXX_STANDARD 20)

find_package(fmt REQUIRED)
find_package(DCMTK REQUIRED)

set(SOURCES main.cpp
src/PatientRecord.cpp
src/StudyQueryRetriever.cpp
src/Callbacks.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src/include)

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt DCMTK::DCMTK)
