#!/bin/sh

LIBS='-L${libdir} -ltickit'
CFLAGS='-I${includedir}'

case "$LIBDIR" in
  /usr/lib) ;;
  /usr/local/lib) ;;
  *)
    LIBS="$LIBS -Wl,-rpath -Wl,$LIBDIR"
    ;;
esac

cat <<EOF
libdir=$LIBDIR
includedir=$INCDIR

Name: tickit
Description: Terminal Interface Construction KIT
Version: 0.3RC1
Libs: $LIBS
Cflags: $CFLAGS
EOF
