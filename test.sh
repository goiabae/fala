#!/usr/bin/env bash

set -u

FALA="./build/fala"

if [ -t 1 ]; then
	red='\e[0;31m'
	yellow='\e[0;33m'
	purple='\e[0;35m'
	reset='\e[0m'
	bold='\e[1m'
else
	red=
	yellow=
	purple=
	reset=
	bold=
fi

tmp=$(mktemp -p '' -d falaXXXX)

warn()  { echo -e "${yellow}WARNING: ${1}${reset}"; }
test()  { echo -e "${bold}TEST: ${1}${reset}"; }
error() { echo -e "${red}ERROR: ${1}${reset}"; }

fail_count=0

fail() {
	error "Output of test ${tmp}/${1}.${2} differs from expected output in ./test/${1}.${2}"
	fail_count=$(( fail_count + 1 ))
}

interpret() {
	test=$1
	inter="$2"
	file="$3"
	if [ -f ./test/$test.sh ]; then
		sh ./test/$test.sh > $tmp/$test.in
	elif [ -f ./test/$test.c ]; then
		cc -o $tmp/$test ./test/$test.c
		$tmp/$test $tmp/$test.in $tmp/$test.expected
	else
		touch $tmp/$test.in
	fi
	timeout 10s $inter $file < $tmp/$test.in > $tmp/$test.actual
}

compare() {
	test=$1
	if [ -f ./test/$test.out ]; then
		cmp $tmp/$f.actual ./test/$test.out || fail $test out
	elif [ -f $tmp/$f.expected ]; then
		cmp $tmp/$f.actual $tmp/$f.expected || fail $test out
	else
		warn "Interpreted output file for test \"${test}\" does not exist"
		echo -en "${purple}"
		cat $tmp/$test.actual 1>&2
		echo -en "${reset}"
	fi
}

if [ $# -gt 0 ]; then
	f="$1"

	test "INTERPRETED ${f}"
	interpret $f "$FALA -i" ./examples/$f.fala
	compare $f

	test "COMPILED ${f}"
	$FALA -c -o $tmp/$f.rap ./examples/$f.fala

	if ! [ -f $tmp/$f.rap ]; then
		fail "Compiled file for test \"${f}\" does not exist" fala
	fi
fi

if [ $fail_count -eq '0' ]; then
	rm -r $tmp
	exit 0
else
	echo -e "Test files available at ${tmp}"
	exit 1
fi
