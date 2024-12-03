echo " " >> ~/.bashrc
cur=$(pwd)
echo export LD_LIBRARY_PATH=${cur}/obj:/usr/lib:/usr/lib64:$LD_LIBRARY_PATH >> ~/.bashrc
source ~/.bashrc