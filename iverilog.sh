#!/bin/sh
rm -f *.vvp
rm -f *.vcd
iverilog -o testbench.vvp XorSalsa8.v testbench.v
vvp testbench.vvp
gtkwave xorsalsa8.vcd xorsalsa8.gtkw 
