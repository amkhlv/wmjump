TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

HEADERS += \
    wmctrl.h

SOURCES += \
    main.c \
    wmctrl.c

unix: CONFIG += link_pkgconfig
unix: PKGCONFIG += gtk+-3.0
unix: PKGCONFIG += glib-2.0
unix: PKGCONFIG += cairo
unix: PKGCONFIG += gdk-pixbuf-2.0
unix: PKGCONFIG += atk
unix: PKGCONFIG += x11
unix: PKGCONFIG += xmu


