echo " " >> ~/.bashrc
cur=$(pwd)
# echo export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libtcmalloc.so.4 >> ~/.bashrc
echo export LD_LIBRARY_PATH=${cur}/obj:/usr/lib:/usr/lib64:/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH >> ~/.bashrc
source ~/.bashrc