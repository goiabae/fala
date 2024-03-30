max=$(shuf -i 1-100 -n 1)
for i in `seq 0 $max`; do
	shuf -i 1-10000 -n 1
done
