#!/bin/bash

expack&>/dev/null
if [[ $? -eq 127 ]] ; then
	echo "expack was not found; please add it to your PATH environment variable"
	echo "and run this script again."
	exit 127
fi

for f in faces gumps shapes paperdol sprites fonts mainshp
do
	if [[ -e "graphics/$f.in" ]] ; then
		echo "Generating $f..."
		expack -i graphics/$f.in
		rm ../data/${f}_*.h
	fi
done

if [[ -e "usecode.uc" ]] ; then
	ucc -o usecode $0&>/dev/null
	if [[ $? -eq 127 ]] ; then
		echo "ucc was not found; please add it to your PATH environment variable"
		echo "and run this script again."
		exit 127
	fi
	
	echo "Compiling Usecode..."
	if ucc -o ../data/usecode usecode.uc; then
		echo "Usecode has been successfully compiled!"
	else
		echo "There were error(s) compiling usecode!"
	fi
fi
