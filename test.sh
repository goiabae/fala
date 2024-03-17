#!/usr/bin/env sh

set -eux

for f in ./examples/*.fala; do
	f="${f##*/}"
	f="${f%.*}"

	echo "Testing $f"
	if [ -f ./examples/$f.input ]; then
		./build/fala ./examples/$f.fala < ./examples/$f.input
	else
		./build/fala ./examples/$f.fala
	fi
done
