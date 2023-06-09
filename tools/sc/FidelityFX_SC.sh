#!/bin/sh

### Run the original binary via wine:
# wine $(dirname "$0")/FidelityFX_SC.exe $@

### Run the reverse-engineered python script:
python $(dirname $0)/FidelityFX_SC.py $@
