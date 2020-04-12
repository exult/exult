#!/bin/bash

# This script generates Makefile.am and Makefile.mingw files for Exult mods that
# follow a certain layout, as long as they (1) are in a subdir of this script
# and (2) have their own *.cfg file. The layouts are somewhat flexible; data to
# go into the 'patch' dir is searched for under 'patch' and 'data' subdirs. Any
# generated files (such as flex files and usecode files) do not follow these
# restrictions; *.in files (other than Makefile.in) are used with expack to make
# flexes, usecode.uc file is used with ucc to make final usecode file, and they
# will be installed wherever they are generated.
#
# Warning 1: This should be run only from ./content and only after a 'make clean'
# has been executed.
#
# Warning 2: This is a bash script and is likely NON-PORTABLE due to many of the
# commands used. For this reason, the generated makefiles have been included in
# Exult SVN.

# Find all cfg files.
find . -mindepth 2 -iname "*.cfg" | while read -r cfgfile; do
	# Strip initial ./ from cfg name.
	cfgfile="${cfgfile#./}"
	# Get the dir we will process now.
	moddir=$(dirname $cfgfile)
	# Destination dir for mod data (equal to cfg file name).
	installdir=$(basename $cfgfile .cfg)
	# Check for 'patch' or 'data' dirs. Only these dirs are known for now.
	patchdir=""
	if [[ -d "$moddir/patch" ]]; then
		patchdir="patch/"
	elif [[ -d "$moddir/data" ]]; then
		patchdir="data/"
	fi
	
	# Find usecode.uc location and check if we need to set ucc includes.
	sourcedir="$(find $moddir -name usecode.uc)"
	sourcedir=$(dirname "${sourcedir#*$moddir/}")
	include=""
	if [[ -n "$sourcedir" ]]; then
		include="-I $sourcedir"
		sourcedir="$sourcedir/"
	fi

	# Determine the base game mod. We assume forgeofvirtue except for the two
	# we know are for silverseed.
	case "$moddir" in
		"si") export basedest="silverseed";;
		"sifixes") export basedest="silverseed";;
		*) export basedest="forgeofvirtue";;
	esac
	
	# Variables used to build the makefiles.
	datafiles_am=""
	datafiles_mingw=""
	buildrules=""
	buildexpack="no"
	builducc="no"
	targets_mingw="all:"
	cleanfiles=""
	
	# Remove existing makefiles.
	modmakefile_am="$moddir/Makefile.am"
	modmakefile_mingw="$moddir/Makefile.mingw"
	rm -f $modmakefile_am $modmakefile_mingw
	
	# Boilerplate for Makefile.am:
	echo "# This is an automatically generated file; please do not edit it manually.
# Instead, run makefile_builder.sh from the parent directory.

# Base of the exult source
SRC=../..

UCCDIR=\$(SRC)/usecode/compiler
UCC=\$(UCCDIR)/ucc

EXPACKDIR=\$(SRC)/tools
EXPACK=\$(EXPACKDIR)/expack
" >> $modmakefile_am

	# Boilerplate for Makefile.mingw:
	echo "# This is an automatically generated file; please do not edit it manually.
# Instead, run makefile_builder.sh from the parent directory.
# It may require a little tweaking. (paths)

# Where is Ultima 7 installed
U7PATH=C:/Ultima7

# Base of the exult source
SRC=../..

UCCDIR=\$(SRC)
UCC=\$(UCCDIR)/ucc.exe

EXPACKDIR=\$(SRC)
EXPACK=\$(EXPACKDIR)/expack.exe

${moddir}dir=\$(U7PATH)/$basedest/mods
" >> $modmakefile_mingw
	
	# Store MinGW dest dir:
	destdir_mingw="\$(${moddir}dir)/$installdir"

	# Get usecode dependencies.
	sources=$(find $moddir -iname "*.uc" | while read -r f; do echo "	${f#$moddir/}	\\"; done | sort)
	if [[ -n "$sources" ]]; then
		locoutput="USECODE_OBJECTS = \\
