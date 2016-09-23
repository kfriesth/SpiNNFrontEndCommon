# If SPINN_DIRS is not defined, this is an error!
ifndef SPINN_DIRS
    $(error SPINN_DIRS is not set.  Please define SPINN_DIRS (possibly by running "source setup" in the spinnaker tools folder))
endif

PRAERIE_IF_BUILD = build
LIB := 1
include $(SPINN_DIRS)/make/Makefile.common

ifndef PRINT_DEBUG
    PRINT_DEBUG = PRODUCTION_CODE
endif

CFLAGS += $(OSPACE) -I include -D$(PRINT_DEBUG)
LDFLAGS += -lspinn_common -lspinn_frontend_common

# Objects
OBJS = praerie_interface.o eieio_interface.o 
BUILD_OBJS = $(OBJS:%.o=$(PRAERIE_IF_BUILD)/%.o)

# Headers
HEADERS = praerie_interface.h eieio_interface.h praerie-typedefs.h byte_manip.h
INSTALL_HEADERS = $(HEADERS:%.h=$(SPINN_INC_DIR)/%.h)

# Libraries
LIBS = libPRAERIE_interface.a
INSTALL_LIBS = $(LIBS:%.a=$(SPINN_LIB_DIR)/%.a)

# Build rules (default)
$(PRAERIE_IF_BUILD)/libPRAERIE_interface.a: $(BUILD_OBJS) 
	$(RM) $@
	$(AR) $@ $(BUILD_OBJS)

$(PRAERIE_IF_BUILD)/%.o: src/%.c $(PRAERIE_IF_BUILD)
	$(CC) $(CFLAGS) -o $@ $<

$(PRAERIE_IF_BUILD):
	$(MKDIR) $@

# Installing rules
install: $(INSTALL_LIBS) $(INSTALL_HEADERS)

$(SPINN_LIB_DIR)/%.a: $(PRAERIE_IF_BUILD)/%.a
	$(CP) $< $@

$(SPINN_INC_DIR)/%.h: include/%.h
	$(CP) $< $@

$(SPINN_MAKE_LIB_DIR)/%: %
	$(CP) $< $@

clean:
	$(RM) $(PRAERIE_IF_BUILD)/libPRAERIE_interface.a $(BUILD_OBJS)

install-clean:
	$(RM) $(INSTALL_LIBS) $(INSTALL_HEADERS)
