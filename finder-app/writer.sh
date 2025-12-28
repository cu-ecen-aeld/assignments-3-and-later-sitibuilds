#!/usr/bin/env sh
# Assignment 1
# Samuel S

writefile=$1
writestr=$2

if [ $# -ne 2 ]; then
    echo "Expected 2 arguments but received $#"
    exit 1
fi

mkdir -p "$(dirname "$writefile")"
if [ $? -ne 0 ]; then
    echo "Failed to create path: $writefile"
    exit 1
fi

echo "$writestr" > "$writefile"
if [ $? -ne 0 ]; then
    echo "Failed to create path: $writefile"
    exit 1
fi
