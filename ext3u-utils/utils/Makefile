#################################################
# Copyright 2009								#
#												#
# Antonio Davoli, Vasile Claudiu Perta			#
# ----------------------------------------------#
# Makefile 										#
#################################################

CC = gcc
RM = rm -f
CP = cp
DBG=-Wall -g  
EXT3u_INCLUDE=

BIN_DIR = /usr/bin
MAN_PAGES_DIR = /usr/share/man/man1

ULS_NAME = uls
UNDEL_NAME = urm
USTATS_NAME = ustats

ULS_OBJS = uls.o uls_lib.o
UNDEL_OBJS = urm.o 
USTATS_OBJS = ustats.o uls_lib.o
COMMON_OBJS = ucommon.o 

all: $(ULS_NAME) $(UNDEL_NAME) $(USTATS_NAME)

$(ULS_NAME): $(ULS_OBJS) $(COMMON_OBJS)
	$(CC) $(DBG) $(EXT3u_INCLUDE) -o $(ULS_NAME) $(ULS_OBJS) $(COMMON_OBJS) 

$(UNDEL_NAME): $(UNDEL_OBJS) $(COMMON_OBJS)
	$(CC) $(DBG) $(EXT3u_INCLUDE) -o $(UNDEL_NAME) $(UNDEL_OBJS) $(COMMON_OBJS) 

$(USTATS_NAME): $(USTATS_OBJS) $(COMMON_OBJS)
	$(CC) $(DBG) $(EXT3u_INCLUDE) -o $(USTATS_NAME) $(USTATS_OBJS) $(COMMON_OBJS) 

%.o: %.c
	$(CC) $(DBG) $(EXT3u_INCLUDE) -c $<

install:
	$(CP) $(ULS_NAME) $(BIN_DIR)/$(ULS_NAME)
	$(CP) $(UNDEL_NAME) $(BIN_DIR)/$(UNDEL_NAME)
	$(CP) $(USTATS_NAME) $(BIN_DIR)/$(USTATS_NAME)
	$(CP) ../man/$(ULS_NAME).1.gz $(MAN_PAGES_DIR)
	$(CP) ../man/$(UNDEL_NAME).1.gz $(MAN_PAGES_DIR)
	$(CP) ../man/$(USTATS_NAME).1.gz $(MAN_PAGES_DIR)

uninstall:
	$(RM) $(BIN_DIR)/$(ULS_NAME)
	$(RM) $(BIN_DIR)/$(UNDEL_NAME) 
	$(RM) $(BIN_DIR)/$(USTATS_NAME)
	$(RM) $(MAN_PAGES_DIR)/$(ULS_NAME).1.gz
	$(RM) $(MAN_PAGES_DIR)/$(UNDEL_NAME).1.gz
	$(RM) $(MAN_PAGES_DIR)/$(USTATS_NAME).1.gz 

clean: 
	$(RM) $(ULS_NAME) $(ULS_OBJS) $(UNDEL_NAME) $(USTATS_NAME) $(UNDEL_OBJS) $(USTATS_OBJS)
