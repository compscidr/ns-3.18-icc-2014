# Transforms the trace files into the data we need for the plots
./analysis.sh

# Plot the csv data files into pngs
for i in *.csv
do
  export name=${i%.*}
  ./plot.sh
done
