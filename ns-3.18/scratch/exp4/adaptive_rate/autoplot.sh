#!/usr/bin/gnuplot
set terminal pngcairo font "arial,10" size 500,500
set output '8-Average Delay.dat.png'
set xrange[0:5]
set yrange[0:*]
set bars 4.0
set style fill empty
set xlabel 'Selection Scheme'
set ylabel 'Delay in microseconds'
plot \
  'Lowest Delay/8-Average Delay.dat' using 1:3:2:6:5:xticlabels('Lowest Delay') with candlesticks notitle whiskerbars, \
  ''	using 1:4:4:4:4 with candlesticks lt -1 notitle, \
  'Utility/8-Average Delay.dat' using 1:3:2:6:5:xticlabels('Utility') with candlesticks notitle whiskerbars, \
  ''	using 1:4:4:4:4 with candlesticks lt -1 notitle, \
  'Nearest/8-Average Delay.dat' using 1:3:2:6:5:xticlabels('Nearest') with candlesticks notitle whiskerbars, \
  ''	using 1:4:4:4:4 with candlesticks lt -1 notitle, \
  'Highest Rate/8-Average Delay.dat' using 1:3:2:6:5:xticlabels('Highest Rate') with candlesticks notitle whiskerbars, \
  ''	using 1:4:4:4:4 with candlesticks lt -1 notitle
