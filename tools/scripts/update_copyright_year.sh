#!/bin/bash

# run from the root of our source to update all front facing copyright years

# Get the current year
CURRENT_YEAR=$(date +%Y)

# Update copyright year in keyactions.cc
if [ -f keyactions.cc ]; then
	echo "Updating keyactions.cc..."
	sed -i.bak "s/1998-[0-9]\{4\}/1998-${CURRENT_YEAR}/g" keyactions.cc
fi

# Update copyright year in info.plist.in
if [ -f info.plist.in ]; then
	echo "Updating info.plist.in..."
	sed -i.bak "s/1998-[0-9]\{4\}/1998-${CURRENT_YEAR}/g" info.plist.in
fi

# Update copyright year in macosx/exult_studio_info.plist.in
if [ -f macosx/exult_studio_info.plist.in ]; then
	echo "Updating macosx/exult_studio_info.plist.in..."
	sed -i.bak "s/1998-[0-9]\{4\}/1998-${CURRENT_YEAR}/g" macosx/exult_studio_info.plist.in
fi

# Update copyright year in win32/exultico.rc
if [ -f win32/exultico.rc ]; then
	echo "Updating win32/exultico.rc..."
	sed -i.bak "s/1998-[0-9]\{4\}/1998-${CURRENT_YEAR}/g" win32/exultico.rc
fi

# Update copyright year in win32/exultstudioico.rc
if [ -f win32/exultstudioico.rc ]; then
	echo "Updating win32/exultstudioico.rc..."
	sed -i.bak "s/1998-[0-9]\{4\}/1998-${CURRENT_YEAR}/g" win32/exultstudioico.rc
fi

# Remove backup files
find . -name "*.bak" -type f -delete

echo "Copyright years updated to ${CURRENT_YEAR}"