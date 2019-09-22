#!/bin/bash

set -euo pipefail

PROG=$1
LIST=$2
MODE=$3

while read DOMAIN PROBLEM;do
./$PROG $DOMAIN $PROBLEM -x $MODE -o
./../master_thesis/VAL/Validate $DOMAIN $PROBLEM plan.txt
done < $LIST|grep -E "Found plan|valid"
