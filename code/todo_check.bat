@echo off

set Wildcard=*.h *.cpp *.inl *.c

echo ------
echo ------

echo TODOS FOUND:
findstr -s -n -i -l "TODO" %Wildcard%

echo ------
echo ------
