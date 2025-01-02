#!/bin/bash

echo "Do you want to compile with debug or release?"
echo "1) compile with debug, default choice"
echo "2) compile with release"
read -p "Enter choice (1, 2): " choice

case $choice in
  1)
    export BUILD_TYPE=debug
    ;;
  2)
    export BUILD_TYPE=release
    ;;
  *)
    export BUILD_TYPE=debug
    ;;
esac
echo -e "Build with debug type, BUILD_TYPE=${BUILD_TYPE:-Unknown}\n\n"

echo "Do you want to compile with default OPE or online OPE?"
echo "1) compile with default OPE (ope-KS88), default choice"
echo "2) compile with online OPE (PRZB11)"
read -p "Enter choice (1, 2): " choice

case $choice in
  1)
    export ONLINE_OPE=0
    ;;
  2)
    export ONLINE_OPE=1
    ;;
  *)
    export ONLINE_OPE=0
    ;;
esac
echo -e "Build with OPE type is ${ONLINE_OPE:-Unknown}\n\n"

echo "Do you clean all compile files?"
echo "1) Yes"
echo "2) No, default"
read -p "Enter choice (1, 2): " choice

case $choice in
  1)
    echo -e "To clean all compile files\n\n"
    make clean
    ;;
  *)
    ;;
esac

echo -e "\n\nBegin to compile project ......"
make
echo -e "Make done\n\n"

echo "Begin to install crypto library ......"
# 将 BUILD_TYPE 传递给 sudo
sudo -E make install
echo "Install done"