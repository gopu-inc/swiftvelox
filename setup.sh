#!/bin/bash

# SwiftFlow Interpreter Setup Script
# Downloads and builds required libraries

set -e

echo "Setting up SwiftFlow Interpreter..."
echo "====================================="

# Create directories
mkdir -p libs/include libs/lib
mkdir -p bin obj

# Check for required tools
echo "Checking for required tools..."
command -v wget >/dev/null 2>&1 || { echo "wget required but not found. Installing..."; sudo apt-get install wget -y; }
command -v cmake >/dev/null 2>&1 || { echo "cmake required but not found. Installing..."; sudo apt-get install cmake -y; }
command -v git >/dev/null 2>&1 || { echo "git required but not found. Installing..."; sudo apt-get install git -y; }

# Install system packages
echo "Installing system packages..."
sudo apt-get update
sudo apt-get install -y build-essential libreadline-dev libpcre3-dev

# Download and build Boehm GC
echo "Downloading Boehm GC..."
cd /tmp
wget https://github.com/ivmai/bdwgc/releases/download/v8.2.2/gc-8.2.2.tar.gz
tar -xzf gc-8.2.2.tar.gz
cd gc-8.2.2
./configure --prefix=$PWD/../../libs --disable-threads
make
make install
cd ../..
rm -rf /tmp/gc-8.2.2 /tmp/gc-8.2.2.tar.gz

# Download and build Jansson
echo "Downloading Jansson..."
cd /tmp
wget https://github.com/akheron/jansson/releases/download/v2.14/jansson-2.14.tar.gz
tar -xzf jansson-2.14.tar.gz
cd jansson-2.14
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=$PWD/../../../libs -DJANSSON_BUILD_DOCS=OFF ..
make
make install
cd ../../..
rm -rf /tmp/jansson-2.14 /tmp/jansson-2.14.tar.gz

# Download LibTomMath
echo "Downloading LibTomMath..."
cd /tmp
git clone --depth 1 https://github.com/libtom/libtommath.git
cd libtommath
make
cp tommath.h $PWD/../../libs/include/
cp libtommath.a $PWD/../../libs/lib/
cd ../..
rm -rf /tmp/libtommath

# Download LibTomCrypt
echo "Downloading LibTomCrypt..."
cd /tmp
git clone --depth 1 https://github.com/libtom/libtomcrypt.git
cd libtomcrypt
make
cp src/headers/tomcrypt.h $PWD/../../libs/include/
cp libtomcrypt.a $PWD/../../libs/lib/
cd ../..
rm -rf /tmp/libtomcrypt

echo "====================================="
echo "Setup complete!"
echo "You can now build SwiftFlow with: make"
echo "Run the interpreter: ./bin/swiftflow"
echo "Or start REPL: ./bin/swiftflow -"
