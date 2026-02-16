# Caecode

**Caecode** is a lightweight, modular code editor built with **GTK+ 3** and **GtkSourceView**. It prioritizes speed, efficiency, and a clean user experience while maintaining a minimal memory footprint.

## Core Features

- **Modular Architecture**: Cleanly separated logic for UI, Editor, Sidebar, and I/O operations.
- **Asynchronous Operations**: Non-blocking folder exploration and file I/O for a responsive UI.
- **Intelligent Sidebar**: Tree-based file hierarchy with automatic filename marking for unsaved changes (`*`).
- **Technical Excellence**:
    - **Fuzzy Search**: Rapid file navigation with a dedicated search interface (`Ctrl + P`).
    - **Syntax Highlighting**: Robust support via GtkSourceView.
    - **Monochrome Themes**: Custom curated Dark and Light monochrome variants.
- **Productivity Focused**: Integrated Vim-like cursor movement shortcuts.

## Modular Architecture

Caecode is structured for maintainability and scalability:

- `core/includes/`: Module interfaces and global application state.
- `core/src/`: Function-specific implementations (UI, Editor, Sidebar, Search, File Ops).
- `core/themes/`: Custom GtkSourceView style schemes.

## Getting Started

### 1. Prerequisites
Ensure you have the development libraries for GTK+ 3 and GtkSourceView 3. On Debian-based systems:

```bash
sudo apt update
sudo apt install libgtk-3-dev libgtksourceview-3.0-dev
```

### 2. Build and Run
Compile the project from the root directory:

```bash
make
```

To run the application immediately after build:

```bash
make run
```

### 3. Installation (.deb)
Caecode supports professional packaging for Linux systems:

```bash
# Build the .deb package
make builddeb

# Install the package system-wide
sudo make install

# Uninstall the package
sudo make uninstall
```

## Keyboard Shortcuts

| Shortcut | Description |
|----------|-------------|
| `Ctrl + O` | Open Folder |
| `Ctrl + S` | Save Current File |
| `Ctrl + B` | Toggle Sidebar Visibility |
| `Ctrl + M` | Toggle Monochrome Themes |
| `Ctrl + P` | Open Quick Search Popup |
| `Ctrl + R` | Reload Current Folder Tree |
| `Ctrl + Q` | Close Currently Opened Folder |
| `Ctrl + I/K/J/L` | Precise Cursor Navigation (Up/Down/Left/Right) |
| `Esc` | Close Search Popup |
| `Enter` | Activate Selection in Search |

## Technical Specification

- **Optimization**: Uses `GtkTreeRowReference` for O(1) sidebar updates and asynchronous GLib I/O tasks.
- **Language**: C / GTK+ 3
- **Build System**: GNU Make
- **Package Manager**: Dpkg (Debian)

---

## License
Caecode is released under the **MIT License**.
