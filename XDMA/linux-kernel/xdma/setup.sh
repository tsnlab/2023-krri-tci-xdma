#!/bin/bash
sudo rmmod xdma
sudo insmod ~/my/xdma/linux-kernel/xdma/xdma.ko
echo 1 | sudo tee /sys/bus/pci/devices/0000:0a:00.0/remove
echo 1 | sudo tee /sys/bus/pci/rescan
sleep 1
sudo ip a add 10.1.1.10/24 dev enp10s0
