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

n=$'\n'
t=$'\t'

# Find all cfg files.
find . -mindepth 2 -iname "*.cfg" | while read -r cfgfile; do
	# Strip initial ./ from cfg name.
	cfgfile="${cfgfile#./}"
	# Get the dir we will process now.
	moddir=$(dirname "$cfgfile")
	# Destination dir for mod data (equal to cfg file name).
	installdir=$(basename "$cfgfile" .cfg)
	# Check for 'patch' or 'data' dirs. Only these dirs are known for now.
	patchdir=""
	if [[ -d "$moddir/patch" ]]; then
		patchdir="patch/"
	elif [[ -d "$moddir/data" ]]; then
		patchdir="data/"
	fi

	# Find usecode.uc location and check if we need to set ucc includes.
	sourcedir="$(find "$moddir" -name usecode.uc)"
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
	nodist_datafiles_am=""
	datafiles_am=""
	datafiles_mingw=""
	extradist_am=""
	buildrules=""
	buildexpack="no"
	builducc="no"
	targets_mingw="all:"
	cleanfiles=""

	# Remove existing makefiles.
	modmakefile_am="$moddir/Makefile.am"
	modmakefile_mingw="$moddir/Makefile.mingw"
	rm -f "$modmakefile_am" "$modmakefile_mingw"

	# Boilerplate for Makefile.am:
	echo "# This is an automatically generated file; please do not edit it manually.
# Instead, run makefile_builder.sh from the parent directory.

# Base of the exult source
UCCDIR:=\$(top_srcdir)/tools/compiler
UCC:=\$(UCCDIR)/ucc

EXPACKDIR:=\$(top_srcdir)/tools
EXPACK:=\$(EXPACKDIR)/expack
" >> "$modmakefile_am"

	# Boilerplate for Makefile.mingw:
	echo "# This is an automatically generated file; please do not edit it manually.
# Instead, run makefile_builder.sh from the parent directory.
# It may require a little tweaking. (paths)

# Where is Ultima 7 installed
U7PATH:=C:/Ultima7

# Base of the exult source
SRC:=../..

srcdir:=.

UCCDIR:=\$(SRC)
UCC:=\$(UCCDIR)/ucc.exe

EXPACKDIR:=\$(SRC)
EXPACK:=\$(EXPACKDIR)/expack.exe

