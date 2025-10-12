n=100
for i in $(seq 1 $n); do
	echo $(shuf -i 1-1000 -n 1)
done
