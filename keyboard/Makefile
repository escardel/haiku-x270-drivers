CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall -Wextra -Wno-multichar -fPIC
HEADERS = -I/boot/system/develop/headers/private/interface -I/boot/system/develop/headers/private/app
fn_shortcuts: FnShortcuts.cpp
	$(CXX) $(CXXFLAGS) $(HEADERS) -shared -o $@ $< -lbe -lmedia
clean:
	rm -f fn_shortcuts

INSTALL_DIR = /boot/home/config/non-packaged/add-ons/input_server/filters
install: fn_shortcuts
	@test ! -e $(INSTALL_DIR)/fn_shortcuts || (echo "Run make uninstall before replacing the loaded add-on."; exit 1)
	mkdir -p $(INSTALL_DIR)
	cp fn_shortcuts $(INSTALL_DIR)/fn_shortcuts
uninstall:
	rm -f $(INSTALL_DIR)/fn_shortcuts
check: test
	./test
test: test.cpp FnShortcuts.cpp
	$(CXX) $(CXXFLAGS) $(HEADERS) -o $@ $< -lbe -lmedia
check-fullscreen: test-fullscreen
	./test-fullscreen
test-fullscreen: test-fullscreen.cpp FnShortcuts.cpp
	$(CXX) $(CXXFLAGS) $(HEADERS) -o $@ $< -lbe -lmedia
.PHONY: clean install uninstall check check-fullscreen

check-popup: test-popup
	./test-popup
test-popup: test-popup.cpp FnShortcuts.cpp
	$(CXX) $(CXXFLAGS) $(HEADERS) -o $@ $< -lbe -lmedia
.PHONY: check-popup
