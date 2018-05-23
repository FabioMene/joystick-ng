#!/usr/bin/env sh
# Removes and inserts local joystick-ng

systemctl --user stop jng-screensaver-dbus.service

sudo rmmod joystick-ng

sudo insmod ./joystick_ng.ko

systemctl --user start jng-screensaver-dbus.service
