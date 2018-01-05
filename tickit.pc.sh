cat <<EOF
libdir=$LIBDIR
includedir=$INCDIR

Name: tickit
Description: Terminal Interface Construction KIT
Version: 0.2
Libs: -L\${libdir} -ltickit
Cflags: -I\${includedir}
EOF