${moddir}dir:=\$(U7PATH)/$basedest/mods
" >> "$modmakefile_mingw"

	# Store MinGW dest dir:
	destdir_mingw="\$(${moddir}dir)/$installdir"

	# MinGW EXTRADIST
	distfiles_mingw=$(find "$moddir" -maxdepth 1 -iname '*.ico' -or -iname '*.png' -or -iname '*.txt' | while read -r f; do file="${f#$moddir/}" ; echo "${t}cp ${file} \$(${moddir}dir)/${installdir}/${file}"; done | sort)
	if [[ -n "$distfiles_mingw" ]]; then
		distfiles_mingw="${distfiles_mingw}${n}${t}cp ./../../COPYING \$(${moddir}dir)/${installdir}/License.txt"
	elif [[ -f "$moddir/README" ]]; then
		distfiles_mingw="${t}cp README \$(${moddir}dir)/${installdir}/Readme.txt${n}${t}cp ./../../COPYING \$(${moddir}dir)/${installdir}/License.txt"
	else
		distfiles_mingw="${t}cp ./../../COPYING \$(${moddir}dir)/${installdir}/License.txt"
	fi

	# Automake EXTRADIST
	distfiles=$(find "$moddir" -maxdepth 1 -iname '*.ico' -or -iname '*.png' -or -iname '*.mingw' -or -iname '*.txt' | sort | sed -r "s%^(.*)$moddir/(.*)\s*\$%${t}\2% ; \$!s%^(.*)\s*\$%\1${t}\\\\%")
	if [[ -n "$distfiles" ]]; then
		extradist_am="$extradist_am${t}\\${n}${distfiles}"
	fi
	distfiles=$(find "$moddir" -iname 'make.*' | sort | sed -r "s%^(.*)$moddir/(.*)\s*\$%${t}\2% ; \$!s%^(.*)\s*\$%\1${t}\\\\%")
	if [[ -n "$distfiles" ]]; then
		extradist_am="$extradist_am${t}\\${n}${distfiles}"
	fi

	# Get usecode dependencies.
	sources=$(find "$moddir" -iname "*.uc" | sort | sed -r "s%^(.*)$moddir/(.*)\s*\$%${t}\2% ; \$!s%^(.*)\s*\$%\1${t}\\\\%")
	if [[ -n "$sources" ]]; then
		extradist_am="$extradist_am${t}\\${n}${t}\$(USECODE_OBJECTS)"
		locoutput="USECODE_OBJECTS :=${t}\\${n}${sources}"
		echo -e "$locoutput" >> "$modmakefile_am"
		echo -e "$locoutput" >> "$modmakefile_mingw"
		nodist_datafiles_am="$nodist_datafiles_am\\${n}${t}${patchdir}usecode${t}"
		datafiles_mingw="$datafiles_mingw${t}cp ${patchdir}usecode $destdir_mingw/${patchdir}usecode${n}"
		targets_mingw="$targets_mingw ${patchdir}usecode"
		buildrules="$buildrules${n}${patchdir}usecode: \$(UCC) \$(USECODE_OBJECTS)${n}${t}\$(UCC) $include -o ${patchdir}usecode ${sourcedir}usecode.uc${n}"
		cleanfiles="$cleanfiles${n}${t}${patchdir}usecode${t}\\"
		builducc="yes"
	fi

	# Get flex file dependencies from expack scripts. We check *.in scripts for
	# this, except for any Makefile.in files.
	infiles=$(find "$moddir" -iname "*.in" | grep -v "Makefile.in" | sort)
	for f in $infiles; do
		# Get first line of script for the destination file name.
		read -r flexname < "$f"
		# Warning: I am taking a shortcut here and assuming that expack will
		# place the destination flex file in the patch dir we found earlier.
		# This is because I don't want to have to figure out which dir the
		# flex is supposed to be built from.
		#fpath="$(readlink -m $(dirname $f)/$flexname)"
		#fpath="${fpath#$(pwd)/$moddir}"
		fpath="$(basename "$flexname")"
		fname="${f#*$moddir/}"
		fnamep="${patchdir}$fpath"
		skippedfirst="no"
		# Parse, format and sort dependencies.
		flist=$(sed '/^$/d' "$f" | while read -r g; do if [[ "$skippedfirst" == "no" ]]; then skippedfirst="yes"; else echo "	${sourcedir}graphics/${g#:*:}	\\"; fi; done | sort)
		if [[ -n "$flist" ]]; then
			fnameu=$(echo "${fpath//./_}_OBJECTS" | tr "[:lower:]" "[:upper:]")
			extradist_am="$extradist_am${t}\\${n}${t}\$($fnameu)"
			locoutput="${n}$fnameu :=${t}\\${n}${t}$fname${t}\\${n}${flist%${t}*}"
			echo -e "$locoutput" >> "$modmakefile_am"
			echo -e "$locoutput" >> "$modmakefile_mingw"
			nodist_datafiles_am="$nodist_datafiles_am\\${n}${t}$fnamep${t}"
			datafiles_mingw="$datafiles_mingw${t}cp $fnamep $destdir_mingw/$fnamep${n}"
			targets_mingw="$targets_mingw $fnamep"
			buildrules="$buildrules${n}$fnamep: \$(EXPACK) \$($fnameu)${n}${t}\$(EXPACK) -i \$(srcdir)${f#$moddir}${n}"
			cleanfiles="$cleanfiles${n}${t}$fnamep${t}\\${n}${t}${fnamep//./_}.h${t}\\"
			buildexpack="yes"
		fi
	done

	# Automake dest dir:
	destdir_am="\$(datadir)/exult/$basedest/mods"
	extradist_am="$extradist_am${t}\\${n}${t}\$(${moddir}_DATA)"
	echo -e "${n}${moddir}dir := $destdir_am${n}${n}${moddir}_DATA :=${t}\\${n}${t}$(basename "$cfgfile")${n}" >> "$modmakefile_am"

	destdir_am="\$(${moddir}dir)/$installdir"

	# MinGW install mkdir for generated files. We set a flag to prevent the
	# output of a second mkdir for this same directory later on.
	didpatchroot="no"
	if [[ -n "$datafiles_mingw" ]]; then
		datafiles_mingw="${t}mkdir -p $destdir_mingw/${patchdir%/}
