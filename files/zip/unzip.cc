/* unzip.cc -- IO on .zip files using zlib
   Version 0.15 beta, Mar 19th, 1998,

   Modified by Ryan Nunn. Nov 9th 2001
   Modified by the Exult Team. Nov 10th 2001 - 2022
   Read unzip.h for more info
*/

/* Added by Ryan Nunn */
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifdef HAVE_ZIP_SUPPORT
#	include <cctype>
#	include <cstddef>
#	include <cstdio>
#	include <cstdlib>
#	include <cstring>
#	ifdef NO_ERRNO_H
extern int errno;
#	else
#		include <cerrno>
#	endif
using namespace std;

#	include "unzip.h"

#	include "../databuf.h"
#	include "../utils.h"
#	include "exceptions.h"

#	if !defined(unix) && !defined(CASESENSITIVITYDEFAULT_YES) && !defined(CASESENSITIVITYDEFAULT_NO)
#		define CASESENSITIVITYDEFAULT_NO
#	endif

#	ifndef UNZ_BUFSIZE
#		define UNZ_BUFSIZE (16384)
#	endif

#	ifndef UNZ_MAXFILENAMEINZIP
#		define UNZ_MAXFILENAMEINZIP (256)
#	endif

#	define SIZECENTRALDIRITEM (0x2e)
#	define SIZEZIPLOCALHEADER (0x1e)

const char unz_copyright[] = " unzip 0.15 Copyright 1998 Gilles Vollant ";

#	ifdef __GNUC__
#		pragma GCC diagnostic push
#		pragma GCC diagnostic ignored "-Wold-style-cast"
#	endif    // __GNUC__
static int U7inflateInit2(z_stream* stream) {
	return inflateInit2(stream, -MAX_WBITS);
}
#	ifdef __GNUC__
#		pragma GCC diagnostic pop
#	endif    // __GNUC__

/* unz_file_info_interntal contain internal info about a file in zipfile*/
struct unz_file_info_internal {
	uLong offset_curfile; /* relative offset of local header 4 bytes */
};

/* file_in_zip_read_info_s contain internal information about a file in zipfile,
	when reading and decompress it */
struct file_in_zip_read_info_s {
	char*    read_buffer; /* internal buffer for compressed data */
	z_stream stream;      /* zLib stream structure for inflate */

	uLong pos_in_zipfile;     /* position in byte on the zipfile, for fseek*/
	uLong stream_initialised; /* flag set if stream structure is initialised*/

	uLong offset_local_extrafield; /* offset of the local extra field */
	uInt  size_local_extrafield;   /* size of the local extra field */
	uLong pos_local_extrafield;    /* position in the local extra field in read*/

	uLong        crc32;                   /* crc32 of all data uncompressed */
	uLong        crc32_wait;              /* crc32 we must obtain after decompress all */
	uLong        rest_read_compressed;    /* number of byte to be decompressed */
	uLong        rest_read_uncompressed;  /*number of byte to be obtained after decomp*/
	IDataSource* file;                    /* io structore of the zipfile */
	uLong        compression_method;      /* compression method (0==store) */
	uLong        byte_before_the_zipfile; /* byte before the zipfile, (>0 for sfx)*/
};

/* unz_s contain internal information about the zipfile
 */
struct unz_s {
	IDataSource*    file;                    /* io structore of the zipfile */
	unz_global_info gi;                      /* public global information */
	uLong           byte_before_the_zipfile; /* byte before the zipfile, (>0 for sfx)*/
	uLong           num_file;                /* number of the current file in the zipfile*/
	uLong           pos_in_central_dir;      /* pos of the current file in the central dir*/
	uLong           current_file_ok;         /* flag about the usability of the current file*/
	uLong           central_pos;             /* position of the beginning of the central dir*/

	uLong size_central_dir;   /* size of the central directory  */
	uLong offset_central_dir; /* offset of start of central directory with
								 respect to the starting disk number */

	unz_file_info                            cur_file_info;          /* public info about the current file in zip*/
	unz_file_info_internal                   cur_file_info_internal; /* private info about it*/
	std::unique_ptr<file_in_zip_read_info_s> pfile_in_zip_read;      /* structure about the current
																			  file if we are decompressing it */
};

/* ===========================================================================
	 Read a byte from a gz_stream; update next_in and avail_in. Return EOF
   for end of file.
   IN assertion: the stream s has been sucessfully opened for reading.
*/

static int unzlocal_getByte(IDataSource* fin, int* pi) {
	if (fin->getAvail() < 1) {
		return UNZ_EOF;
	}
	if (!fin->good()) {
		return UNZ_UNKNOWNFILEERROR;
	}

	*pi = fin->read1();
	if (fin->fail()) {
		fin->clear_error();
		return UNZ_UNKNOWNFILEERROR;
	} else {
		return UNZ_OK;
	}
}

static int unzlocal_read(IDataSource* fin, void* ptr, size_t count) {
	if (fin->getAvail() < count) {
		return UNZ_EOF;
	}
	if (!fin->good()) {
		return UNZ_UNKNOWNFILEERROR;
	}

	fin->read(ptr, count);
	if (fin->fail()) {
		fin->clear_error();
		return UNZ_UNKNOWNFILEERROR;
	} else {
		return UNZ_OK;
	}
}

/* ===========================================================================
   Reads a long in LSB order from the given gz_stream. Sets
*/
static int unzlocal_getShort(IDataSource* fin, uLong* pX) {
	uLong x;
	int   i = 0;
	int   err;

	err = unzlocal_getByte(fin, &i);
	x   = static_cast<uLong>(i);

	if (err == UNZ_OK) {
		err = unzlocal_getByte(fin, &i);
	}
	x += static_cast<uLong>(i) << 8;

	if (err == UNZ_OK) {
		*pX = x;
	} else {
		*pX = 0;
	}
	return err;
}

