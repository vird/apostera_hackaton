#!/bin/bash
rm main 2>/dev/null
gcc -std=c11 main.c -o main -lOpenCL -lm -lSDL -lv4l2 -lpthread -pthread
