CONTIKI_PROJECT = udp-mobile udp-rsu
PROJECT_SOURCEFILES += metrics_agg.c
CONTIKI = ../../..

all: $(CONTIKI_PROJECT)
# Pick lightweight stack: CSMA + IPv6 (no RPL)
MAKE_MAC     = MAKE_MAC_CSMA
MAKE_NET     = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_NULLROUTING

include $(CONTIKI)/Makefile.include
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"
