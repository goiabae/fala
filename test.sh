#!/usr/bin/env sh

set -u

purple='\e[0;35m'
reset='\e[0m'

for f in ./examples/*.fala; do
	f="${f##*/}"
	f="${f%.*}"

	echo -e "\n${purple}TESTING INTEPRETED ${f}${reset}"
	if [ -f ./examples/$f.input ]; then
		./build/fala -i ./examples/$f.fala < ./examples/$f.input
	else
		./build/fala -i ./examples/$f.fala
	fi

	echo -e "\n${purple}TESTING COMPILED ${f}${reset}"
	./build/fala -c -o ./examples/$f.rap ./examples/$f.fala
done
