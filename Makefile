# Makefile for Caecode with GtkSourceView

CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0 gtksourceview-3.0 gio-2.0 vte-2.91` -Icore/includes -Wno-deprecated-declarations
LDFLAGS = `pkg-config --libs gtk+-3.0 gtksourceview-3.0 gio-2.0 vte-2.91`
TARGET = caecode
SRC = $(wildcard core/src/*.c)

# Paths
BIN_DIR = bin
BUILD_DIR = build
BIN_TARGET = $(BIN_DIR)/$(TARGET)

# Packaging
VERSION ?= 0.2.7
ARCH ?= $(shell dpkg --print-architecture 2>/dev/null || echo amd64)
PKG_ROOT = $(BUILD_DIR)/caecode_$(VERSION)
DEB_FILE = $(BUILD_DIR)/caecode_$(VERSION)_$(ARCH).deb

all: $(BIN_TARGET)

$(BIN_TARGET): $(SRC)
	@mkdir -p $(BIN_DIR)
	$(CC) -o $(BIN_TARGET) $(SRC) $(CFLAGS) $(LDFLAGS)

run: $(BIN_TARGET)
	./$(BIN_TARGET)

# Build a .deb package using dpkg-deb
builddeb: $(BIN_TARGET)
	@echo "Building .deb package..."
	@rm -rf $(PKG_ROOT)
	@mkdir -p $(PKG_ROOT)/DEBIAN
	@mkdir -p $(PKG_ROOT)/usr/bin
	@mkdir -p $(PKG_ROOT)/usr/share/applications
	@mkdir -p $(PKG_ROOT)/usr/share/icons/hicolor/256x256/apps
	@mkdir -p $(PKG_ROOT)/usr/share/caecode/themes
	@cp $(BIN_TARGET) $(PKG_ROOT)/usr/bin/caecode
	@chmod 755 $(PKG_ROOT)/usr/bin/caecode
	# Install themes
	@cp core/themes/*.xml $(PKG_ROOT)/usr/share/caecode/themes/
	@chmod 644 $(PKG_ROOT)/usr/share/caecode/themes/*.xml
	# Install desktop icon
	@cp assets/logo/caecode.png $(PKG_ROOT)/usr/share/icons/hicolor/256x256/apps/caecode.png
	@chmod 644 $(PKG_ROOT)/usr/share/icons/hicolor/256x256/apps/caecode.png
	# Create desktop entry
	@printf "[Desktop Entry]\n" > $(PKG_ROOT)/usr/share/applications/caecode.desktop
	@printf "Name=Caecode\n" >> $(PKG_ROOT)/usr/share/applications/caecode.desktop
	@printf "Comment=Lightweight code editor using GTK and GtkSourceView\n" >> $(PKG_ROOT)/usr/share/applications/caecode.desktop
	@printf "Exec=caecode\n" >> $(PKG_ROOT)/usr/share/applications/caecode.desktop
	@printf "Icon=caecode\n" >> $(PKG_ROOT)/usr/share/applications/caecode.desktop
	@printf "Terminal=false\n" >> $(PKG_ROOT)/usr/share/applications/caecode.desktop
	@printf "Type=Application\n" >> $(PKG_ROOT)/usr/share/applications/caecode.desktop
	@printf "Categories=Development;IDE;Utility;\n" >> $(PKG_ROOT)/usr/share/applications/caecode.desktop
	@chmod 644 $(PKG_ROOT)/usr/share/applications/caecode.desktop
	@printf "Package: caecode\nVersion: $(VERSION)\nSection: editors\nPriority: optional\nArchitecture: $(ARCH)\nMaintainer: Unknown <unknown@example.com>\nDepends: libgtk-3-0, libgtksourceview-3.0-1, libvte-2.91-0\nDescription: Caecode - lightweight code editor using GTK and GtkSourceView\n" > $(PKG_ROOT)/DEBIAN/control
	@mkdir -p $(BUILD_DIR)
	dpkg-deb --build $(PKG_ROOT) $(DEB_FILE)
	@echo "Created $(DEB_FILE)"

# Install the generated .deb package
install: clean builddeb
	@echo "Installing $(DEB_FILE) (may require sudo password)..."
	sudo dpkg -i $(DEB_FILE) || sudo apt -f install -y

# Uninstall the installed package
uninstall:
	@echo "Uninstalling caecode (may require sudo password)..."
	@sudo dpkg -r caecode || { echo "Package 'caecode' may not be installed or removal failed."; }
	@sudo rm -rf /usr/share/caecode

# Generate a codebase listing for analysis
list:
	@echo "Generating codebase listing..."
	@mkdir -p z_listing
	@printf "%s\n" "================================================================================" > z_listing/caecodelisting.txt
	@printf "%s\n" "PROJECT: Caecode - Lightweight premium code editor for GNOME" >> z_listing/caecodelisting.txt
	@printf "%s\n" "DESCRIPTION: Caecode is a high-performance, minimalist code editor built with" >> z_listing/caecodelisting.txt
	@printf "%s\n" "             GTK+ 3 and VTE. It features a compact terminal UI, sidebar explorer," >> z_listing/caecodelisting.txt
	@printf "%s\n" "             and integrated developer tools with a premium aesthetic." >> z_listing/caecodelisting.txt
	@printf "%s\n" "COPYRIGHT: (c) 2026 Gilang Teja Krishna" >> z_listing/caecodelisting.txt
	@printf "%s\n" "URL: https://github.com/gtkrshnaaa" >> z_listing/caecodelisting.txt
	@printf "%s\n" "================================================================================" >> z_listing/caecodelisting.txt
	@printf "\n" >> z_listing/caecodelisting.txt
	@for file in Makefile *.sh $$(find core -type f); do \
		if [ -f "$$file" ]; then \
			printf "%s\n" "--- FILE: $$file ---" >> z_listing/caecodelisting.txt; \
			cat "$$file" >> z_listing/caecodelisting.txt; \
			printf "\n\n" >> z_listing/caecodelisting.txt; \
		fi \
	done
	@echo "Codebase listing saved to z_listing/caecodelisting.txt"

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR)

.PHONY: all run clean builddeb install uninstall