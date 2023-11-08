# Linux driver folder
#

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))
MTK_CUSTOM_TOUCHPANEL := $(subst ",,$(CONFIG_MTK_CUSTOM_TOUCHPANEL))
DRIVER_PATH_CUSTOM :=$(srctree)/../custom/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/tp_conf/gslX68X
MTK_CUSTOM_TOUCHPANEL := $(subst ",,$(CONFIG_MTK_CUSTOM_TOUCHPANEL))
MTK_GSLX680_H := $(srctree)/../custom/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/tp_conf/gslX68X/mtk_gslX680.h

ccflags-y += -I$(srctree)/../custom/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/tp_conf/gslX68X/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/

$(shell sed -i '/-start-/{n;d}' $(MTK_GSLX680_H))
$(shell sed -i '/-end-/i#include \"$(DRIVER_PATH_CUSTOM)/cust_config/$(MTK_CUSTOM_TOUCHPANEL).h\"' $(MTK_GSLX680_H))

obj-y   +=  gsl_point_id.o
obj-y	+=  mtk_gslX680.o