static int unzlocal_getLong(IDataSource* fin, uLong* pX) {
	uLong x;
	int   i = 0;
	int   err;

	err = unzlocal_getByte(fin, &i);
	x   = static_cast<uLong>(i);

	if (err == UNZ_OK) {
		err = unzlocal_getByte(fin, &i);
	}
	x += static_cast<uLong>(i) << 8;

	if (err == UNZ_OK) {
		err = unzlocal_getByte(fin, &i);
	}
	x += static_cast<uLong>(i) << 16;

	if (err == UNZ_OK) {
		err = unzlocal_getByte(fin, &i);
	}
	x += static_cast<uLong>(i) << 24;

	if (err == UNZ_OK) {
		*pX = x;
	} else {
		*pX = 0;
	}
	return err;
}

/* My own strcmpi / strcasecmp */
static int strcmpcasenosensitive_internal(const char* fileName1, const char* fileName2) {
	for (;;) {
		auto c1 = std::toupper(static_cast<unsigned char>(*(fileName1++)));
		auto c2 = std::toupper(static_cast<unsigned char>(*(fileName2++)));
		if (c1 == '\0') {
			return (c2 == '\0') ? 0 : -1;
		}
		if (c2 == '\0') {
			return 1;
		}
		if (c1 < c2) {
			return -1;
		}
		if (c1 > c2) {
			return 1;
		}
	}
}

#	ifdef CASESENSITIVITYDEFAULT_NO
#		define CASESENSITIVITYDEFAULTVALUE 2
#	else
#		define CASESENSITIVITYDEFAULTVALUE 1
#	endif

#	ifndef STRCMPCASENOSENTIVEFUNCTION
#		define STRCMPCASENOSENTIVEFUNCTION strcmpcasenosensitive_internal
#	endif

/*
   Compare two filename (fileName1,fileName2).
   If iCaseSenisivity = 1, comparision is case sensitivity (like strcmp)
   If iCaseSenisivity = 2, comparision is not case sensitivity (like strcmpi
																or strcasecmp)
   If iCaseSenisivity = 0, case sensitivity is defaut of your operating system
		(like 1 on Unix, 2 on Windows)

*/
extern int ZEXPORT unzStringFileNameCompare(const char* fileName1, const char* fileName2, int iCaseSensitivity) {
	if (iCaseSensitivity == 0) {
		iCaseSensitivity = CASESENSITIVITYDEFAULTVALUE;
	}

	if (iCaseSensitivity == 1) {
		return strcmp(fileName1, fileName2);
	}

	return STRCMPCASENOSENTIVEFUNCTION(fileName1, fileName2);
}

#	define BUFREADCOMMENT (0x400)

/*
  Locate the Central directory of a zipfile (at the end, just before
	the global comment)
*/
static uLong unzlocal_SearchCentralDir(IDataSource* fin) {
	unsigned char* buf;
	uLong          uSizeFile;
	uLong          uBackRead;
	uLong          uMaxBack  = 0xffff; /* maximum size of global comment */
	uLong          uPosFound = 0;

	uSizeFile = fin->getSize();

	if (uMaxBack > uSizeFile) {
		uMaxBack = uSizeFile;
	}

	buf = new unsigned char[BUFREADCOMMENT + 4];

	uBackRead = 4;
	while (uBackRead < uMaxBack) {
		uLong uReadSize;
		uLong uReadPos;
		int   i;
		if (uBackRead + BUFREADCOMMENT > uMaxBack) {
			uBackRead = uMaxBack;
		} else {
			uBackRead += BUFREADCOMMENT;
		}
		uReadPos = uSizeFile - uBackRead;

		uReadSize = ((BUFREADCOMMENT + 4) < (uSizeFile - uReadPos)) ? (BUFREADCOMMENT + 4) : (uSizeFile - uReadPos);
		fin->seek(uReadPos);

		if (UNZ_OK != unzlocal_read(fin, buf, uReadSize)) {
			break;
		}

		for (i = uReadSize - 3; (i--) > 0;) {
			if ((buf[i] == 0x50) && (buf[i + 1] == 0x4b) && (buf[i + 2] == 0x05) && (buf[i + 3] == 0x06)) {
				uPosFound = uReadPos + i;
				break;
			}
		}

		if (uPosFound != 0) {
			break;
		}
	}
	delete[] buf;
	return uPosFound;
}

static int ZEXPORT unzCloseInternal(unz_s*& file) {
	int err = UNZ_OK;
	if (file == nullptr) {
		return UNZ_PARAMERROR;
	}

	if (file->pfile_in_zip_read != nullptr) {
		err = unzCloseCurrentFile(file);
	}
	file->file = nullptr;

	delete file;
	file = nullptr;
	return err;
}

void unzFile::set(unz_s* newdata) {
	if (data) {
		unzCloseInternal(data);
	}
	data = newdata;
}

unzFile::~unzFile() {
	if (data) {
		unzCloseInternal(data);
	}
}

