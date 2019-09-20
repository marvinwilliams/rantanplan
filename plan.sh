#!/bin/bash

set -euo pipefail

DOMAIN=$1
PROBLEM=$2

./bin/rantanplan_glucose $DOMAIN $PROBLEM -x $3 -o
./../master_thesis/VAL/Validate $DOMAIN $PROBLEM plan.txt
