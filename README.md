Description
===========

"Mouseless navigation". Keyboard navigation of the Linux desktop. 

Use the keyboard to switch between windows and workspaces. 
This program shows a menu with the colored list of windows, marked by letters a-z. 
Colors are configurable. Pressing the key a-z "teleports" to that window. 
Pressing the space bar brings up the previous window. Pressing 1-9 "teleports" to the corresponding workspace. 

![Screenshot](screenshot.png "Screenshot")

(with some artwork from [colourlovers.com](http://www.colourlovers.com/lover/albenaj))

Building
========

    sudo apt-get install build-essential libx11-dev libxmu-dev libglib2.0-dev libgtk2.0-dev libcairo2-dev libgdk-pixbuf2.0-dev libatk1.0-dev qt5-qmake qt5-default
    qmake
    make

Installation
============

The installation consists of simply copying files:

    [ -d ~/.wmjump/ ] || { mkdir ~/.wmjump/ ; cp data/*  ~/.wmjump/ ; }
    sudo install -g root -o root -m 0755 wmjump /usr/bin/
    sudo install -g root -o root -m 0644 wmjump.1 /usr/share/man/man1/

Use
===

The instructions for use and config of wmjump can be found in README or by running:

    man wmjump

