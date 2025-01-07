#!/bin/bash


PROGRAM='/mnt/c/programming_projects/OS_Lab2/diy-page-cache-larentiina/build/bench'

REPEAT=1
RUNS=10
TOTAL_REAL=0


for ((i = 1; i <= RUNS; i++)); do
    echo "Запуск $i:"
    # Используем time напрямую для замера времени выполнения
    time "$PROGRAM" "$REPEAT"

done

for ((i = 1; i <= RUNS; i++)); do
    echo "Запуск $i:"
    # Используем time напрямую для замера времени выполнения
    perf stat "$PROGRAM" "$REPEAT"

done
