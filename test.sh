#!/usr/bin/env sh

set -u

purple='\e[0;35m'
reset='\e[0m'

for f in ./examples/*.fala; do
	f="${f##*/}"
	f="${f%.*}"

	echo -e "\n${purple}TESTING INTEPRETED ${f}${reset}"
	if [ -f ./test/$f.sh ]; then
		sh ./test/$f.sh | ./build/fala -i ./examples/$f.fala
	else
		./build/fala -i ./examples/$f.fala
	fi

	echo -e "\n${purple}TESTING COMPILED ${f}${reset}"
	./build/fala -V -c ./examples/$f.fala
done
