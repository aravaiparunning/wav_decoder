cmake_minimum_required(VERSION 3.1)
set(CMAKE_OSX_ARCHITECTURES arm64 x86_64) # build fat if we're on a Mac

# Set the project name and version number. This allows for a user of your
# library or tool to specify a particular version when they include it, as in 
#   find_package(CMakeDemo 1.0 REQUIRED)
project(wave_decoder VERSION 1.0 LANGUAGES C)
set(wave_decoder_ROOT, ${CMAKE_CURRENT_LIST_DIR})
set(wave_decoder_VERSION 1.0)

# Function to prepend the subdirectory to source files in subdirectories
FUNCTION(PREPEND var )
   SET(listVar "")
   FOREACH(f ${${var}})
       LIST(APPEND listVar "${CMAKE_CURRENT_SOURCE_DIR}/${f}")
   ENDFOREACH(f)
   SET(${var} "${listVar}" PARENT_SCOPE)
ENDFUNCTION(PREPEND)


# Add the library CMakeDemo as a target, with the contents of src/ and include/
# as dependencies.
add_library(wave_decoder OBJECT 
  src/wave_parser.c
  )

target_include_directories(wave_decoder SYSTEM PUBLIC
   ${CMAKE_CURRENT_LIST_DIR}/src
   )
   
target_link_libraries(wave_decoder)



install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/src"
  DESTINATION "${CMAKE_INSTALL_PREFIX}/${INC_INSTALL_DIR}/"
  FILES_MATCHING
  PATTERN "*.h")

set_target_properties(
  wave_decoder
  PROPERTIES OUTPUT_NAME lvgl
             ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
             LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
             RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
             PUBLIC_HEADER "${LVGL_PUBLIC_HEADERS}")

install(
  TARGETS wave_decoder
  ARCHIVE DESTINATION "${LIB_INSTALL_DIR}"
  LIBRARY DESTINATION "${LIB_INSTALL_DIR}"
  RUNTIME DESTINATION "${LIB_INSTALL_DIR}"
  PUBLIC_HEADER DESTINATION "${INC_INSTALL_DIR}")