/*
  Open a Zip file. path contain the full pathname (by example,
	 on a Windows NT computer "c:\\test\\zlib109.zip" or on an Unix computer
	 "zlib/zlib109.zip".
	 If the zipfile cannot be opened (file don't exist or in not valid), the
	   return value is nullptr.
	 Else, the return value is a unzFile Handle, usable with other function
	   of this unzip package.
*/
extern unzFile ZEXPORT unzOpen(IDataSource* fin) {
	unzFile s  = {};
	unz_s   us = {};
	uLong   central_pos;
	uLong   uL;

	uLong number_disk;         /* number of the current dist, used for
								  spaning ZIP, unsupported, always 0*/
	uLong number_disk_with_CD; /* number the the disk with central dir, used
								  for spaning ZIP, unsupported, always 0*/
	uLong number_entry_CD;     /* total number of entries in
								  the central dir
								  (same than number_entry on nospan) */

	int err = UNZ_OK;

	if (unz_copyright[0] != ' ') {
		return {};
	}

	central_pos = unzlocal_SearchCentralDir(fin);
	if (central_pos == 0) {
		err = UNZ_UNKNOWNFILEERROR;
	}

	fin->seek(central_pos);

	/* the signature, already checked */
	if (err == UNZ_OK) {
		err = unzlocal_getLong(fin, &uL);
	}

	/* number of this disk */
	if (err == UNZ_OK) {
		err = unzlocal_getShort(fin, &number_disk);
	}

	/* number of the disk with the start of the central directory */
	if (err == UNZ_OK) {
		err = unzlocal_getShort(fin, &number_disk_with_CD);
	}

	/* total number of entries in the central dir on this disk */
	if (err == UNZ_OK) {
		err = unzlocal_getShort(fin, &us.gi.number_entry);
	}

	/* total number of entries in the central dir */
	if (err == UNZ_OK) {
		err = unzlocal_getShort(fin, &number_entry_CD);

		if ((number_entry_CD != us.gi.number_entry) || (number_disk_with_CD != 0) || (number_disk != 0)) {
			err = UNZ_BADZIPFILE;
		}
	}

	/* size of the central directory */
	if (err == UNZ_OK) {
		err = unzlocal_getLong(fin, &us.size_central_dir);
	}

	/* offset of start of central directory with respect to the
		  starting disk number */
	if (err == UNZ_OK) {
		err = unzlocal_getLong(fin, &us.offset_central_dir);
	}

	/* zipfile comment length */
	if (err == UNZ_OK) {
		err = unzlocal_getShort(fin, &us.gi.size_comment);
	}

	if ((central_pos < us.offset_central_dir + us.size_central_dir) && (err == UNZ_OK)) {
		err = UNZ_BADZIPFILE;
	}

	if (err != UNZ_OK) {
		return {};
	}

	us.file                    = fin;
	us.byte_before_the_zipfile = central_pos - (us.offset_central_dir + us.size_central_dir);
	us.central_pos             = central_pos;
	us.num_file                = 0;

	s.set(new unz_s(std::move(us)));
	err = unzGoToFirstFile(s);
	if (err != UNZ_OK) {
		return {};
	}

	return s;
}

/*
  Write info about the ZipFile in the *pglobal_info structure.
  No preparation of the structure is needed
  return UNZ_OK if there is no problem. */
extern int ZEXPORT unzGetGlobalInfo(unz_s* file, unz_global_info* pglobal_info) {
	if (file == nullptr) {
		return UNZ_PARAMERROR;
	}
	*pglobal_info = file->gi;
	return UNZ_OK;
}

/*
   Translate date/time from Dos format to tm_unz (readable more easilty)
*/
static void unzlocal_DosDateToTmuDate(uLong ulDosDate, tm_unz* ptm) {
	uLong uDate;
	uDate        = ulDosDate >> 16;
	ptm->tm_mday = uDate & 0x1fu;
	ptm->tm_mon  = ((uDate & 0x1E0u) / 0x20u) - 1u;
	ptm->tm_year = ((uDate & 0x0FE00u) / 0x0200u) + 1980u;

	ptm->tm_hour = (ulDosDate & 0xF800u) / 0x800u;
	ptm->tm_min  = (ulDosDate & 0x7E0u) / 0x20u;
	ptm->tm_sec  = 2u * (ulDosDate & 0x1fu);
}

