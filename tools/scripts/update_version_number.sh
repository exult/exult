#!/bin/bash

# run from the root of our source to update all version numbers and possible git/snapshot identifiers

# Check if version number argument is provided
if [ -z "$1" ]; then
	echo "Error: No version number provided."
	echo "Usage: ./update_version_number.sh VERSION"
	echo "Example: ./update_version_number.sh 1.12"
	exit 1
fi

NEW_VERSION="$1"
echo "Updating version to $NEW_VERSION"

# Check if version contains "git"
git=false
if [[ "$NEW_VERSION" == *"git"* ]]; then
	git=true
	echo "Git version detected"
fi

# Extract the base version without "git" for file version numbers
BASE_VERSION=$(echo "$NEW_VERSION" | sed 's/git//')

# Split version into components for FILE_VERSION
# Handle cases with two or three components (e.g., 1.12 or 1.12.3)
if [[ "$BASE_VERSION" =~ ^([0-9]+)\.([0-9]+)$ ]]; then
	# Two components like 1.12
	FILE_VERSION="${BASH_REMATCH[1]},${BASH_REMATCH[2]},0,0"
	SHORT_VERSION="${BASH_REMATCH[1]},${BASH_REMATCH[2]},0"
elif [[ "$BASE_VERSION" =~ ^([0-9]+)\.([0-9]+)\.([0-9]+)$ ]]; then
	# Three components like 1.12.3
	FILE_VERSION="${BASH_REMATCH[1]},${BASH_REMATCH[2]},${BASH_REMATCH[3]},0"
	SHORT_VERSION="${BASH_REMATCH[1]},${BASH_REMATCH[2]},${BASH_REMATCH[3]}"
else
	echo "Error: Invalid version format. Expected formats: X.Y or X.Y.Z"
	exit 1
fi

echo "Base version: $BASE_VERSION"
echo "File version: $FILE_VERSION"

# For macOS, use LC_ALL=C to handle UTF-8 content
export LC_ALL=C

# Update configure.ac
if [ -f configure.ac ]; then
	echo "Updating configure.ac..."
	perl -i.bak -pe "s/AC_INIT\(\[Exult\],\[[0-9.]*[a-z]*\],/AC_INIT([Exult],[$NEW_VERSION],/" configure.ac
fi

# Update makefile.common
if [ -f makefile.common ]; then
	echo "Updating makefile.common..."
	perl -i.bak -pe "s/VERSION:=[0-9.]*[a-z]*/VERSION:=$NEW_VERSION/" makefile.common
fi

# Update Windows resource files
update_rc_file() {
	local file=$1
	if [ -f "$file" ]; then
		echo "Updating $file..."
        
		# Create a temporary file
		temp_file=$(mktemp)
        
		# Process the file using perl which handles UTF-8 better
		perl -pe "s/FILEVERSION [0-9,]*/FILEVERSION $FILE_VERSION/g" "$file" > "$temp_file"
		mv "$temp_file" "$file"
        
		temp_file=$(mktemp)
		perl -pe "s/PRODUCTVERSION [0-9,]*/PRODUCTVERSION $FILE_VERSION/g" "$file" > "$temp_file"
		mv "$temp_file" "$file"
        
		temp_file=$(mktemp)
		perl -pe "s/VALUE \"FileVersion\", \"[0-9., ]*[a-z]*\\\\0\"/VALUE \"FileVersion\", \"$NEW_VERSION\\\\0\"/" "$file" > "$temp_file"
		mv "$temp_file" "$file"
        
		temp_file=$(mktemp)
		perl -pe "s/VALUE \"ProductVersion\", \"[0-9.]*[a-z]*\\\\0\"/VALUE \"ProductVersion\", \"$NEW_VERSION\\\\0\"/" "$file" > "$temp_file"
		mv "$temp_file" "$file"
	fi
}

for rc_file in win32/exconfig.rc win32/exultico.rc win32/exultstudioico.rc; do
	update_rc_file "$rc_file"
