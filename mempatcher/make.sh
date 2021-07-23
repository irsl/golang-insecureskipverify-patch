#!/bin/sh

# yeal, really no deps
# you may want to include: 
gcc -DDEBUG mempatcher.c -o mempatcher

gcc example.c -o example