/*
  Get Info about the current file in the zipfile, with internal only info
*/
static int unzlocal_GetCurrentFileInfoInternal(
		unz_s* file, unz_file_info* pfile_info, unz_file_info_internal* pfile_info_internal, char* szFileName,
		uLong fileNameBufferSize, void* extraField, uLong extraFieldBufferSize, char* szComment, uLong commentBufferSize) {
	unz_file_info          file_info          = {};
	unz_file_info_internal file_info_internal = {};
	int                    err                = UNZ_OK;
	uLong                  uMagic;
	long                   lSeek = 0;

	if (file == nullptr) {
		return UNZ_PARAMERROR;
	}
	file->file->seek(file->pos_in_central_dir + file->byte_before_the_zipfile);
	if (!file->file->good()) {
		err = UNZ_UNKNOWNFILEERROR;
	}

	/* we check the magic */
	if (err == UNZ_OK) {
		err = unzlocal_getLong(file->file, &uMagic);
		if (err == UNZ_OK && uMagic != 0x02014b50) {
			err = UNZ_BADZIPFILE;
		}
	}

	if (err == UNZ_OK) {
		err = unzlocal_getShort(file->file, &file_info.version);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getShort(file->file, &file_info.version_needed);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getShort(file->file, &file_info.flag);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getShort(file->file, &file_info.compression_method);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getLong(file->file, &file_info.dosDate);
	}

	if (err == UNZ_OK) {
		unzlocal_DosDateToTmuDate(file_info.dosDate, &file_info.tmu_date);

		err = unzlocal_getLong(file->file, &file_info.crc);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getLong(file->file, &file_info.compressed_size);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getLong(file->file, &file_info.uncompressed_size);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getShort(file->file, &file_info.size_filename);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getShort(file->file, &file_info.size_file_extra);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getShort(file->file, &file_info.size_file_comment);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getShort(file->file, &file_info.disk_num_start);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getShort(file->file, &file_info.internal_fa);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getLong(file->file, &file_info.external_fa);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getLong(file->file, &file_info_internal.offset_curfile);
	}

	lSeek += file_info.size_filename;
	if ((err == UNZ_OK) && (szFileName != nullptr)) {
		uLong uSizeRead;
		if (file_info.size_filename < fileNameBufferSize) {
			*(szFileName + file_info.size_filename) = '\0';
			uSizeRead                               = file_info.size_filename;
		} else {
			uSizeRead = fileNameBufferSize;
		}

		if ((file_info.size_filename > 0) && (fileNameBufferSize > 0)) {
			err = unzlocal_read(file->file, szFileName, uSizeRead);
		}
		lSeek -= uSizeRead;
	}

	if ((err == UNZ_OK) && (extraField != nullptr)) {
		uLong uSizeRead;
		if (file_info.size_file_extra < extraFieldBufferSize) {
			uSizeRead = file_info.size_file_extra;
		} else {
			uSizeRead = extraFieldBufferSize;
		}

		if (lSeek != 0) {
			file->file->skip(lSeek);
			if (file->file->good()) {
				lSeek = 0;
			} else {
				err = UNZ_UNKNOWNFILEERROR;
			}
		}
		if ((file_info.size_file_extra > 0) && (extraFieldBufferSize > 0)) {
			err = unzlocal_read(file->file, extraField, uSizeRead);
		}
		lSeek += file_info.size_file_extra - uSizeRead;
	} else {
		lSeek += file_info.size_file_extra;
	}

	if ((err == UNZ_OK) && (szComment != nullptr)) {
		uLong uSizeRead;
		if (file_info.size_file_comment < commentBufferSize) {
			*(szComment + file_info.size_file_comment) = '\0';
			uSizeRead                                  = file_info.size_file_comment;
		} else {
			uSizeRead = commentBufferSize;
		}

		if (lSeek != 0) {
			file->file->skip(lSeek);
			if (file->file->good()) {
				lSeek = 0;
			} else {
				err = UNZ_UNKNOWNFILEERROR;
			}
		}
		if (err == UNZ_OK && (file_info.size_file_comment > 0) && (commentBufferSize > 0)) {
			err = unzlocal_read(file->file, szComment, uSizeRead);
		}
		lSeek += file_info.size_file_comment - uSizeRead;
	} else {
		lSeek += file_info.size_file_comment;
	}

	if ((err == UNZ_OK) && (pfile_info != nullptr)) {
		*pfile_info = file_info;
	}

	if ((err == UNZ_OK) && (pfile_info_internal != nullptr)) {
		*pfile_info_internal = file_info_internal;
	}

	return err;
}

/*
  Write info about the ZipFile in the *pglobal_info structure.
  No preparation of the structure is needed
  return UNZ_OK if there is no problem.
*/
extern int ZEXPORT unzGetCurrentFileInfo(
		unz_s* file, unz_file_info* pfile_info, char* szFileName, uLong fileNameBufferSize, void* extraField,
		uLong extraFieldBufferSize, char* szComment, uLong commentBufferSize) {
	return unzlocal_GetCurrentFileInfoInternal(
			file, pfile_info, nullptr, szFileName, fileNameBufferSize, extraField, extraFieldBufferSize, szComment,
			commentBufferSize);
}

extern int ZEXPORT unzGetCurrentFileInfo(
		unz_s* file, unz_file_info* pfile_info, std::string& fileName, void* extraField, uLong extraFieldBufferSize,
		char* szComment, uLong commentBufferSize) {
	fileName.resize(file->cur_file_info.size_filename);
	return unzlocal_GetCurrentFileInfoInternal(
			file, pfile_info, nullptr, fileName.data(), fileName.size(), extraField, extraFieldBufferSize, szComment,
			commentBufferSize);
}

/*
  Set the current file of the zipfile to the first file.
  return UNZ_OK if there is no problem
*/
extern int ZEXPORT unzGoToFirstFile(unz_s* file) {
	int err = UNZ_OK;
	if (file == nullptr) {
		return UNZ_PARAMERROR;
	}
	file->pos_in_central_dir = file->offset_central_dir;
	file->num_file           = 0;
	err                      = unzlocal_GetCurrentFileInfoInternal(
            file, &file->cur_file_info, &file->cur_file_info_internal, nullptr, 0, nullptr, 0, nullptr, 0);
	file->current_file_ok = (err == UNZ_OK);
	return err;
}

/*
  Set the current file of the zipfile to the next file.
  return UNZ_OK if there is no problem
  return UNZ_END_OF_LIST_OF_FILE if the actual file was the latest.
*/
extern int ZEXPORT unzGoToNextFile(unz_s* file) {
	int err;

	if (file == nullptr) {
		return UNZ_PARAMERROR;
	}
	if (!file->current_file_ok) {
		return UNZ_END_OF_LIST_OF_FILE;
	}
	if (file->num_file + 1 == file->gi.number_entry) {
		return UNZ_END_OF_LIST_OF_FILE;
	}

	file->pos_in_central_dir += SIZECENTRALDIRITEM + file->cur_file_info.size_filename + file->cur_file_info.size_file_extra
								+ file->cur_file_info.size_file_comment;
	file->num_file++;
	err = unzlocal_GetCurrentFileInfoInternal(
			file, &file->cur_file_info, &file->cur_file_info_internal, nullptr, 0, nullptr, 0, nullptr, 0);
	file->current_file_ok = (err == UNZ_OK);
	return err;
}

