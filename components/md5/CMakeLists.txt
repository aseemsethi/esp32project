file(GLOB_RECURSE SOURCES md5/*.c)

idf_component_register(SRCS ${SOURCES}
                       INCLUDE_DIRS .
                       )