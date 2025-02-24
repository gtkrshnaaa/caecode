# Caecode

**Caecode** is a lightweight text editor built with **GTK+ 3** and **GtkSourceView**. It’s designed for speed and efficiency, perfect for users who need a simple yet feature-rich editor without consuming excessive memory.

---


## Download

You can download the latest version of Caecode here:

[Download Caecode v0.0.1 (.deb)](https://github.com/gtkrshnaaa/caecode/releases/download/v0.0.1/caecode_0.0.1.deb)

---

# Caecode Screenshots

Here are some screenshots from the Caecode app.

![Screenshot 1](assets/screenshot/Screenshot%20from%202025-02-24%2021-55-25.png)
![Screenshot 2](assets/screenshot/Screenshot%20from%202025-02-24%2021-55-37.png)
![Screenshot 3](assets/screenshot/Screenshot%20from%202025-02-24%2021-55-42.png)
![Screenshot 4](assets/screenshot/Screenshot%20from%202025-02-24%2021-55-47.png)
![Screenshot 5](assets/screenshot/Screenshot%20from%202025-02-24%2021-55-52.png)
![Screenshot 6](assets/screenshot/Screenshot%20from%202025-02-24%2021-55-58.png)
![Screenshot 7](assets/screenshot/Screenshot%20from%202025-02-24%2021-56-09.png)
![Screenshot 8](assets/screenshot/Screenshot%20from%202025-02-24%2021-56-14.png)
![Screenshot 9](assets/screenshot/Screenshot%20from%202025-02-24%2021-56-24.png)
![Screenshot 10](assets/screenshot/Screenshot%20from%202025-02-24%2021-56-44.png)
![Screenshot 11](assets/screenshot/Screenshot%20from%202025-02-24%2021-57-06.png)
![Screenshot 12](assets/screenshot/Screenshot%20from%202025-02-24%2021-57-13.png)
![Screenshot 13](assets/screenshot/Screenshot%20from%202025-02-24%2021-57-17.png)
![Screenshot 14](assets/screenshot/Screenshot%20from%202025-02-24%2021-57-39.png)
![Screenshot 15](assets/screenshot/Screenshot%20from%202025-02-24%2021-57-42.png)
![Screenshot 16](assets/screenshot/Screenshot%20from%202025-02-24%2021-57-45.png)
![Screenshot 17](assets/screenshot/Screenshot%20from%202025-02-24%2021-57-47.png)
![Screenshot 18](assets/screenshot/Screenshot%20from%202025-02-24%2021-57-50.png)

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