/*
  Try locate the file szFileName in the zipfile.
  For the iCaseSensitivity signification, see unzipStringFileNameCompare

  return value :
  UNZ_OK if the file is found. It becomes the current file.
  UNZ_END_OF_LIST_OF_FILE if the file is not found
*/
extern int ZEXPORT unzLocateFile(unz_s* file, const char* szFileName, int iCaseSensitivity) {
	int err;

	uLong num_fileSaved;
	uLong pos_in_central_dirSaved;

	if (file == nullptr) {
		return UNZ_PARAMERROR;
	}

	if (strlen(szFileName) >= UNZ_MAXFILENAMEINZIP) {
		return UNZ_PARAMERROR;
	}

	if (!file->current_file_ok) {
		return UNZ_END_OF_LIST_OF_FILE;
	}

	num_fileSaved           = file->num_file;
	pos_in_central_dirSaved = file->pos_in_central_dir;

	err = unzGoToFirstFile(file);

	while (err == UNZ_OK) {
		char szCurrentFileName[UNZ_MAXFILENAMEINZIP + 1]{};
		unzGetCurrentFileInfo(file, nullptr, szCurrentFileName, sizeof(szCurrentFileName) - 1, nullptr, 0, nullptr, 0);
		if (unzStringFileNameCompare(szCurrentFileName, szFileName, iCaseSensitivity) == 0) {
			return UNZ_OK;
		}
		err = unzGoToNextFile(file);
	}

	file->num_file           = num_fileSaved;
	file->pos_in_central_dir = pos_in_central_dirSaved;
	return err;
}

/*
  Read the local header of the current zipfile
  Check the coherency of the local header and info in the end of central
		directory about this file
  store in *piSizeVar the size of extra info in local header
		(filename and size of extra field data)
*/
static int unzlocal_CheckCurrentFileCoherencyHeader(
		unz_s* s, uInt* piSizeVar, uLong* poffset_local_extrafield, uInt* psize_local_extrafield) {
	uLong uMagic;
	uLong uData;
	uLong uFlags;
	uLong size_filename;
	uLong size_extra_field;
	int   err = UNZ_OK;

	*piSizeVar                = 0;
	*poffset_local_extrafield = 0;
	*psize_local_extrafield   = 0;

	s->file->seek(s->cur_file_info_internal.offset_curfile);
	if (!s->file->good()) {
		return UNZ_UNKNOWNFILEERROR;
	}

	if (err == UNZ_OK) {
		err = unzlocal_getLong(s->file, &uMagic);
		if (err == UNZ_OK && uMagic != 0x04034b50) {
			err = UNZ_BADZIPFILE;
		}
	}

	if (err == UNZ_OK) {
		err = unzlocal_getShort(s->file, &uData);
	}
	/*
		else if ((err==UNZ_OK) && (uData!=s->cur_file_info.wVersion))
			err=UNZ_BADZIPFILE;
	*/
	if (err == UNZ_OK) {
		err = unzlocal_getShort(s->file, &uFlags);
	}

	if (err == UNZ_OK) {
		err = unzlocal_getShort(s->file, &uData);
	}
	if ((err == UNZ_OK) && (uData != s->cur_file_info.compression_method)) {
		err = UNZ_BADZIPFILE;
	}

	if ((err == UNZ_OK) && (s->cur_file_info.compression_method != 0) && (s->cur_file_info.compression_method != Z_DEFLATED)) {
		err = UNZ_BADZIPFILE;
	}

	if (err == UNZ_OK) {
		err = unzlocal_getLong(s->file, &uData); /* date/time */
	}

	if (err == UNZ_OK) {
		err = unzlocal_getLong(s->file, &uData); /* crc */
	}

	if ((err == UNZ_OK) && (uData != s->cur_file_info.crc) && ((uFlags & 8) == 0)) {
		err = UNZ_BADZIPFILE;
	}

	if (err == UNZ_OK) {
		err = unzlocal_getLong(s->file, &uData); /* size compr */
	}
	if ((err == UNZ_OK) && (uData != s->cur_file_info.compressed_size) && ((uFlags & 8) == 0)) {
		err = UNZ_BADZIPFILE;
	}

	if (err == UNZ_OK) {
		err = unzlocal_getLong(s->file, &uData); /* size uncompr */
	}
	if ((err == UNZ_OK) && (uData != s->cur_file_info.uncompressed_size) && ((uFlags & 8) == 0)) {
		err = UNZ_BADZIPFILE;
	}

	if (err == UNZ_OK) {
		err = unzlocal_getShort(s->file, &size_filename);

		if ((err == UNZ_OK) && (size_filename != s->cur_file_info.size_filename)) {
			err = UNZ_BADZIPFILE;
		}

		*piSizeVar += size_filename;
	}

	if (err == UNZ_OK) {
		err = unzlocal_getShort(s->file, &size_extra_field);

		*poffset_local_extrafield = s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER + size_filename;
		*psize_local_extrafield   = size_extra_field;

		*piSizeVar += size_extra_field;
	}

	return err;
}

