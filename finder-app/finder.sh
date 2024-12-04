#!/bin/bash

filedir=$1
searchstr=$2

if [ "$#" -ne 2 ]
then
	echo "Need 2 parameters "
	exit 1
elif ! [ -d $filedir ] 
then
	echo "$1 is not a valid directory"
	exit 1
else
	echo "The number of files are $(ls -1 ${filedir} -1 | wc -l) and the number of matching lines are $(grep -Rw "${searchstr}" ${filedir} | wc -l)"
	exit 0
fi
