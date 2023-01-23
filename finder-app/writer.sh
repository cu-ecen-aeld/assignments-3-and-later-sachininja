# Author: Sachin Mathad
# Assignment 1 
# Description: Open and write to file arguments given in through CLI 

#!/bin/bash

# error case if arguments not equal to 2
if [ $# -ne 2 ] 
then 
    echo Input 2 Arguments
    exit 1 
fi 

#isolate the path name 
path=$( dirname $1 )
#create directory
mkdir -p $path
#write to file 
echo $2 > $1
#check for errors 
if [ $? -eq 1 ]
then 
    echo "Text write failed"
    exit 1
fi
#check for file create errors 
if [ ! -e $1 ]
then 
    echo "File not created"
    exit 1
fi


exit 0