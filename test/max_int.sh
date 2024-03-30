max=$(shuf -i 1-100 -n 1)
echo $max
for i in `seq 1 $max`; do
	shuf -i 1-10000 -n 1
done
