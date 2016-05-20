#!/bin/bash

for i in *.dot
do
    dot -Tpdf $i > ${i/dot/pdf}
done
