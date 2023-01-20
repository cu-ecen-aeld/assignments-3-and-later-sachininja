# Author: Sachin Mathad
# Assignment 1 
# Description: Count and print number of files in the directory and number of matching lines 

#!/bin/bash

# error case if arguments not equal to 2
if [ $# -ne 2 ] 
then 
    echo "Input 2 Arguments"
    exit 1 
fi 

# error case if not a directory
if [ ! -d $1 ] 
then 
    echo "Not a directory"
    exit 1 
fi 

# count the number of files in the directory 
NumOfFiles=$(ls $1 | wc -l)

# count the number of files in the directory 
NumOfLines=$( grep -r $2 $1 | wc -l )

# NumOfFiles=$( find $1 | wc -l)



echo "The number of files are $NumOfFiles and the number of matching lines are $NumOfLines"

exit 0