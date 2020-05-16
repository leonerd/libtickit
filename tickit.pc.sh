#!/bin/sh

LIBS='-L${libdir} -ltickit'
CFLAGS='-I${includedir}'

cat <<EOF
libdir=$LIBDIR
includedir=$INCDIR

Name: tickit
Description: Terminal Interface Construction KIT
Version: 0.4.0-RC1
Libs: $LIBS
Cflags: $CFLAGS
EOF