/*
  Open for reading data the current file in the zipfile.
  If there is no error and the file is opened, the return value is UNZ_OK.
*/
extern int ZEXPORT unzOpenCurrentFile(unz_s* file) {
	int                                      err = UNZ_OK;
	bool                                     Store;
	uInt                                     iSizeVar;
	std::unique_ptr<file_in_zip_read_info_s> pfile_in_zip_read_info;
	uLong                                    offset_local_extrafield; /* offset of the local extra field */
	uInt                                     size_local_extrafield;   /* size of the local extra field */

	if (file == nullptr) {
		return UNZ_PARAMERROR;
	}
	if (!file->current_file_ok) {
		return UNZ_PARAMERROR;
	}

	if (file->pfile_in_zip_read != nullptr) {
		err = unzCloseCurrentFile(file);
		if (err != UNZ_OK) {
			return err;
		}
	}

	if (unzlocal_CheckCurrentFileCoherencyHeader(file, &iSizeVar, &offset_local_extrafield, &size_local_extrafield) != UNZ_OK) {
		return UNZ_BADZIPFILE;
	}

	try {
		pfile_in_zip_read_info = std::make_unique<file_in_zip_read_info_s>();
	} catch (std::bad_alloc&) {
		std::cerr << "unzOpenCurrentFile: make_unique<file_in_zip_read_info_s> failed" << std::endl;
		return UNZ_INTERNALERROR;
	}

	pfile_in_zip_read_info->read_buffer             = new char[UNZ_BUFSIZE];
	pfile_in_zip_read_info->offset_local_extrafield = offset_local_extrafield;
	pfile_in_zip_read_info->size_local_extrafield   = size_local_extrafield;
	pfile_in_zip_read_info->pos_local_extrafield    = 0;

	if (pfile_in_zip_read_info->read_buffer == nullptr) {
		return UNZ_INTERNALERROR;
	}

	pfile_in_zip_read_info->stream_initialised = 0;

	if ((file->cur_file_info.compression_method != 0) && (file->cur_file_info.compression_method != Z_DEFLATED)) {
		return UNZ_BADZIPFILE;
	}
	Store = file->cur_file_info.compression_method == 0;

	pfile_in_zip_read_info->crc32_wait              = file->cur_file_info.crc;
	pfile_in_zip_read_info->crc32                   = 0;
	pfile_in_zip_read_info->compression_method      = file->cur_file_info.compression_method;
	pfile_in_zip_read_info->file                    = file->file;
	pfile_in_zip_read_info->byte_before_the_zipfile = file->byte_before_the_zipfile;

	pfile_in_zip_read_info->stream.total_out = 0;

	if (!Store) {
		pfile_in_zip_read_info->stream.zalloc = nullptr;
		pfile_in_zip_read_info->stream.zfree  = nullptr;
		pfile_in_zip_read_info->stream.opaque = nullptr;

		err = U7inflateInit2(&pfile_in_zip_read_info->stream);
		if (err == Z_OK) {
			pfile_in_zip_read_info->stream_initialised = 1;
		}
		/* windowBits is passed < 0 to tell that there is no zlib header.
		 * Note that in this case inflate *requires* an extra "dummy" byte
		 * after the compressed stream in order to complete decompression and
		 * return Z_STREAM_END.
		 * In unzip, i don't wait absolutely Z_STREAM_END because I known the
		 * size of both compressed and uncompressed data
		 */
	}
	pfile_in_zip_read_info->rest_read_compressed   = file->cur_file_info.compressed_size;
	pfile_in_zip_read_info->rest_read_uncompressed = file->cur_file_info.uncompressed_size;

	pfile_in_zip_read_info->pos_in_zipfile = file->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER + iSizeVar;

	pfile_in_zip_read_info->stream.avail_in = 0;

	file->pfile_in_zip_read = std::move(pfile_in_zip_read_info);
	return UNZ_OK;
}

