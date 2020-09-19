#!/usr/bin/gnuplot

# http://blog.thinkoriginally.com/2011/06/14/gnuplot-box-and-whiskers-plot/

name=system("echo $name")
maxy=system("echo $maxy")
units=system("echo $units")
set terminal pngcairo font "arial,10" size 500,500
set output 'data/'.name.'.png'

set autoscale
set xrange[0:11]
set yrange[0:maxy]

# Data columns: X Min 1stQuartile Median 3rdQuartile Max Titles
set bars 4.0
set style fill empty
set datafile separator ','
set xlabel "Probability"
set ylabel name.' '.units
plot name.'.csv' using 1:3:2:6:5 with candlesticks title "Quartiles" whiskerbars, \
  ''         using 1:4:4:4:4 with candlesticks lt -1 notitle
