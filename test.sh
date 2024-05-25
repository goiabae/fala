#!/usr/bin/env sh

set -u

FALA="./build/fala"

# path to raposeitor intepreter
if [ -v 1 ]; then
	 RAP="$1"
else
	RAP=raposeitor
fi

red='\e[0;31m'
yellow='\e[0;33m'
purple='\e[0;35m'
reset='\e[0m'
bold='\e[1m'

warn() {
	echo -e "${yellow}WARNING: ${1}${reset}"
}

test() {
	echo -e "${bold}TEST: ${1}${reset}"
}

tmp=$(mktemp -p '' -d falaXXXX)

fail() {
	echo -e "${red}ERROR: Output of test ${tmp}/${1}.${2} differs from expected output in ./test/${1}.${2}${reset}"
	exit 1
}

interpret() {
	test=$1
	inter="$2"
	file="$3"
	if [ -f ./test/$test.sh ]; then
		sh ./test/$test.sh | $inter $file > $tmp/$test.out 2> /dev/null
	else
		$inter $file > $tmp/$test.out 2> /dev/null
	fi
}

for f in ./examples/*.fala; do
	f="${f##*/}"
	f="${f%.*}"

	test "INTERPRETED ${f}"
	interpret $f "$FALA -i" ./examples/$f.fala

	if [ -f ./test/$f.out ]; then
		cmp $tmp/$f.out ./test/$f.out || fail $f out
	else
		warn "Interpreted output file for test \"${f}\" does not exist"
	fi

	test "COMPILED ${f}"
	$FALA -c -o $tmp/$f.rap ./examples/$f.fala 2> /dev/null

	if ! [ -f $tmp/$f.rap ]; then
		fail "Compiled file for test \"${f}\" does not exist" fala
	fi

	if [ "$RAP" ]; then
		interpret $f "$RAP" $tmp/$f.rap
		if [ -f ./test/$f.out ]; then
			cmp $tmp/$f.out ./test/$f.out || fail $f out
		fi
	fi
done

rm -r $tmp