done

# Update installer files - use perl for better UTF-8 handling
update_iss_file() {
	local file=$1
	if [ -f "$file" ]; then
		echo "Updating $file..."
        
		# Update VersionInfoVersion
		temp_file=$(mktemp)
		perl -pe "s/VersionInfoVersion=[0-9.]*/VersionInfoVersion=$BASE_VERSION/" "$file" > "$temp_file"
		mv "$temp_file" "$file"
        
		# Handle AppVerName based on file type
		if [ "$file" = "win32/exult_installer.iss" ]; then
			temp_file=$(mktemp)
			if [ "$git" = true ]; then
				# For git versions, add Snapshot
				perl -pe "s/AppVerName=Exult.*$/AppVerName=Exult $NEW_VERSION Snapshot/" "$file" > "$temp_file"
			else
				# For non-git versions, no Snapshot text
				perl -pe "s/AppVerName=Exult.*$/AppVerName=Exult $NEW_VERSION/" "$file" > "$temp_file"
			fi
			mv "$temp_file" "$file"
		elif [ "$file" = "win32/exult_studio_installer.iss" ]; then
			# Special case for studio installer
			temp_file=$(mktemp)
			if [ "$git" = true ]; then
				perl -pe "s/AppVerName=.*$/AppVerName=Exult Studio Git/" "$file" > "$temp_file"
			else
				perl -pe "s/AppVerName=.*$/AppVerName=Exult Studio $NEW_VERSION/" "$file" > "$temp_file"
			fi
			mv "$temp_file" "$file"
		elif [ "$file" = "win32/exult_tools_installer.iss" ]; then
			# Special case for tools installer
			temp_file=$(mktemp)
			if [ "$git" = true ]; then
				perl -pe "s/AppVerName=.*$/AppVerName=Exult Tools Git/" "$file" > "$temp_file"
			else
				perl -pe "s/AppVerName=.*$/AppVerName=Exult Tools $NEW_VERSION/" "$file" > "$temp_file"
			fi
			mv "$temp_file" "$file"
		elif [ "$file" = "win32/exult_shpplugin_installer.iss" ]; then
			# Special case for shape plugin installer
			temp_file=$(mktemp)
			if [ "$git" = true ]; then
				perl -pe "s/AppVerName=.*$/AppVerName=Shape plug-in Git/" "$file" > "$temp_file"
			else
				perl -pe "s/AppVerName=.*$/AppVerName=Shape plug-in $NEW_VERSION/" "$file" > "$temp_file"
			fi
			mv "$temp_file" "$file"
		fi
	fi
}

for iss_file in win32/exult_installer.iss win32/exult_studio_installer.iss win32/exult_tools_installer.iss win32/exult_shpplugin_installer.iss; do
	update_iss_file "$iss_file"
done

# Update MSVC include file
if [ -f msvcstuff/vs2019/msvc_include.h ]; then
	echo "Updating msvcstuff/vs2019/msvc_include.h..."
	temp_file=$(mktemp)
	perl -pe "s/#define VERSION[ ]*\"[0-9.]*[a-z]*\"/#define VERSION       \"$NEW_VERSION\"/" msvcstuff/vs2019/msvc_include.h > "$temp_file"
	mv "$temp_file" msvcstuff/vs2019/msvc_include.h
fi

# Update iOS config.h
if [ -f ios/include/config.h ]; then
	echo "Updating ios/include/config.h..."
	temp_file=$(mktemp)
	perl -pe "s/#define VERSION \"[0-9.]*[a-z]*\"/#define VERSION \"$NEW_VERSION\"/" ios/include/config.h > "$temp_file"
	mv "$temp_file" ios/include/config.h
    
	temp_file=$(mktemp)
	perl -pe "s/#define PACKAGE_VERSION \"[0-9.]*[a-z]*\"/#define PACKAGE_VERSION \"$NEW_VERSION\"/" ios/include/config.h > "$temp_file"
	mv "$temp_file" ios/include/config.h
    
	temp_file=$(mktemp)
	perl -pe "s/#define PACKAGE_STRING \"Exult [0-9.]*[a-z]*\"/#define PACKAGE_STRING \"Exult $NEW_VERSION\"/" ios/include/config.h > "$temp_file"
	mv "$temp_file" ios/include/config.h
