# Makefile

CONFIG=default
DESTINATION=/tmp/udptool-$(shell date +'%Y%m%d')-$(shell cut -c1-6 .git/refs/heads/master)
BUILD=build.$(CONFIG)

include config/$(CONFIG)

.PHONY: all binaries clean dist-clean config help source-package binary-package help doc install

all: binaries

install: binaries
	install $(BUILD)/source/udptool /usr/local/bin/udptool

help:
	@echo "Typical usage:"
	@echo "  make CONFIG=default"
	@echo "  make CONFIG=default source-package"
	@echo "  make CONFIG=default binary-package"

source-package:
	@git archive --format tar --prefix udprecv/ HEAD | gzip >$(DESTINATION)-src.tar.gz

binary-package: binaries
	@echo "Building binary package in" $(DESTINATION)-bin
	@rm -rf $(DESTINATION)-bin
	@mkdir -p $(DESTINATION)-bin
	@cp $(BUILD)/source/udptool $(DESTINATION)-bin/
	@tar czf $(DESTINATION)-bin.tar.gz -C $(shell dirname $(DESTINATION)-bin) $(shell basename $(DESTINATION)-bin)

doc: doc/udptool-manual.html

doc/udptool-manual.html: doc/udptool-manual.txt
	asciidoc $<

binaries: $(BUILD)/CMakeCache.txt
	make -C$(BUILD) -j$(NUM_CORES)

clean:
	make -C$(BUILD) clean

dist-clean:
	rm -rf $(BUILD)

config: $(BUILD)/CMakeCache.txt

$(BUILD)/CMakeCache.txt:
	mkdir -p $(BUILD)
	cd $(BUILD) && cmake -D CMAKE_BUILD_TYPE=$(BUILD_TYPE) -D BOOST_INCLUDES:PATH=$(BOOST_INCLUDES) -D BOOST_LIBS:PATH=$(BOOST_LIBS) ..
