# Author: Sachin Mathad
# Assignment 1 
# Description: Count and print number of files in the directory and number of matching lines 

#!/bin/bash

# error case if arguments not equal to 2
if [ $# -ne 2 ] 
then 
    echo Input 2 Arguments
    exit 1 
fi 


# count the number of files in the directory 
path=$( dirname $1 )
mkdir -p $path
echo $2 > $1

if [ $? -eq 1 ]
then 
    echo "Text write failed"
    exit 1
fi

if [ ! -e $1 ]
then 
    echo "File not created"
    exit 1
fi


exit 0