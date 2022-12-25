#!/bin/bash

gcc main.c -lpthread

./a.out 5 > test_res.txt
./a.out 10 >> test_res.txt
./a.out 20 >> test_res.txt
./a.out 50 >> test_res.txt
./a.out 100 >> test_res.txt
./a.out 200 >> test_res.txt
./a.out 500 >> test_res.txt
./a.out 1000 >> test_res.txt
./a.out 2000 >> test_res.txt
./a.out 5000 >> test_res.txt
./a.out 10000 >> test_res.txt
./a.out 20000 >> test_res.txt
./a.out 50000 >> test_res.txt
