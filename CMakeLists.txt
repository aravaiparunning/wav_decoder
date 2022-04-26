cmake_minimum_required(VERSION 3.1)
set(CMAKE_OSX_ARCHITECTURES arm64 x86_64) # build fat if we're on a Mac

# Set the project name and version number. This allows for a user of your
# library or tool to specify a particular version when they include it, as in 
#   find_package(CMakeDemo 1.0 REQUIRED)
project(WaveDecoder VERSION 1.0 LANGUAGES C)
set(WaveDecoder_ROOT, ${CMAKE_CURRENT_LIST_DIR})
set(WaveDecoder_VERSION 1.0)

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
add_library(WaveDecoder STATIC 
  src/wave_parser.c
  )

target_include_directories(WaveDecoder SYSTEM PUBLIC
   ${WaveDecoder_ROOT}/src
   )

# Specify that CMakeDemo requires LAPACK to link properly. Ideally, LAPACK would
# specify LAPACK::LAPACK for linking so that we can avoid using the variables.
# However, each package is different and one must check the documentation to 
# see what variables are defined.
target_link_libraries(WaveDecoder)

