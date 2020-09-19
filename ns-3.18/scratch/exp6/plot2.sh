#!/usr/bin/gnuplot

name1=system("echo $name1")
name2=system("echo $name2")
title=system("echo $title")
label1=system("echo $label1");
label2=system("echo $label2");

set terminal pngcairo font "arial,10" size 500,500
set output title.'.png'

set autoscale
set xrange[0:3]
set yrange[0:*]

# Data columns: X Min 1stQuartile Median 3rdQuartile Max Titles
set bars 4.0
set style fill empty
plot name1.'.csv' using 1:3:2:6:5:xticlabels(label1) with candlesticks notitle whiskerbars, \
	''	using 1:4:4:4:4 with candlesticks lt -1 notitle, \
     name2.'.csv' using 1:3:2:6:5:xticlabels(label2) with candlesticks notitle whiskerbars, \
	''	using 1:4:4:4:4 with candlesticks lt -1 notitle