${sources%\\*}"
		echo "$locoutput" >> $modmakefile_am
		echo "$locoutput" >> $modmakefile_mingw
		datafiles_am="$datafiles_am	${patchdir}usecode	\\
"
		datafiles_mingw="$datafiles_mingw	cp ${patchdir}usecode $destdir_mingw/${patchdir}usecode
"
		targets_mingw="$targets_mingw ${patchdir}usecode"
		buildrules="$buildrules
${patchdir}usecode: \$(UCC) \$(USECODE_OBJECTS)
	\$(UCC) $include -o ${patchdir}usecode ${sourcedir}usecode.uc
"
		cleanfiles="$cleanfiles
	${patchdir}usecode	\\"
		builducc="yes"
	fi
	
	# Get flex file dependencies from expack scripts. We check *.in scripts for
	# this, except for any Makefile.in files.
	infiles=$(find $moddir -iname "*.in" | grep -v "Makefile.in" | sort)
	for f in $infiles; do
		# Get first line of script for the destination file name.
		read -r flexname < $f
		# Warning: I am taking a shortcut here and assuming that expack will
		# place the destination flex file in the patch dir we found earlier.
		# This is because I don't want to have to figure out which dir the
		# flex is supposed to be built from.
		#fpath="$(readlink -m $(dirname $f)/$flexname)"
		#fpath="${fpath#$(pwd)/$moddir}"
		fpath="$(basename $flexname)"
		fname="${f#*$moddir/}"
		fnamep="${patchdir}$fpath"
		skippedfirst="no"
		# Parse, format and sort dependencies.
		flist=$(cat $f | while read -r g; do if [[ "$skippedfirst" == "no" ]]; then skippedfirst="yes"; else echo "	${sourcedir}graphics/${g#:*:}	\\"; fi; done | sort)
		if [[ -n "$flist" ]]; then
			fnameu=$(echo "${fpath//./_}_OBJECTS" | tr "[:lower:]" "[:upper:]")
			locoutput="
$fnameu = \\
	$fname	\\
${flist%\\*}"
			echo "$locoutput" >> $modmakefile_am
			echo "$locoutput" >> $modmakefile_mingw
			datafiles_am="$datafiles_am	$fnamep	\\
"
			datafiles_mingw="$datafiles_mingw	cp $fnamep $destdir_mingw/$fnamep
"
			targets_mingw="$targets_mingw $fnamep"
			buildrules="$buildrules
