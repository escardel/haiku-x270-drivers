CXX = g++
CPPFLAGS = -I/boot/system/develop/headers/private/graphics/intel_extreme \
	-I/boot/system/develop/headers/private/graphics/common
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wno-multichar
LDLIBS = -lbe

all: DisplayColor DisplayColorStartup SafeDisplayTest

DisplayColor: DisplayColor.cpp DisplayColor.rdef
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDLIBS)
	rc -o DisplayColor.rsrc DisplayColor.rdef
	xres -o $@ DisplayColor.rsrc
	mimeset -f $@

DisplayColorStartup: Startup.cpp Startup.rdef
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDLIBS)
	rc -o Startup.rsrc Startup.rdef
	xres -o $@ Startup.rsrc
	mimeset -f $@

tests/HardwareTests: tests/HardwareTests.cpp DisplayColor.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDLIBS)

tests/PreviewTests: tests/PreviewTests.cpp DisplayColor.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDLIBS)

check: DisplayColor tests/HardwareTests tests/PreviewTests
	./DisplayColor --self-test
	./tests/HardwareTests
	./tests/PreviewTests

SafeDisplayTest: SafeDisplayTest.cpp DisplayColor.cpp SafeDisplayTest.rdef
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@ $(LDLIBS)
	rc -o SafeDisplayTest.rsrc SafeDisplayTest.rdef
	xres -o $@ SafeDisplayTest.rsrc
	mimeset -f $@

check-safety: SafeDisplayTest
	./SafeDisplayTest --self-test

.PHONY: all check check-safety
