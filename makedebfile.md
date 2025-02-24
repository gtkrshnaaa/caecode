## Making Caecode .deb File

### 🎯 **1. Create Debian Directory Structure**

Run this command to set up the folder with a 512x512 icon size:

```bash
mkdir -p caecode_0.0.1/{DEBIAN,usr/local/bin,usr/share/applications,usr/share/icons/hicolor/512x512/apps}
```

---

### 📝 **2. Create Debian Control File**

Create the `control` file:

```bash
nano caecode_0.0.1/DEBIAN/control
```

Fill it with:

```
Package: caecode
Version: 0.0.1
Section: editors
Priority: optional
Architecture: amd64
Depends: libgtk-3-0, libgtksourceview-3.0-1
Maintainer: gtkrshnaaa <https://github.com/gtkrshnaaa>
Description: A lightweight code editor built with C and GTK.
 A minimalist code editor with syntax highlighting and folder navigation.
```

---

### 📂 **3. Copy Binary and Icon**

#### 🔨 **Copy Binary**

Make sure the program is compiled:

```bash
make
```

Then copy the binary:

```bash
cp caecode caecode_0.0.1/usr/local/bin/
```

#### 🎨 **Copy Icon**

Copy the 512x512 icon:

```bash
cp assets/logo/caecode.png caecode_0.0.1/usr/share/icons/hicolor/512x512/apps/
```

---

### 🖥️ **4. Create .desktop File**

Create the `.desktop` file:

```bash
nano caecode_0.0.1/usr/share/applications/caecode.desktop
```

Fill it with:

```
[Desktop Entry]
Name=Caecode
Comment=Lightweight code editor
Exec=/usr/local/bin/caecode
Icon=caecode
Terminal=false
Type=Application
Categories=Development;TextEditor;
```

---

### 🔒 **5. Set Permissions**

Set permissions:

```bash
chmod -R 755 caecode_0.0.1/DEBIAN
```

---

### 📦 **6. Build the .deb File**

Run:

```bash
dpkg-deb --build caecode_0.0.1
```

---

### ✅ **7. Test Installation**

To install:

```bash
sudo dpkg -i caecode_0.0.1.deb
```

---