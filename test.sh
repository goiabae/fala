#!/usr/bin/env sh

set -eu

purple='\e[0;35m'
reset='\e[0;30m'

for f in ./examples/*.fala; do
	f="${f##*/}"
	f="${f%.*}"

	echo -e "\n${purple}TESTING ${f}${reset}"
	if [ -f ./examples/$f.input ]; then
		./build/fala ./examples/$f.fala < ./examples/$f.input
	else
		./build/fala ./examples/$f.fala
	fi
done
