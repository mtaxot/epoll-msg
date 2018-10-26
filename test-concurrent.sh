#!/bin/bash

loop=100

set i = 0
set sleeps = 1
for((i=0; i< loop;i=i+1))
do
    echo '##############################'  ${i}  '###################################'
    sleep 0.01
    ./client2 &

done

