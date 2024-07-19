echo " " >> ~/.bashrc
cur=$(pwd)
echo export LD_LIBRARY_PATH=${cur}/obj:/usr/lib:$LD_LIBRARY_PATH >> ~/.bashrc
source ~/.bashrc