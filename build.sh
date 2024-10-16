#!/bin/sh
set -e
. ./headers.sh

mkdir -p sysroot/sys
mkdir -p sysroot/bin
mkdir -p sysroot/dev

cat > sysroot/sys/config << EOF
shell=/bin/bash
EOF

for PROJECT in $PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE install)
done