$datafiles_mingw"
		didpatchroot="yes"
	fi

	# Generate install locations of all patch data files.
	if [[ -n "$patchdir" ]]; then
		# We have a known patch dir.
		# Search this dir, all mapXX subdirs and music subdir, if present.
		for dirname in "$moddir/${patchdir%/}" $(find "$moddir/$patchdir" -type d -regex "$moddir/${patchdir}map[0-9A-Fa-f][0-9A-Fa-f]") $(find "$moddir/$patchdir" -type d -name "$moddir/${patchdir}music"); do
			# Compute automake rule name.
			dirrule="${dirname#$moddir/}"
			dirrule="${dirrule//[^a-zA-Z0-9]/}"
			# Gather files. Maybe replace with a more targetted list?
			dirfiles=$(find "$dirname" -maxdepth 1 -type f \( \! -iname "*.h" -a \! -iname "*~" -a \! -iname "usecode" -a \! -iname "*.flx" -a \! -iname "*.vga" \) -o -iname "combos.flx" -o -iname "minimaps.vga" | sort)
			if [[ -n "$dirfiles" || -n "$datafiles_am" || -n "$nodist_datafiles_am" ]]; then
				echo -e "${moddir}${dirrule}dir =${t}$destdir_am/${dirname#$moddir/}${n}" >> "$modmakefile_am"
				if [[ -n "$nodist_datafiles_am" ]]; then
					echo -e "nodist_${moddir}${dirrule}_DATA :=${t}${nodist_datafiles_am%${t}*}${n}" >> "$modmakefile_am"
				fi
				if [[ "$didpatchroot" == "no" || "$dirname" != "$moddir/${patchdir%/}" ]]; then
					datafiles_mingw="$datafiles_mingw${n}\tmkdir -p $destdir_mingw/${dirname#$moddir/}${n}"
				fi
				echo -e "${moddir}${dirrule}_DATA :=${t}\\" >> "$modmakefile_am"
				extradist_am="$extradist_am${t}\\${n}${t}\$(${moddir}${dirrule}_DATA)"
				# Format and sort file list for automake makefile.
				infiles_am=$(echo "$dirfiles" | while read -r f; do echo -e "${t}${f#$moddir/}${t}\\"; done | sort)
				datafiles_am="$datafiles_am$infiles_am"
				datafiles_am="${datafiles_am%${t}*}"
				if [[ -n "$datafiles_am" ]]; then
					echo -e "$datafiles_am${n}" >> "$modmakefile_am"
				fi
				# Format and sort file list for MinGW makefile.
				infiles_mingw=$(echo "$dirfiles" | while read -r f; do dest="${f#$moddir/}"; echo -e "${t}cp $dest $destdir_mingw/$dest"; done | sort)
				datafiles_mingw="$datafiles_mingw$infiles_mingw"
			fi
			datafiles_am=""
			nodist_datafiles_am=""
		done
	else
		# We do not have a known patch dir.
		if [[ -n "$nodist_datafiles_am" || -n "$datafiles_am" ]]; then
			echo -e "${moddir}patchdir := $destdir_am/patch${n}" >> "$modmakefile_am"
		fi
		if [[ -n "$nodist_datafiles_am" ]]; then
			# We do not have a known patch dir.
			echo -e "nodist_${moddir}patch_DATA :=${t}${nodist_datafiles_am%${t}*}${n}" >> "$modmakefile_am"
			nodist_datafiles_am=""
		fi

		if [[ -n "$datafiles_am" ]]; then
			echo -e "${moddir}patch_DATA :=${t}\\${n}${datafiles_am%${t}*}${n}" >> "$modmakefile_am"
			datafiles_am=""
		fi
	fi

	# Print the EXTRADIST diles
	if [[ -n "$extradist_am" ]]; then
		echo -e "EXTRA_DIST :=$extradist_am${n}" >> "$modmakefile_am"
	fi

	# Print the list of files to delete on 'make clean'.
	if [[ -n "$cleanfiles" ]]; then
		locoutput="CLEANFILES :=${t}\\${cleanfiles%${t}*}${n}"
		echo -e "$locoutput" >> "$modmakefile_am"
		echo -e "${n}$locoutput" >> "$modmakefile_mingw"
	fi

	# Rules for MinGW 'make install' and 'make uninstall'.
	if [[ -n "$datafiles_mingw" ]]; then
		if [[ -n "$distfiles_mingw" ]]; then
			datafiles_mingw="${datafiles_mingw%$'\n'}${n}$distfiles_mingw"
		else
			datafiles_mingw="$datafiles_mingw"
		fi
	elif [[ -n "$distfiles_mingw" ]]; then
		datafiles_mingw="$distfiles_mingw"
	else
		datafiles_mingw=""
	fi
	echo -e "$targets_mingw${n}\ninstall: all${n}\tmkdir -p \$(${moddir}dir)${n}\tcp $(basename "$cfgfile") \$(${moddir}dir)/$(basename "$cfgfile")${n}$datafiles_mingw${n}\nuninstall:${n}\trm -f \$(${moddir}dir)/$(basename "$cfgfile")${n}\trm -rf $destdir_mingw${n}" >> "$modmakefile_mingw"

	# Output rule to build expack, if needed.
	if [[ "$buildexpack" == "yes" ]]; then
		echo -e "\$(EXPACK):${n}${t}+(cd \$(EXPACKDIR);\$(MAKE) expack)${n}" >> "$modmakefile_am"
		echo -e "\$(EXPACK):${n}${t}+(cd \$(EXPACKDIR);\$(MAKE) -f Makefile.mingw expack.exe)${n}" >> "$modmakefile_mingw"
	fi

	# Output rule to build ucc, if needed.
	if [[ "$builducc" == "yes" ]]; then
		echo -e "\$(UCC):${n}${t}+(cd \$(UCCDIR);\$(MAKE))${n}" >> "$modmakefile_am"
		echo -e "\$(UCC):${n}${t}+(cd \$(UCCDIR);\$(MAKE) -f Makefile.mingw ucc.exe)${n}" >> "$modmakefile_mingw"
	fi

	# Build rules for data files.
	echo -e "$buildrules" >> "$modmakefile_am"
	echo -e "$buildrules" >> "$modmakefile_mingw"

	# Rule for MinGW 'make clean'.
	if [[ -n "$cleanfiles" ]]; then
		echo -e "clean:${n}\trm -f \$(CLEANFILES)${n}" >> "$modmakefile_mingw"
	fi
done

