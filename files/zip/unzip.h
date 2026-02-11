/* unzip.h -- IO for uncompress .zip files using zlib
   Version 0.15 beta, Mar 19th, 1998,

   Copyright (C) 1998 Gilles Vollant

   Modified by Ryan Nunn. Nov 9th 2001
   Modified by the Exult Team. Sep 20th 2003-2022

   This unzip package allow extract file from .ZIP file, compatible with
  PKZip 2.04g WinZip, InfoZip tools and compatible. Encryption and multi volume
  ZipFile (span) are not supported. Old compressions used by old PKZip 1.x are
  not supported

   THIS IS AN ALPHA VERSION. AT THIS STAGE OF DEVELOPPEMENT, SOMES API OR
  STRUCTURE CAN CHANGE IN FUTURE VERSION !! I WAIT FEEDBACK at mail
  info@winimage.com Visit also http://www.winimage.com/zLibDll/unzip.html for
  evolution

   Condition of use and distribution are the same than zlib :

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
	 claim that you wrote the original software. If you use this software
	 in a product, an acknowledgment in the product documentation would be
	 appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
	 misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.


*/
/* for more info about .ZIP format, see
	  ftp://ftp.cdrom.com/pub/infozip/doc/appnote-970311-iz.zip
   PkWare has also a specification at :
	  ftp://ftp.pkware.com/probdesc.zip */

#ifndef _unz_H
#define _unz_H

#ifdef HAVE_ZIP_SUPPORT
#	include <string>
#	define ZLIB_CONST
#	include <zlib.h>

struct unz_s;
class unzFile;

class IDataSource;

#	define UNZ_OK                   (0)
#	define UNZ_END_OF_LIST_OF_FILE  (-100)
#	define UNZ_ERRNO                (Z_ERRNO)
#	define UNZ_EOF                  (0)
#	define UNZ_PARAMERROR           (-102)
#	define UNZ_BADZIPFILE           (-103)
#	define UNZ_INTERNALERROR        (-104)
#	define UNZ_CRCERROR             (-105)
#	define UNZ_UNKNOWNFILEERROR     (-106)
#	define UNZ_FILENAMETOOLONGERROR (-107)

/* tm_unz contain date/time info */
struct tm_unz {
	uInt tm_sec;  /* seconds after the minute - [0,59] */
	uInt tm_min;  /* minutes after the hour - [0,59] */
	uInt tm_hour; /* hours since midnight - [0,23] */
	uInt tm_mday; /* day of the month - [1,31] */
	uInt tm_mon;  /* months since January - [0,11] */
	uInt tm_year; /* years - [1980..2044] */
};

/* unz_global_info structure contain global data about the ZIPfile
   These data comes from the end of central dir */
struct unz_global_info {
	uLong number_entry; /* total number of entries in
		   the central dir on this disk */
	uLong size_comment; /* size of the global comment of the zipfile */
};

/* unz_file_info contain information about a file in the zipfile */
struct unz_file_info {
	uLong version;            /* version made by                 2 bytes */
	uLong version_needed;     /* version needed to extract       2 bytes */
	uLong flag;               /* general purpose bit flag        2 bytes */
	uLong compression_method; /* compression method              2 bytes */
	uLong dosDate;            /* last mod file date in Dos fmt   4 bytes */
	uLong crc;                /* crc-32                          4 bytes */
	uLong compressed_size;    /* compressed size                 4 bytes */
	uLong uncompressed_size;  /* uncompressed size               4 bytes */
	uLong size_filename;      /* filename length                 2 bytes */
	uLong size_file_extra;    /* extra field length              2 bytes */
	uLong size_file_comment;  /* file comment length             2 bytes */

	uLong disk_num_start; /* disk number start               2 bytes */
	uLong internal_fa;    /* internal file attributes        2 bytes */
	uLong external_fa;    /* external file attributes        4 bytes */

	tm_unz tmu_date;
};

/*
   Compare two filename (fileName1,fileName2).
   If iCaseSenisivity = 1, comparision is case sensitivity (like strcmp)
   If iCaseSenisivity = 2, comparision is not case sensitivity (like strcmpi
								or strcasecmp)
   If iCaseSenisivity = 0, case sensitivity is defaut of your operating system
	(like 1 on Unix, 2 on Windows)
*/
extern int ZEXPORT unzStringFileNameCompare(const char* fileName1, const char* fileName2, int iCaseSensitivity);

extern unzFile ZEXPORT unzOpen(IDataSource* datasource);

/*
  Write info about the ZipFile in the *pglobal_info structure.
  No preparation of the structure is needed
  return UNZ_OK if there is no problem. */
extern int ZEXPORT unzGetGlobalInfo(unz_s* file, unz_global_info* pglobal_info);

