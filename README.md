# Caecode

**Caecode** is a lightweight text editor built with **GTK+ 3** and **GtkSourceView**. It’s designed for speed and efficiency, perfect for users who need a simple yet feature-rich editor without consuming excessive memory.

---


## Download

You can download the latest version of Caecode here:

[Download Caecode v0.0.2 (.deb)](https://github.com/gtkrshnaaa/caecode/releases/download/v0.0.2/caecode_0.0.2.deb)

---

# Caecode Screenshots

Here are some screenshots from the Caecode app.

![Screenshot 1](assets/screenshot/Screenshot%20from%202025-02-25%2011-13-52.png)
![Screenshot 2](assets/screenshot/Screenshot%20from%202025-02-25%2011-14-12.png)


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
- Yaru Dark  
- Yaru  
- Classic  
- Cobalt  
- Kate  
- Oblivion  
- Solarized Dark  
- Solarized Light  
- Tango  


### Screenshots

Here are some theme screenshots from the Caecode app.

![Theme Screenshot 1](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-14-50.png)
![Theme Screenshot 2](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-15-02.png)
![Theme Screenshot 3](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-15-14.png)
![Theme Screenshot 4](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-15-19.png)
![Theme Screenshot 5](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-15-22.png)
![Theme Screenshot 6](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-15-24.png)
![Theme Screenshot 7](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-15-28.png)
![Theme Screenshot 8](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-15-31.png)
![Theme Screenshot 9](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-15-34.png)
![Theme Screenshot 10](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-15-55.png)
![Theme Screenshot 11](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-15-59.png)
![Theme Screenshot 12](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-16-03.png)
![Theme Screenshot 13](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-16-06.png)
![Theme Screenshot 14](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-16-08.png)
![Theme Screenshot 15](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-16-10.png)
![Theme Screenshot 16](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-16-13.png)
![Theme Screenshot 17](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-16-15.png)
![Theme Screenshot 18](assets/screenshot/caecode-theme/Screenshot%20from%202025-02-25%2011-16-17.png)

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
