# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

#list(APPEND EXTRA_COMPONENT_DIRS components/md5 components/u8g2)
set(EXTRA_COMPONENT_DIRS components)

project(secdev)

#get_filename_component(EXTRA_COMPONENT_DIRS
#   "components/u8g2" ABSOLUTE
#   "components/md5" ABSOLUTE
#)

# Add some external components to the project
#set(IDF_EXTRA_COMPONENT_DIRS ${EXTRA_COMPONENT_DIRS})