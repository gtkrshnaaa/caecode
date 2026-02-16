# Makefile for Caecode with GtkSourceView

CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0 gtksourceview-3.0 gio-2.0` -Icore/includes -Wno-deprecated-declarations
LDFLAGS = `pkg-config --libs gtk+-3.0 gtksourceview-3.0 gio-2.0`
TARGET = caecode
SRC = $(wildcard core/src/*.c)

# Paths
BIN_DIR = bin
BUILD_DIR = build
BIN_TARGET = $(BIN_DIR)/$(TARGET)

# Packaging
VERSION ?= 0.0.5
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
	@printf "Package: caecode\nVersion: $(VERSION)\nSection: editors\nPriority: optional\nArchitecture: $(ARCH)\nMaintainer: Unknown <unknown@example.com>\nDepends: libgtk-3-0, libgtksourceview-3.0-1\nDescription: Caecode - lightweight code editor using GTK and GtkSourceView\n" > $(PKG_ROOT)/DEBIAN/control
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

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR)

.PHONY: all run clean builddeb install uninstall