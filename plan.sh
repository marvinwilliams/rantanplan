#!/bin/bash

set -euo pipefail

DOMAIN=$1
PROBLEM=$2

./bin/rantanplan_glucose $1 $2
./../master_thesis/VAL/Validate $1 $2 plan.txt
