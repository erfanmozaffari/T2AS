#!/bin/bash

cd ./openwsn-fw/
sudo apt-get install -y python-dev
sudo apt-get install -y scons
cd ../openwsn-sw/
sudo apt-get install -y python-pip
sudo apt-get install -y python-tk
sudo pip install -r requirements.txt

