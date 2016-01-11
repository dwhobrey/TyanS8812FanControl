# gnuplot
# usage: call "Chart.plot".
# Plots Smart Fan IV pwm/temp data to compare with function d(x).
unset label
set title "Smart Fan IV PWM vs Temp."
set xlabel "Temperature / degrees C."
set ylabel "PWM / 255"
set timestamp "%H:%M:%S %d/%m/%Y"

# p = duty cycle percentage.
p = 0.15
d(x) = 255.0 * p + 9.0 * (x - 42.0) * (1.0 - p)

plot "Chart.dat" index "D30" title "D30" with lines, \
"Chart.dat" index "D45" title "D45" with lines, \
"Chart.dat" index "D60" title "D60" with lines, \
d(x) title "d(x)" with lines;

