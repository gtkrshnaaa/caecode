#!/bin/bash

# Caecode Auto-Installer Script
# This script installs all necessary dependencies and installs Caecode.

set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

echo -e "${BLUE}=========================${NC}"
echo -e "${BLUE}  Caecode Installer      ${NC}"
echo -e "${BLUE}=========================${NC}"
echo ""

# 1. Update and Install Dependencies
echo -e "${GREEN}[1/3] Installing Dependencies...${NC}"
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    pkg-config \
    libgtk-3-dev \
    libgtksourceview-3.0-dev \
    libvte-2.91-dev \
    libglib2.0-dev

# 2. Build the Application
echo -e "\n${GREEN}[2/3] Building Caecode...${NC}"
if [ -f "Makefile" ]; then
    make clean
    make
else
    echo -e "${RED}Error: Makefile not found. Please run this script from the Caecode root directory.${NC}"
    exit 1
fi

# 3. Install the Application
echo -e "\n${GREEN}[3/3] Installing Caecode to System...${NC}"
sudo make install

echo -e "\n${GREEN}Installation Complete!${NC}"
echo -e "You can now run Caecode by typing '${BLUE}caecode${NC}' in your terminal."