/*
  Read bytes from the current file.
  buf contain buffer where data must be copied
  len the size of buf.

  return the number of byte copied if somes bytes are copied
  return 0 if the end of file was reached
  return <0 with error code if there is an error
	(UNZ_UNKNOWNERROR for IO error, or zLib error for uncompress error)
*/
extern int ZEXPORT unzReadCurrentFile(unz_s* file, voidp buf, unsigned len) {
	int  err   = UNZ_OK;
	uInt iRead = 0;
	if (file == nullptr) {
		return UNZ_PARAMERROR;
	}
	auto& pfile_in_zip_read_info = file->pfile_in_zip_read;

	if (!pfile_in_zip_read_info) {
		return UNZ_PARAMERROR;
	}

	if (pfile_in_zip_read_info->read_buffer == nullptr) {
		return UNZ_END_OF_LIST_OF_FILE;
	}
	if (len == 0) {
		return 0;
	}

	pfile_in_zip_read_info->stream.next_out = static_cast<Bytef*>(buf);

	pfile_in_zip_read_info->stream.avail_out = len;

	if (len > pfile_in_zip_read_info->rest_read_uncompressed) {
		pfile_in_zip_read_info->stream.avail_out = pfile_in_zip_read_info->rest_read_uncompressed;
	}

	while (pfile_in_zip_read_info->stream.avail_out > 0) {
		if ((pfile_in_zip_read_info->stream.avail_in == 0) && (pfile_in_zip_read_info->rest_read_compressed > 0)) {
			uInt uReadThis = UNZ_BUFSIZE;
			if (pfile_in_zip_read_info->rest_read_compressed < uReadThis) {
				uReadThis = pfile_in_zip_read_info->rest_read_compressed;
			}
			if (uReadThis == 0) {
				return UNZ_EOF;
			}
			pfile_in_zip_read_info->file->seek(
					pfile_in_zip_read_info->pos_in_zipfile + pfile_in_zip_read_info->byte_before_the_zipfile);
			if (!pfile_in_zip_read_info->file->good()) {
				return UNZ_UNKNOWNFILEERROR;
			}
			auto err = unzlocal_read(pfile_in_zip_read_info->file, pfile_in_zip_read_info->read_buffer, uReadThis);
			if (err != UNZ_OK) {
				return err;
			}
			pfile_in_zip_read_info->pos_in_zipfile += uReadThis;

			pfile_in_zip_read_info->rest_read_compressed -= uReadThis;

			pfile_in_zip_read_info->stream.next_in  = reinterpret_cast<Bytef*>(pfile_in_zip_read_info->read_buffer);
			pfile_in_zip_read_info->stream.avail_in = uReadThis;
		}

		if (pfile_in_zip_read_info->compression_method == 0) {
			uInt uDoCopy;
			uInt i;
			if (pfile_in_zip_read_info->stream.avail_out < pfile_in_zip_read_info->stream.avail_in) {
				uDoCopy = pfile_in_zip_read_info->stream.avail_out;
			} else {
				uDoCopy = pfile_in_zip_read_info->stream.avail_in;
			}

			for (i = 0; i < uDoCopy; i++) {
				*(pfile_in_zip_read_info->stream.next_out + i) = *(pfile_in_zip_read_info->stream.next_in + i);
			}

			pfile_in_zip_read_info->crc32 = crc32(pfile_in_zip_read_info->crc32, pfile_in_zip_read_info->stream.next_out, uDoCopy);
			pfile_in_zip_read_info->rest_read_uncompressed -= uDoCopy;
			pfile_in_zip_read_info->stream.avail_in -= uDoCopy;
			pfile_in_zip_read_info->stream.avail_out -= uDoCopy;
			pfile_in_zip_read_info->stream.next_out += uDoCopy;
			pfile_in_zip_read_info->stream.next_in += uDoCopy;
			pfile_in_zip_read_info->stream.total_out += uDoCopy;
			iRead += uDoCopy;
		} else {
			uLong        uTotalOutBefore;
			uLong        uTotalOutAfter;
			const Bytef* bufBefore;
			uLong        uOutThis;
			const int    flush = Z_SYNC_FLUSH;

			uTotalOutBefore = pfile_in_zip_read_info->stream.total_out;
			bufBefore       = pfile_in_zip_read_info->stream.next_out;

			/*
			if ((pfile_in_zip_read_info->rest_read_uncompressed ==
					 pfile_in_zip_read_info->stream.avail_out) &&
				(pfile_in_zip_read_info->rest_read_compressed == 0))
				flush = Z_FINISH;
			*/
			err = inflate(&pfile_in_zip_read_info->stream, flush);

			uTotalOutAfter = pfile_in_zip_read_info->stream.total_out;
			uOutThis       = uTotalOutAfter - uTotalOutBefore;

			pfile_in_zip_read_info->crc32 = crc32(pfile_in_zip_read_info->crc32, bufBefore, (uOutThis));

			pfile_in_zip_read_info->rest_read_uncompressed -= uOutThis;

			iRead += (uTotalOutAfter - uTotalOutBefore);

			if (err == Z_STREAM_END) {
				return (iRead == 0) ? UNZ_EOF : iRead;
			}
			if (err != Z_OK) {
				break;
			}
		}
	}

	if (err == Z_OK) {
		return iRead;
	}
	return err;
}

/*
  Give the current position in uncompressed data
*/
extern z_off_t ZEXPORT unztell(unz_s* file) {
	if (file == nullptr) {
		return UNZ_PARAMERROR;
	}
	auto& pfile_in_zip_read_info = file->pfile_in_zip_read;

	if (!pfile_in_zip_read_info) {
		return UNZ_PARAMERROR;
	}

	return static_cast<z_off_t>(pfile_in_zip_read_info->stream.total_out);
}

/*
  return 1 if the end of file was reached, 0 elsewhere
*/
extern int ZEXPORT unzeof(unz_s* file) {
	if (file == nullptr) {
		return UNZ_PARAMERROR;
	}
	auto& pfile_in_zip_read_info = file->pfile_in_zip_read;

	if (!pfile_in_zip_read_info) {
		return UNZ_PARAMERROR;
	}

	if (pfile_in_zip_read_info->rest_read_uncompressed == 0) {
		return 1;
	} else {
		return 0;
	}
}

/*
  Read extra field from the current file (opened by unzOpenCurrentFile)
  This is the local-header version of the extra field (sometimes, there is
	more info in the local-header version than in the central-header)

  if buf==nullptr, it return the size of the local extra field that can be read

  if buf!=nullptr, len is the size of the buffer, the extra header is copied in
	buf.
  the return value is the number of bytes copied in buf, or (if <0)
	the error code
*/
extern int ZEXPORT unzGetLocalExtrafield(unz_s* file, voidp buf, unsigned len) {
	uInt  read_now;
	uLong size_to_read;

	if (file == nullptr) {
		return UNZ_PARAMERROR;
	}
	auto& pfile_in_zip_read_info = file->pfile_in_zip_read;

	if (!pfile_in_zip_read_info) {
		return UNZ_PARAMERROR;
	}

	size_to_read = (pfile_in_zip_read_info->size_local_extrafield - pfile_in_zip_read_info->pos_local_extrafield);

	if (buf == nullptr) {
		return size_to_read;
	}

	if (len > size_to_read) {
		read_now = size_to_read;
	} else {
		read_now = len;
	}

	if (read_now == 0) {
		return 0;
	}

	pfile_in_zip_read_info->file->seek(
			pfile_in_zip_read_info->offset_local_extrafield + pfile_in_zip_read_info->pos_local_extrafield);
	if (!pfile_in_zip_read_info->file->good()) {
		return UNZ_UNKNOWNFILEERROR;
	}

	auto err = unzlocal_read(pfile_in_zip_read_info->file, buf, size_to_read);
	if (err != UNZ_OK) {
		return err;
	}

	return read_now;
}

