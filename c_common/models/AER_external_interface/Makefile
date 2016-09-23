CONFIGS = PRAERIE_Transmitter_Receiver_Instrumented \
         PRAERIE_Transmitter_Receiver_Uninstrumented \
         PRAERIE_Transmitter_Instrumented \
         PRAERIE_Transmitter_Uninstrumented \
         PRAERIE_Receiver_Instrumented \
         PRAERIE_Receiver_Uninstrumented 
PRAERIE_BUILD_DIRS := $(addprefix builds/, $(CONFIGS))

all: $(PRAERIE_BUILD_DIRS)
	for d in $(PRAERIE_BUILD_DIRS); do (cd $$d; "$(MAKE)") || exit $$?; done

clean: $(PRAERIE_BUILD_DIRS)
	for d in $(PRAERIE_BUILD_DIRS); do (cd $$d; "$(MAKE)" clean) || exit $$?; done
