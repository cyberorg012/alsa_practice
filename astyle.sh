#! /bin/bash 
for f in $(find . -name '*.c' -or -name '*.cpp' -or -name '*.h' -type f) 
do 
    astyle --style=kr --indent=tab --pad-header --pad-oper $f 
done  
# after formate the code,we need to rm '*.orig' files 
for f in $(find . -name '*.orig' -type f) 
do 
    rm $f 
done
