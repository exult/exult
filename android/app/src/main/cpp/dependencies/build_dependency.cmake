# Construct the CMake script
configure_file(CMakeLists.txt.in ${CMAKE_CURRENT_BINARY_DIR}/CMakeLists.txt @ONLY)

# Generate
execute_process(
  COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM} .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
if (result)
  message(FATAL_ERROR "CMake generate failed for ${DEPENDENCY}: ${result}")
endif()

# Build
execute_process(
  COMMAND ${CMAKE_COMMAND} --debug-output --build .
  RESULT_VARIABLE result
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
if (result)
  message(FATAL_ERROR "CMake build failed for ${DEPENDENCY}: ${result}")
endif()

# Define a target with the headers and library
add_library(${DEPENDENCY} SHARED IMPORTED GLOBAL)
set_target_properties(
  ${DEPENDENCY} PROPERTIES
  IMPORTED_LOCATION ${DEPENDENCIES_INSTALL_DIR}/lib/lib${DEPENDENCY}.so
)
set_target_properties(
  ${DEPENDENCY} PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES ${DEPENDENCIES_INSTALL_DIR}/include
)