$fnamep: \$(EXPACK) \$($fnameu)
	\$(EXPACK) -i \$(srcdir)${f#$moddir}
"
			cleanfiles="$cleanfiles
	$fnamep	\\
	${fnamep//./_}.h	\\"
			buildexpack="yes"
		fi
	done

	# Automake dest dir:
	destdir_am="\$(datadir)/exult/$basedest/mods"
	echo "
${moddir}dir = $destdir_am

${moddir}_DATA = \\
	$(basename $cfgfile)
" >> $modmakefile_am
	
	destdir_am="\$(${moddir}dir)/$installdir"

	# MinGW install mkdir for generated files. We set a flag to prevent the
	# output of a second mkdir for this same directory later on.
	didpatchroot="no"
	if [[ -n "$datafiles_mingw" ]]; then
		datafiles_mingw="	mkdir -p $destdir_mingw/${patchdir%/}
$datafiles_mingw"
		didpatchroot="yes"
	fi

	# Generate install locations of all patch data files.
	if [[ -n "$patchdir" ]]; then
		# We have a known patch dir.
		# Search this dir, all mapXX subdirs and music subdir, if present.
		for dirname in "$moddir/${patchdir%/}" $(find $moddir/$patchdir -type d -regex "$moddir/${patchdir}map[0-9A-Fa-f][0-9A-Fa-f]") $(find $moddir/$patchdir -type d -name "$moddir/${patchdir}music"); do
			# Compute automake rule name.
			dirrule="${dirname#$moddir/}"
			dirrule="${dirrule//[^a-zA-Z0-9]/}"
			# Gather files. Maybe replace with a more targetted list?
			dirfiles=$(find $dirname -maxdepth 1 -type f \( \! -iname "*.h" -a \! -iname "*~" -a \! -iname "usecode" -a \! -iname "*.flx" -a \! -iname "*.vga" \) -o -iname "combos.flx" -o -iname "minimaps.vga" | sort)
			if [[ -n "$dirfiles" || -n "$datafiles_am" ]]; then
				echo "${moddir}${dirrule}dir = $destdir_am/${dirname#$moddir/}
" >> $modmakefile_am
				if [[ "$didpatchroot" == "no" || "$dirname" != "$moddir/${patchdir%/}" ]]; then
					datafiles_mingw="$datafiles_mingw
	mkdir -p $destdir_mingw/${dirname#$moddir/}
"
				fi
				echo "${moddir}${dirrule}_DATA = \\" >> $modmakefile_am
				# Format and sort file list for automake makefile.
				infiles_am=$(echo "$dirfiles" | while read -r f; do echo "	${f#$moddir/}	\\"; done | sort)
				datafiles_am="$datafiles_am$infiles_am"
				datafiles_am="${datafiles_am%\\*}"
				echo "$datafiles_am
" >> $modmakefile_am
				# Format and sort file list for MinGW makefile.
				infiles_mingw=$(echo "$dirfiles" | while read -r f; do dest="${f#$moddir/}"; echo "	cp $dest $destdir_mingw/$dest"; done | sort)
				datafiles_mingw="$datafiles_mingw$infiles_mingw"
			fi
			datafiles_am=""
		done
	elif [[ -n "$datafiles_am" ]]; then
		# We do not have a known patch dir.
		echo "${moddir}patchdir = $destdir_am/patch
" >> $modmakefile_am
		echo "${moddir}patch_DATA = \\
${datafiles_am%\\*}
" >> $modmakefile_am
		datafiles_am=""
	fi

	# Print the list of files to delete on 'make clean'.
	if [[ -n "$cleanfiles" ]]; then
		locoutput="CLEANFILES = \\${cleanfiles%\\*}
"
		echo "$locoutput" >> $modmakefile_am
		echo "
$locoutput" >> $modmakefile_mingw
	fi
	
	# Output rule to build expack, if needed.
	if [[ "x$buildexpack" == "xyes" ]]; then
		echo "\$(EXPACK):
	+(cd \$(EXPACKDIR);\$(MAKE) expack)
" >> $modmakefile_am
		echo "\$(EXPACK):
	+(cd \$(EXPACKDIR);\$(MAKE) -f Makefile.mingw expack.exe)
" >> $modmakefile_mingw
	fi
	
	# Output rule to build ucc, if needed.
	if [[ "x$builducc" == "xyes" ]]; then
		echo "\$(UCC):
	+(cd \$(UCCDIR);\$(MAKE))
" >> $modmakefile_am
		echo "\$(UCC):
	+(cd \$(UCCDIR);\$(MAKE) -f Makefile.mingw ucc.exe)
" >> $modmakefile_mingw
	fi
	
	# Build rules for data files.
	echo "$buildrules" >> $modmakefile_am
	echo "$buildrules" >> $modmakefile_mingw
	
	# Rule for MinGW 'make clean'.
	if [[ -n "$cleanfiles" ]]; then
		echo "clean:
	rm -f \$(CLEANFILES)
" >> $modmakefile_mingw
	fi

	# Rules for MinGW 'make install' and 'make uninstall'.
	echo "$targets_mingw

install: all
	mkdir \$(${moddir}dir)
	cp $(basename $cfgfile) \$(${moddir}dir)/$(basename $cfgfile)
$datafiles_mingw

uninstall:
	rm -f \$(${moddir}dir)/$(basename $cfgfile)
	rm -rf $destdir_mingw
" >> $modmakefile_mingw
done

