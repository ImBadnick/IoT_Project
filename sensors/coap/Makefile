CONTIKI_PROJECT = temperature-control
all: $(CONTIKI_PROJECT)

PLATFORMS_ONLY=cc26x0-cc13x0

# Include CoAP resources
MODULES_REL += ./resources

CONTIKI=../..

# Include the CoAP implementation
include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap

include $(CONTIKI)/Makefile.include
include $(CONTIKI)/Makefile.identify-target
