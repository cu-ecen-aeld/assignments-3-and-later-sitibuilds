#!/usr/bin/env sh
# Assignment 1
# Samuel S

filesdir=$1
searchstr=$2

if [ $# -ne 2 ]; then
    echo "Expected 2 arguments but received $#"
    exit 1
elif [ ! -d "$filesdir" ]; then
    echo "Path is not a valid directory: $filesdir"
fi

nfiles=$(ls -1 $filesdir | wc -l);
nmatch=$(grep -R -o $searchstr $filesdir | wc -l);

echo "The number of files are $nfiles and the number of matching lines are $nmatch"