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

    sudo apt-get install build-essential libx11-dev libxmu-dev libglib2.0-dev libgtk-3-dev libcairo2-dev libgdk-pixbuf2.0-dev libatk1.0-dev qt5-qmake qt5-default
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

What is WM_CLASS ?
==================

The color of a button is determined by the `WM_CLASS` of the window.
To learn `WM_CLASS`, execute command: 

    xprop WM_CLASS

The mouse cursor will become a cross. Click on the window in question.
The output will be something like:

    WM_CLASS(STRING) = "xterm", "UXTerm"

Notice that there are two values: "xterm" and "UXTerm"
The first one, "xterm", is called `res_name` , while the second ("UXTerm") is called `res_class`.
(Together they form a struct `XClassHint` of the `xlib` library.) 

The `wmjump` looks at `res_name`. It ignores `res_class`.


Using xdotool to set WM_CLASS of a window
=========================================

The `res_name` of  a window is typically hard-coded in the application, and
sometimes it is desirable to change it.

A program called `xdotool` allows to change the `res_name` of any window. It is done
in two steps. The first step is to execute:

    xdotool  search --pid   $WINDOW_PID

where `$WINDOW_PID` is the process ID of the process which owns the window.
The result will be a window code (or a list of codes, if there are several windows). 
Let us call it `$WINDOW_CODE`. Then second step is to execute:

    xdotool  set_window --classname NEWNAME  $WINDOW_CODE

where `NEWNAME` is the `res_name` we want.
