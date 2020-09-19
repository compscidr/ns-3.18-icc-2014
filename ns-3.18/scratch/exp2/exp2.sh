# script for exp1 experiment

rm -rf *tr.txt
rm -rf *.csv
rm -rf *.png
rm -rf results

#vary the number of clients
for i in {2,4,6,8,10}
do
  ../../waf --run="exp2 --num_clients=$i --max_x=30 --max_y=30 --total_time=60.0 --runs=30" 
done

mv ../../*tr.txt .

# Transforms the trace files into the data we need for the plots
./analysis.sh

# Plot the csv data files into pngs
for i in *.csv
do
  export name=${i%.*}
  ./plot.sh
done

mkdir results
mv *.csv results
mv *tr.txt results
mv *.png results