/*
  Get the global comment string of the ZipFile, in the szComment buffer.
  uSizeBuf is the size of the szComment buffer.
  return the number of byte copied or an error code <0
*/
extern int ZEXPORT unzGetGlobalComment(unz_s* file, char* szComment, uLong uSizeBuf);

/***************************************************************************/
/* Unzip package allow you browse the directory of the zipfile */
/*
  Set the current file of the zipfile to the first file.
  return UNZ_OK if there is no problem
*/
extern int ZEXPORT unzGoToFirstFile(unz_s* file);

/*
  Set the current file of the zipfile to the next file.
  return UNZ_OK if there is no problem
  return UNZ_END_OF_LIST_OF_FILE if the actual file was the latest.
*/
extern int ZEXPORT unzGoToNextFile(unz_s* file);

/*
  Try locate the file szFileName in the zipfile.
  For the iCaseSensitivity signification, see unzStringFileNameCompare

  return value :
  UNZ_OK if the file is found. It becomes the current file.
  UNZ_END_OF_LIST_OF_FILE if the file is not found
*/
extern int ZEXPORT unzLocateFile(unz_s* file, const char* szFileName, int iCaseSensitivity);

/*
  Get Info about the current file
  if pfile_info!=nullptr, the *pfile_info structure will contain somes info
  about the current file if szFileName!=nullptr, the filemane string will be
  copied in szFileName (fileNameBufferSize is the size of the buffer) if
  extraField!=nullptr, the extra field information will be copied in extraField
			(extraFieldBufferSize is the size of the buffer).
			This is the Central-header version of the extra field
  if szComment!=nullptr, the comment string of the file will be copied in
  szComment (commentBufferSize is the size of the buffer)
*/
extern int ZEXPORT unzGetCurrentFileInfo(
		unz_s* file, unz_file_info* pfile_info, char* szFileName, uLong fileNameBufferSize, void* extraField,
		uLong extraFieldBufferSize, char* szComment, uLong commentBufferSize);
extern int ZEXPORT unzGetCurrentFileInfo(
		unz_s* file, unz_file_info* pfile_info, std::string& fileName, void* extraField, uLong extraFieldBufferSize,
		char* szComment, uLong commentBufferSize);

/***************************************************************************/
/* for reading the content of the current zipfile, you can open it, read data
   from it, and close it (you can close it before reading all the file)
   */

/*
  Open for reading data the current file in the zipfile.
  If there is no error, the return value is UNZ_OK.
*/
extern int ZEXPORT unzOpenCurrentFile(unz_s* file);

/*
  Close the file in zip opened with unzOpenCurrentFile
  Return UNZ_CRCERROR if all the file was read but the CRC is not good
*/
extern int ZEXPORT unzCloseCurrentFile(unz_s* file);

/*
  Read bytes from the current file (opened by unzOpenCurrentFile)
  buf contain buffer where data must be copied
  len the size of buf.

  return the number of byte copied if somes bytes are copied
  return 0 if the end of file was reached
  return <0 with error code if there is an error
	(UNZ_ERRNO for IO error, or zLib error for uncompress error)
*/
extern int ZEXPORT unzReadCurrentFile(unz_s* file, voidp buf, unsigned len);

/*
  Give the current position in uncompressed data
*/
extern z_off_t ZEXPORT unztell(unz_s* file);

/*
  return 1 if the end of file was reached, 0 elsewhere
*/
extern int ZEXPORT unzeof(unz_s* file);

/*
  Read extra field from the current file (opened by unzOpenCurrentFile)
  This is the local-header version of the extra field (sometimes, there is
	more info in the local-header version than in the central-header)

  if buf==nullptr, it return the size of the local extra field

  if buf!=nullptr, len is the size of the buffer, the extra header is copied in
	buf.
  the return value is the number of bytes copied in buf, or (if <0)
	the error code
*/
extern int ZEXPORT unzGetLocalExtrafield(unz_s* file, voidp buf, unsigned len);

/*
 * Extract all the files in a zipfile to the given path
 */
extern int ZEXPORT unzExtractAllToPath(unz_s* file, const char* destpath);

class unzFile {
	unz_s* data;

public:
	unzFile() : data(nullptr) {}

	// No copying allowed
	unzFile(const unzFile&) = delete;

	// Only moving allowed
	unzFile(unzFile&& other) noexcept : data(other.data) {
		other.data = nullptr;
	}

	void set(unz_s* newdata);

	~unzFile();

	// No copying allowed
	unzFile& operator=(const unzFile& other) = delete;

	// Only moving allowed
	unzFile& operator=(unzFile&& other) noexcept {
		if (this != &other) {
			std::swap(data, other.data);
		}
		return *this;
	}

	operator bool() {
		return data;
	}

	operator unz_s*() {
		return data;
	}

	unz_s* operator->() {
		return data;
	}
};

#endif /*HAVE_ZIP_SUPPORT*/

#endif /* _unz_H */
