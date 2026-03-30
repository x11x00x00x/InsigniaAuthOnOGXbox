XBE_TITLE = insignia_device_auth
GEN_XISO = $(XBE_TITLE).iso

MBEDTLS_DIR = $(CURDIR)/../third_party/mbedtls
MBEDTLS_SRCS := $(filter-out %/net_sockets.c %/psa_its_file.c %/psa_crypto_storage.c,\
	$(wildcard $(MBEDTLS_DIR)/library/*.c))

SRCS += $(MBEDTLS_SRCS)
SRCS += $(CURDIR)/main.c $(CURDIR)/qrcodegen.c $(CURDIR)/nxdk_entropy.c $(CURDIR)/nxdk_mbedtls_time.c \
	$(CURDIR)/https_client.c

NXDK_DIR ?= $(CURDIR)/../../nxdk
NXDK_NET = y

NXDK_CFLAGS += -I$(MBEDTLS_DIR)/include
NXDK_CFLAGS += -I$(CURDIR)
NXDK_CFLAGS += -DMBEDTLS_CONFIG_FILE=\"mbedtls_nxdk_config.h\"

include $(NXDK_DIR)/Makefile

NXDK_CFLAGS := -I$(CURDIR)/lwip_override $(NXDK_CFLAGS)

$(MBEDTLS_DIR)/library/platform_util.obj: NXDK_CFLAGS += -U_WIN32
$(MBEDTLS_DIR)/library/x509_crt.obj: NXDK_CFLAGS += -U_WIN32

TEXT_XBE_TITLE = insignia_device_auth_text
TEXT_SRCS := $(MBEDTLS_SRCS) $(CURDIR)/main_text.c $(CURDIR)/nxdk_entropy.c $(CURDIR)/nxdk_mbedtls_time.c \
	$(CURDIR)/https_client.c
TEXT_OBJS := $(addsuffix .obj, $(basename $(TEXT_SRCS)))

TEXT_EXE_LIBS := \
	$(NXDK_DIR)/lib/libpdclib.lib \
	$(NXDK_DIR)/lib/libwinapi.lib \
	$(NXDK_DIR)/lib/winmm.lib \
	$(NXDK_DIR)/lib/libnxdk_hal.lib \
	$(NXDK_DIR)/lib/libnxdk.lib
ifneq ($(NXDK_DISABLE_AUTOMOUNT_D),y)
TEXT_EXE_LIBS += $(NXDK_DIR)/lib/libnxdk_automount_d.lib
endif
TEXT_EXE_LIBS += \
	$(NXDK_DIR)/lib/libpbkit.lib \
	$(NXDK_DIR)/lib/nxdk_usb.lib \
	$(NXDK_DIR)/lib/libnxdk_net.lib \
	$(NXDK_DIR)/lib/libxboxrt.lib \
	$(NXDK_DIR)/lib/libzlib.lib \
	$(NXDK_DIR)/lib/libpng.lib \
	$(NXDK_DIR)/lib/libjpeg.lib

main_text.exe: $(TEXT_OBJS) $(NXDK_DIR)/lib/xboxkrnl/libxboxkrnl.lib $(TEXT_EXE_LIBS)
	@echo "[ LD       ] $@"
	$(VE)$(LD) $(NXDK_LDFLAGS) $(LDFLAGS) -out:'$@' $^

$(OUTPUT_DIR)/default2.xbe: main_text.exe $(OUTPUT_DIR) $(CXBE)
	@echo "[ CXBE     ] $@"
	$(VE)$(CXBE) -OUT:$@ -TITLE:$(TEXT_XBE_TITLE) $< $(QUIET)

all: $(OUTPUT_DIR)/default2.xbe
