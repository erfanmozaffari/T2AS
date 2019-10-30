#!/bin/bash


#compile openwsn
cd ./openwsn-fw
scons --warn=no-all board=python toolchain=gcc oos_openwsn

# Run OpenWSN   
cd ./../openwsn-sw/software/openvisualizer
sudo scons runweb --sim --simCount=16

echo 'hi'
# ./chronometer.py
#sudo kill -9 `ps -aef | grep 'simCount' | grep -v grep | awk '{print $2}'`