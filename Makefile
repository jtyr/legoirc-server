NAME = legoirc-server
VERSION = 0.1
DISTVNAME = $(NAME)-$(VERSION)

CC = gcc
CFLAGS = -Wall
LDFLAGS = -lbcm2835
CP = cp
CP_F = $(CP) -f
RM = rm
RM_F = $(RM) -f
RM_RF = $(RM) -rf
ECHO = echo
MKDIR_P = mkdir -p
PERLRUN = perl
DIST_CP = best
TAR = tar
TARFLAGS = cvf
COMPRESS = gzip --best

DESTDIR = 
PREFIX = 
BIN_DIR = $(DESTDIR)$(PREFIX)/usr/bin
CONF_DIR = $(DESTDIR)$(PREFIX)/etc/conf.d
SYSTEMD_DIR = $(DESTDIR)$(PREFIX)/usr/lib/systemd/system

BUILD_DIR = .
BUILD_BIN_DIR = $(BUILD_DIR)/bin
BUILD_CONF_DIR = $(BUILD_DIR)/conf
BUILD_SRC_DIR = $(BUILD_DIR)/src
BUILD_SYSTEMD_DIR = $(BUILD_DIR)/systemd

.PHONY : all \
	clean clean_client clean_server \
	install install_client install_server install_server_service \
	install_server_service_bin install_server_service_conf \
	uninstall uninstall_client uninstall_server uninstall_server_service \
	uninstall_server_service_bin uninstall_server_service_conf

all : legoirc-server

legoirc-client : clean_client
	$(CC) $(CFLAGS) -o $(BUILD_SRC_DIR)/legoirc-client \
		$(BUILD_SRC_DIR)/legoirc-client.c

legoirc-server : clean_server
	$(CC) $(CFLAGS) -o $(BUILD_SRC_DIR)/legoirc-server \
		$(BUILD_SRC_DIR)/legoirc-server.c $(LDFLAGS)

$(BIN_DIR) :
	$(MKDIR_P) $(BIN_DIR)

$(CONF_DIR) :
	$(MKDIR_P) $(CONF_DIR)

$(SYSTEMD_DIR) :
	$(MKDIR_P) $(SYSTEMD_DIR)

install_client : ${BIN_DIR}
	$(CP_F) $(BUILD_SRC_DIR)/legoirc-client $(BIN_DIR)

uninstall_client :
	$(RM_F) $(BIN_DIR)/legoirc-client

install_server : ${BIN_DIR}
	$(CP_F) $(BUILD_SRC_DIR)/legoirc-server $(BIN_DIR)

uninstall_server :
	$(RM_F) $(BIN_DIR)/legoirc-server

install_server_service_bin : ${BIN_DIR}
	$(CP_F) $(BUILD_BIN_DIR)/legoirc-server.sh $(BIN_DIR)
	$(CP_F) $(BUILD_BIN_DIR)/vlc-camera-stream.sh $(BIN_DIR)

uninstall_server_service_bin :
	$(RM_F) $(BIN_DIR)/legoirc-server.sh
	$(RM_F) $(BIN_DIR)/vlc-camera-stream.sh

install_server_service_conf : ${CONF_DIR}
	$(CP_F) $(BUILD_CONF_DIR)/legoirc-server.conf $(CONF_DIR)
	$(CP_F) $(BUILD_CONF_DIR)/vlc-camera-stream.conf $(CONF_DIR)

uninstall_server_service_conf :
	$(RM_F) $(CONF_DIR)/legoirc-server.conf
	$(RM_F) $(CONF_DIR)/vlc-camera-stream.conf

install_server_service :  ${SYSTEMD_DIR} install_server_service_bin \
		install_server_service_conf
	$(CP_F) $(BUILD_SYSTEMD_DIR)/legoirc-server.service $(SYSTEMD_DIR)
	$(CP_F) $(BUILD_SYSTEMD_DIR)/vlc-camera-stream.service $(SYSTEMD_DIR)

uninstall_server_service :
	$(RM_F) $(SYSTEMD_DIR)/legoirc-server.service
	$(RM_F) $(SYSTEMD_DIR)/vlc-camera-stream.service

install : install_server install_server_service install_server_service_bin \
	install_server_service_conf

uninstall : uninstall_server uninstall_server_service uninstall_server_service_bin \
	uninstall_server_service_conf

clean_client:
	$(RM_F) $(BUILD_SRC_DIR)/legoirc-client

clean_server:
	$(RM_F) $(BUILD_SRC_DIR)/legoirc-server

clean_dist :
	$(RM_RF) $(DISTVNAME)*
	$(RM_F) MANIFEST

clean : clean_client clean_server clean_dist

MANIFEST :
	$(PERLRUN) "-MExtUtils::Manifest=mkmanifest" -e mkmanifest
	$(ECHO) 'Makefile' >> ./MANIFEST

create_distdir :
	$(PERLRUN) "-MExtUtils::Manifest=manicopy,maniread" -e \
		"manicopy(maniread(),'$(DISTVNAME)', '$(DIST_CP)');"

dist : clean MANIFEST create_distdir
	$(TAR) $(TARFLAGS) $(DISTVNAME).tar $(DISTVNAME)
	$(RM_RF) $(DISTVNAME)
	$(COMPRESS) $(DISTVNAME).tar
