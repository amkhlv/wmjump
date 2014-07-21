#!/bin/bash

(
    pkg-config --cflags --libs glib-2.0 
    pkg-config --cflags --libs gtk+-2.0
    pkg-config --cflags --libs cairo
    pkg-config --cflags --libs gdk-pixbuf-2.0
    pkg-config --cflags --libs atk
    pkg-config --cflags --libs x11
    pkg-config --cflags --libs xmu
) | 
xargs gcc -Wall wmctrl.c main.c -o wmjump

