cmake_minimum_required(VERSION 3.21)

project(lpc_analyzer)

# see https://github.com/microsoft/STL/issues/1814#issuecomment-914784312
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

include(AnalyzerSDK/AnalyzerSDKConfig.cmake)

# sanity checks
if(NOT ("${CMAKE_SIZEOF_VOID_P}" STREQUAL "8"))
    message(FATAL_ERROR "only 64bit build supported")
endif()
if(NOT ("${CMAKE_BUILD_TYPE}" STREQUAL "Release"))
    message(NOTICE "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
endif()

# taken from saleae example code...can probably be improved/simplified
function(add_analyzer_plugin TARGET)
    set(options )
    set(single_value_args )
    set(multi_value_args SOURCES)
    cmake_parse_arguments( _p "${options}" "${single_value_args}" "${multi_value_args}" ${ARGN} )


    add_library(${TARGET} MODULE ${_p_SOURCES})
    target_link_libraries(${TARGET} PRIVATE Saleae::AnalyzerSDK)

    set(ANALYZER_DESTINATION "Analyzers")
    install(TARGETS ${TARGET} RUNTIME DESTINATION ${ANALYZER_DESTINATION}
                              LIBRARY DESTINATION ${ANALYZER_DESTINATION})

    set_target_properties(${TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${ANALYZER_DESTINATION}
                                               LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${ANALYZER_DESTINATION})
endfunction()

set(SOURCES
    LpcAnalyzer.cpp
)

#add_executable(lpc_analyzer LpcAnalyzer.cpp)
add_analyzer_plugin(lpc_analyzer SOURCES ${SOURCES})
