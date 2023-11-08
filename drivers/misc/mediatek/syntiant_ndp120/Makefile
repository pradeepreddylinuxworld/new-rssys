subdir-ccflags-y += -I$(srctree)/drivers/misc/syntiant_ndp120/syntiant_ilib \
                    -I$(srctree)/drivers/misc/syntiant_ndp120/syntiant_packager \
                    -I$(srctree)/drivers/misc/syntiant_ndp120/syntiant-dsp-firmware \
                    -I$(srctree)/drivers/misc/syntiant_ndp120/syntiant-firmware 
                    
subdir-ccflags-y += -DCHIP_TYPE_120 -DDEBUG -DDEBUG_VERBOSE -DMAX_SPEED=8000000 -DSPI_READ_DELAY=2

obj-y += syn_ndp.o
syn_ndp-y := \
	syntiant_ndp10x_package_parser.o \
	syntiant_ndp.o \
	syntiant_ndp10x.o \
	syntiant_ndp120.o \
	syntiant_ndp120_mailbox.o \
	syntiant_ndp120_config.o \
	syntiant_ndp120_config_misc.o \
	syntiant_ndp120_ph.o \
	syntiant_portability.o \
	checksum.o \
	syntiant_package.o \
	syntiant_ndp_error.o \
	syntiant_ndp_common.o \
	syntiant_ndp_aux.o \
	syn_ndp_driver.o \
	es_conversion.o 