fi

# Update iOS info.plist
if [ -f ios/info.plist ]; then
	echo "Updating ios/info.plist..."
	temp_file=$(mktemp)
	perl -0777 -i.bak -pe "s/<key>CFBundleShortVersionString<\/key>\s*<string>[0-9.]*[a-z]*<\/string>/<key>CFBundleShortVersionString<\/key>\n\t<string>$BASE_VERSION<\/string>/" ios/info.plist
fi

# Update Aseprite plugin version
if [ -f tools/aseprite_plugin/package.json ]; then
	echo "Updating tools/aseprite_plugin/package.json..."
	temp_file=$(mktemp)
	perl -pe "s/\"version\": \"[0-9.]*[a-z]*\",/\"version\": \"$NEW_VERSION\",/" tools/aseprite_plugin/package.json > "$temp_file"
	mv "$temp_file" tools/aseprite_plugin/package.json
fi

# Update macosx/macosx.am
if [ -f macosx/macosx.am ]; then
	echo "Updating macosx/macosx.am..."
    
	# Use a temporary file for each operation
	temp_file=$(mktemp)
    
	# First update: Exult volume name (main Exult DMG)
	# This pattern will match --volname "Exult..." but NOT --volname "Exult Studio..."
	if [ "$git" = true ]; then
		# For git versions, use "Exult Git Snapshot"
		perl -pe 's/--volname "Exult(?!\s+Studio)[^"]*"/--volname "Exult Git Snapshot"/g' macosx/macosx.am > "$temp_file"
	else
		# For release versions, use "Exult VERSION"
		perl -pe 's/--volname "Exult(?!\s+Studio)[^"]*"/--volname "Exult '"$NEW_VERSION"'"/g' macosx/macosx.am > "$temp_file"
	fi
	mv "$temp_file" macosx/macosx.am
    
	# Second update: Exult Studio volume name
	temp_file=$(mktemp)
	if [ "$git" = true ]; then
		# For git versions, use "Exult Studio Git Snapshot"
		perl -pe 's/--volname "Exult Studio[^"]*"/--volname "Exult Studio Git Snapshot"/g' macosx/macosx.am > "$temp_file"
	else
		# For release versions, use "Exult Studio VERSION"
		perl -pe 's/--volname "Exult Studio[^"]*"/--volname "Exult Studio '"$NEW_VERSION"'"/g' macosx/macosx.am > "$temp_file"
	fi
	mv "$temp_file" macosx/macosx.am

	# Third update: DMG filenames for Exult and Exult Studio
	temp_file=$(mktemp)
	if [ "$git" = true ]; then
		# For git versions, use snapshot filenames
		perl -pe 's/"Exult-[^\"]*\.dmg"/"Exult-snapshot.dmg"/g' macosx/macosx.am > "$temp_file"
		perl -pe 's/"ExultStudio-[^\"]*\.dmg"/"ExultStudio-snapshot.dmg"/g' macosx/macosx.am > "$temp_file"	
		mv "$temp_file" macosx/macosx.am
	else
 		# For release versions, use versioned filenames
		temp_file=$(mktemp)
		perl -pe "s/\"Exult-[^\"]*\\.dmg\"/\"Exult-$NEW_VERSION.dmg\"/g" macosx/macosx.am > "$temp_file"
		perl -pe "s/\"ExultStudio-[^\"]*\\.dmg\"/\"ExultStudio-$NEW_VERSION.dmg\"/g" macosx/macosx.am > "$temp_file"
		mv "$temp_file" macosx/macosx.am
	fi
fi

find . -name "*.bak" -type f -delete

echo "Version updated to $NEW_VERSION successfully!"