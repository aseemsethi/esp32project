#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := secdev

include $(IDF_PATH)/make/project.mk

#$(eval $(call spiffs_create_partition_image,storage,spiffs_image,FLASH_IN_PROJECT))
SPIFFS_IMAGE_FLASH_IN_PROJECT := 1
$(eval $(call spiffs_create_partition_image,storage,spiffs_image))