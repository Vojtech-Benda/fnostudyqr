cmake_minimum_required(VERSION 3.30)
project(fnostudyqr LANGUAGES CXX)

# set(CMAKE_CXX_STANDARD 20)
# set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-static -static-libgcc -static-libstdc++")

find_package(fmt REQUIRED)
find_package(DCMTK REQUIRED)

# set(SOURCES main.cpp
#     src/PatientRecord.cpp
#     src/StudyQueryRetriever.cpp
#     src/Callbacks.cpp)

add_executable(${PROJECT_NAME} ${SOURCES})

target_sources(${PROJECT_NAME} PRIVATE src/main.cpp src/PatientRecord.cpp src/StudyQueryRetriever.cpp src/Callbacks.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/include)

target_link_libraries(${PROJECT_NAME} PRIVATE fmt::fmt DCMTK::DCMTK)
target_link_libraries(${PROJECT_NAME} PRIVATE $<$<AND:$<BOOL:${MINGW}>,$<CONFIG:Release>>:-static>)

set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX d)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)