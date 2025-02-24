# Caecode

**Caecode** is a lightweight text editor built with **GTK+ 3** and **GtkSourceView**. It’s designed for speed and efficiency, perfect for users who need a simple yet feature-rich editor without consuming excessive memory.

---


## Download

You can download the latest version of Caecode here:

[Download Caecode v0.0.1 (.deb)](https://github.com/gtkrshnaaa/caecode/releases/download/v0.0.1/caecode_0.0.1.deb)

---


## 🎯 Key Features

- 📂 **Open folders** and display file hierarchy in a sidebar.  
- 🔤 **Automatic syntax highlighting** for various programming languages.  
- 🔍 **Quick file search** with auto-selection (similar to Vim's scope).  
- ✍️ **Unsaved changes indicator** with a `*` mark.  
- 🎨 **Theme switching** using `Ctrl + M`.  
- 🏷️ **Line numbers** displayed on the left side of the editor.  
- 💾 **Efficient shortcuts** for basic editing: copy, paste, undo, redo, etc.  

---

## 💻 Run From Source

### 1. **Dependencies**
Make sure you have `GTK+ 3` and `GtkSourceView 3` installed. On Ubuntu, run:  

```bash
sudo apt update
sudo apt install libgtk-3-dev libgtksourceview-3.0-dev
```

### 2. **Compilation**

Clone or download the source code, then run the following commands in the terminal:  

```bash
make
```

To run Caecode after compilation:  

```bash
make run
```

To clean up compiled files:  

```bash
make clean
```

---

## 🎹 Keyboard Shortcuts

| Shortcut        | Function                       |
|-----------------|-------------------------------|
| `Ctrl + O`      | Open folder                   |
| `Ctrl + B`      | Toggle sidebar                |
| `Ctrl + S`      | Save file                     |
| `Ctrl + M`      | Switch theme                  |
| `Ctrl + P`      | Open file search              |
| `Ctrl + C`      | Copy text                     |
| `Ctrl + V`      | Paste text                    |
| `Ctrl + X`      | Cut text                      |
| `Ctrl + Z`      | Undo                          |
| `Ctrl + Y`      | Redo                          |
| `Esc` (in popup) | Close search popup            |
| `Enter` (in popup) | Open selected file from search |

---

## 🌙 Themes

Caecode supports several built-in **GtkSourceView** themes. Use `Ctrl + M` to cycle through themes:  
- Classic  
- Cobalt  
- Kate  
- Oblivion  
- Solarized Dark  
- Solarized Light  
- Tango  
- Yaru  
- Yaru Dark  

---

## 📂 Project Structure

```
caecode/
│
├── caecode.c          # Main source code
├── Makefile           # Build configuration
└── README.md          # Documentation
```

---

## 🛠️ Contributing

Caecode is still in early development. You can contribute by:  
- Reporting bugs  
- Suggesting new features  
- Submitting pull requests  

---

## ⚖️ License

Caecode is released under the **MIT License**. Feel free to use, modify, and distribute it as needed.

---