# Makefile for Caecode with GtkSourceView

CC = gcc
CFLAGS = `pkg-config --cflags gtk+-3.0 gtksourceview-3.0 gio-2.0` -Wno-deprecated-declarations
LDFLAGS = `pkg-config --libs gtk+-3.0 gtksourceview-3.0 gio-2.0`
TARGET = caecode
SRC = caecode.c

# Paths
BIN_DIR = bin
BUILD_DIR = build
BIN_TARGET = $(BIN_DIR)/$(TARGET)

# Packaging
VERSION ?= 0.0.4
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
	@cp $(BIN_TARGET) $(PKG_ROOT)/usr/bin/caecode
	@chmod 755 $(PKG_ROOT)/usr/bin/caecode
	@printf "Package: caecode\nVersion: $(VERSION)\nSection: editors\nPriority: optional\nArchitecture: $(ARCH)\nMaintainer: Unknown <unknown@example.com>\nDepends: libgtk-3-0, libgtksourceview-3.0-1\nDescription: Caecode - lightweight code editor using GTK and GtkSourceView\n" > $(PKG_ROOT)/DEBIAN/control
	@mkdir -p $(BUILD_DIR)
	dpkg-deb --build $(PKG_ROOT) $(DEB_FILE)
	@echo "Created $(DEB_FILE)"

# Install the generated .deb package
install: builddeb
	@echo "Installing $(DEB_FILE) (may require sudo password)..."
	sudo dpkg -i $(DEB_FILE) || sudo apt -f install -y

# Uninstall the installed package
uninstall:
	@echo "Uninstalling caecode (may require sudo password)..."
	@sudo dpkg -r caecode || { echo "Package 'caecode' may not be installed or removal failed."; exit 0; }

clean:
	rm -rf $(BIN_DIR) $(BUILD_DIR)

.PHONY: all run clean builddeb install uninstall