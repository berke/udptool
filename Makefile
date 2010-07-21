# Makefile

CONFIG=default
DESTINATION=/tmp/mobigo-$(shell date +'%Y%m%d')-$(shell cut -c1-6 .git/refs/heads/master)

include config/$(CONFIG)

.PHONY: all binaries clean dist-clean config help source-package binary-package help doc install

all: binaries

install: binaries
	install build/source/balancer /usr/local/bin/balancer
	install build/source/balancerctl /usr/local/bin/balancerctl

help:
	@echo "Typical usage:"
	@echo "  make CONFIG=default"
	@echo "  make CONFIG=default source-package"
	@echo "  make CONFIG=default binary-package"

source-package:
	@git archive --format tar --prefix mobigo/ HEAD | gzip >$(DESTINATION)-src.tar.gz

binary-package: binaries
	@echo "Building binary package in" $(DESTINATION)-bin
	@rm -rf $(DESTINATION)-bin
	@mkdir -p $(DESTINATION)-bin
	@cp build/source/balancer build/source/balancerctl $(DESTINATION)-bin/
	@cp -a source/scripts $(DESTINATION)-bin/
	@tar czf $(DESTINATION)-bin.tar.gz -C $(shell dirname $(DESTINATION)-bin) $(shell basename $(DESTINATION)-bin)

doc:
	cd source && doxygen

binaries: build/CMakeCache.txt
	make -Cbuild -j$(NUM_CORES)

clean:
	make -Cbuild clean

dist-clean:
	rm -rf build

config: build/CMakeCache.txt

build/CMakeCache.txt:
	mkdir -p build
	cd build && cmake -D CMAKE_BUILD_TYPE=$(BUILD_TYPE) -D BOOST_INCLUDES:PATH=$(BOOST_INCLUDES) -D BOOST_LIBS:PATH=$(BOOST_LIBS) ..