/*
  Close the file in zip opened with unzipOpenCurrentFile
  Return UNZ_CRCERROR if all the file was read but the CRC is not good
*/
extern int ZEXPORT unzCloseCurrentFile(unz_s* file) {
	int err = UNZ_OK;

	if (file == nullptr) {
		return UNZ_PARAMERROR;
	}
	auto& pfile_in_zip_read_info = file->pfile_in_zip_read;

	if (!pfile_in_zip_read_info) {
		return UNZ_PARAMERROR;
	}

	if (pfile_in_zip_read_info->rest_read_uncompressed == 0) {
		if (pfile_in_zip_read_info->crc32 != pfile_in_zip_read_info->crc32_wait) {
			err = UNZ_CRCERROR;
		}
	}

	delete[] pfile_in_zip_read_info->read_buffer;
	pfile_in_zip_read_info->read_buffer = nullptr;
	if (pfile_in_zip_read_info->stream_initialised) {
		inflateEnd(&pfile_in_zip_read_info->stream);
	}

	pfile_in_zip_read_info->stream_initialised = 0;

	file->pfile_in_zip_read.reset();

	return err;
}

/*
  Get the global comment string of the ZipFile, in the szComment buffer.
  uSizeBuf is the size of the szComment buffer.
  return the number of byte copied or an error code <0
*/
extern int ZEXPORT unzGetGlobalComment(unz_s* file, char* szComment, uLong uSizeBuf) {
	uLong uReadThis;
	if (file == nullptr) {
		return UNZ_PARAMERROR;
	}

	uReadThis = uSizeBuf;
	if (uReadThis > file->gi.size_comment) {
		uReadThis = file->gi.size_comment;
	}

	file->file->seek(file->central_pos + 22);
	if (!file->file->good()) {
		return UNZ_UNKNOWNFILEERROR;
	}

	if (uReadThis > 0) {
		*szComment = '\0';
		auto err   = unzlocal_read(file->file, szComment, uReadThis);
		if (err != UNZ_OK) {
			return err;
		}
	}

	if ((szComment != nullptr) && (uSizeBuf > file->gi.size_comment)) {
		*(szComment + file->gi.size_comment) = '\0';
	}
	return uReadThis;
}

int ZEXPORT unzExtractAllToPath(unz_s* unzipfile, const char* destpath) {
	int error               UNZ_OK;
	unz_file_info           fileinfo;
	std::unique_ptr<char[]> buffer;
	size_t                  buffer_size  = 0;
	const int               max_filename = 260;

	error = unzGoToFirstFile(unzipfile);
	std::string filepath;
	std::string outpath;
	filepath.reserve(max_filename);
	outpath.reserve(max_filename);
	if (error == UNZ_OK) {
		do {
			error = unzGetCurrentFileInfo(unzipfile, &fileinfo, filepath, nullptr, 0, nullptr, 0);
			if (error < UNZ_OK) {
				return error;
			}

			outpath = destpath;
			// Add slash if needed
			if (outpath[outpath.size() - 1] != '/') {
				outpath += "/";
			}
			outpath += filepath;

			// create directories as required
			std::string directory{get_directory_from_path(outpath)};
			U7mkdir((directory).c_str(), 0755, true);

			// Extract the file
			if (filepath[filepath.size() - 1] != '/') {
				if (error == UNZ_OK) {
					error = unzOpenCurrentFile(unzipfile);
				}
				if (error == UNZ_OK) {
					if (buffer_size < fileinfo.uncompressed_size) {
						try {
							buffer = std::make_unique<char[]>(buffer_size = fileinfo.uncompressed_size);
						} catch (std::bad_alloc&) {
							std::cerr << "ExtractZip: Failed to allocate "
										 "extract buffer "
										 "of size "
									  << fileinfo.uncompressed_size << " for file " << filepath << std::endl;

							return UNZ_INTERNALERROR;
						}
					}

					error = unzReadCurrentFile(unzipfile, buffer.get(), fileinfo.uncompressed_size);
					if (error > 0 && unsigned(error) == fileinfo.uncompressed_size) {
						error = UNZ_OK;
					}
				}
				if (error == UNZ_OK) {
					error = unzCloseCurrentFile(unzipfile);
				}

				if (error != UNZ_OK) {
					std::cerr << "ExtractZip: error trying to extract file: " << filepath << std::endl;
					return error;
				}
				// Write out the buffer
				std::unique_ptr<std::ostream> outfile;

				try {
					outfile = U7open_out(outpath.c_str());
				} catch (exult_exception&) {
					std::cerr << "ExtractZip: exception trying open file \"" << get_system_path(outpath) << "\" for writing"
							  << std::endl;
					return UNZ_UNKNOWNFILEERROR;
				}
				outfile->write(buffer.get(), fileinfo.uncompressed_size);
				if (outfile->fail()) {
					std::cerr << "ExtractZip: error trying to write "
								 "file "
							  << get_system_path(outpath) << std::endl;

					return UNZ_UNKNOWNFILEERROR;
				}
			}
		} while ((error = unzGoToNextFile(unzipfile)) == UNZ_OK);
		if (error == UNZ_END_OF_LIST_OF_FILE) {
			error = UNZ_OK;
		}
	}
	return error;
}

#endif /*HAVE_ZIP_SUPPORT*/
