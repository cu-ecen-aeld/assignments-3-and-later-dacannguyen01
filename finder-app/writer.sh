#!/bin/bash

writefile=$1
writestr=$2

if [ "$#" -ne 2 ]
then
	echo "Need 2 parameters"
	exit 1
fi

if mkdir -p $(dirname "${writefile}")
then
	echo "${writestr}" > "${writefile}" 
	echo "${writestr} written to ${writefile}"
	exit 0
else 
	echo "Cannot write to ${writefile}"
	exit 1
fi
