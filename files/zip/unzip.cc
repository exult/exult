/* unzip.cc -- IO on .zip files using zlib
   Version 0.15 beta, Mar 19th, 1998,

   Modified by Ryan Nunn. Nov 9th 2001

   Read unzip.h for more info
*/



/* Added by Ryan Nunn */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef HAVE_ZIP_SUPPORT

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef NO_ERRNO_H
extern int errno;
#else
#  include <cerrno>
#endif
using namespace std;

#include "zlib.h"
#include "unzip.h"

#if !defined(unix) && !defined(CASESENSITIVITYDEFAULT_YES) && \
                      !defined(CASESENSITIVITYDEFAULT_NO)
#define CASESENSITIVITYDEFAULT_NO
#endif


#ifndef UNZ_BUFSIZE
#define UNZ_BUFSIZE (16384)
#endif

#ifndef UNZ_MAXFILENAMEINZIP
#define UNZ_MAXFILENAMEINZIP (256)
#endif

#define SIZECENTRALDIRITEM (0x2e)
#define SIZEZIPLOCALHEADER (0x1e)


/* I've found an old Unix (a SunOS 4.1.3_U1) without all SEEK_* defined.... */

#ifndef SEEK_CUR
#define SEEK_CUR    1
#endif

#ifndef SEEK_END
#define SEEK_END    2
#endif

#ifndef SEEK_SET
#define SEEK_SET    0
#endif

const char unz_copyright[] =
    " unzip 0.15 Copyright 1998 Gilles Vollant ";

/* unz_file_info_interntal contain internal info about a file in zipfile*/
struct unz_file_info_internal {
	uLong offset_curfile;/* relative offset of local header 4 bytes */
};


/* file_in_zip_read_info_s contain internal information about a file in zipfile,
    when reading and decompress it */
struct file_in_zip_read_info_s {
	char  *read_buffer;         /* internal buffer for compressed data */
	z_stream stream;            /* zLib stream structure for inflate */

	uLong pos_in_zipfile;       /* position in byte on the zipfile, for fseek*/
	uLong stream_initialised;   /* flag set if stream structure is initialised*/

	uLong offset_local_extrafield;/* offset of the local extra field */
	uInt  size_local_extrafield;/* size of the local extra field */
	uLong pos_local_extrafield;   /* position in the local extra field in read*/

	uLong crc32;                /* crc32 of all data uncompressed */
	uLong crc32_wait;           /* crc32 we must obtain after decompress all */
	uLong rest_read_compressed; /* number of byte to be decompressed */
	uLong rest_read_uncompressed;/*number of byte to be obtained after decomp*/
	FILE *file;                 /* io structore of the zipfile */
	uLong compression_method;   /* compression method (0==store) */
	uLong byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)*/
};


/* unz_s contain internal information about the zipfile
*/
struct unz_s {
	FILE *file;                 /* io structore of the zipfile */
	unz_global_info gi;       /* public global information */
	uLong byte_before_the_zipfile;/* byte before the zipfile, (>0 for sfx)*/
	uLong num_file;             /* number of the current file in the zipfile*/
	uLong pos_in_central_dir;   /* pos of the current file in the central dir*/
	uLong current_file_ok;      /* flag about the usability of the current file*/
	uLong central_pos;          /* position of the beginning of the central dir*/

	uLong size_central_dir;     /* size of the central directory  */
	uLong offset_central_dir;   /* offset of start of central directory with
                                   respect to the starting disk number */

	unz_file_info cur_file_info; /* public info about the current file in zip*/
	unz_file_info_internal cur_file_info_internal; /* private info about it*/
	file_in_zip_read_info_s *pfile_in_zip_read; /* structure about the current
                                        file if we are decompressing it */
};


/* ===========================================================================
     Read a byte from a gz_stream; update next_in and avail_in. Return EOF
   for end of file.
   IN assertion: the stream s has been sucessfully opened for reading.
*/


static int unzlocal_getByte(FILE *fin, int *pi) {
	unsigned char c;
	int err = fread(&c, 1, 1, fin);
	if (err == 1) {
		*pi = c;
		return UNZ_OK;
	} else {
		if (ferror(fin))
			return UNZ_ERRNO;
		else
			return UNZ_EOF;
	}
}


/* ===========================================================================
   Reads a long in LSB order from the given gz_stream. Sets
*/
static int unzlocal_getShort(FILE *fin, uLong *pX) {
	uLong x ;
	int i;
	int err;

	err = unzlocal_getByte(fin, &i);
	x = static_cast<uLong>(i);

	if (err == UNZ_OK)
		err = unzlocal_getByte(fin, &i);
	x += static_cast<uLong>(i) << 8;

	if (err == UNZ_OK)
		*pX = x;
	else
		*pX = 0;
	return err;
}

static int unzlocal_getLong(FILE *fin, uLong *pX) {
	uLong x ;
	int i;
	int err;

	err = unzlocal_getByte(fin, &i);
	x = static_cast<uLong>(i);

	if (err == UNZ_OK)
		err = unzlocal_getByte(fin, &i);
	x += static_cast<uLong>(i)<<8;

	if (err == UNZ_OK)
		err = unzlocal_getByte(fin, &i);
	x += static_cast<uLong>(i) << 16;

	if (err == UNZ_OK)
		err = unzlocal_getByte(fin, &i);
	x += static_cast<uLong>(i) << 24;

	if (err == UNZ_OK)
		*pX = x;
	else
		*pX = 0;
	return err;
}


/* My own strcmpi / strcasecmp */
static int strcmpcasenosensitive_internal(const char *fileName1, const char *fileName2) {
	for (;;) {
		auto c1 = std::toupper(static_cast<unsigned char>(*(fileName1++)));
		auto c2 = std::toupper(static_cast<unsigned char>(*(fileName2++)));
		if (c1 == '\0')
			return (c2 == '\0') ? 0 : -1;
		if (c2 == '\0')
			return 1;
		if (c1 < c2)
			return -1;
		if (c1 > c2)
			return 1;
	}
}


#ifdef  CASESENSITIVITYDEFAULT_NO
#define CASESENSITIVITYDEFAULTVALUE 2
#else
#define CASESENSITIVITYDEFAULTVALUE 1
#endif

#ifndef STRCMPCASENOSENTIVEFUNCTION
#define STRCMPCASENOSENTIVEFUNCTION strcmpcasenosensitive_internal
#endif

/*
   Compare two filename (fileName1,fileName2).
   If iCaseSenisivity = 1, comparision is case sensitivity (like strcmp)
   If iCaseSenisivity = 2, comparision is not case sensitivity (like strcmpi
                                                                or strcasecmp)
   If iCaseSenisivity = 0, case sensitivity is defaut of your operating system
        (like 1 on Unix, 2 on Windows)

*/
extern int ZEXPORT unzStringFileNameCompare(const char *fileName1, const char *fileName2, int iCaseSensitivity) {
	if (iCaseSensitivity == 0)
		iCaseSensitivity = CASESENSITIVITYDEFAULTVALUE;

	if (iCaseSensitivity == 1)
		return strcmp(fileName1, fileName2);

	return STRCMPCASENOSENTIVEFUNCTION(fileName1, fileName2);
}

#define BUFREADCOMMENT (0x400)

/*
  Locate the Central directory of a zipfile (at the end, just before
    the global comment)
*/
static uLong unzlocal_SearchCentralDir(FILE *fin) {
	unsigned char *buf;
	uLong uSizeFile;
	uLong uBackRead;
	uLong uMaxBack = 0xffff; /* maximum size of global comment */
	uLong uPosFound = 0;

	if (fseek(fin, 0, SEEK_END) != 0)
		return 0;


	uSizeFile = ftell(fin);

	if (uMaxBack > uSizeFile)
		uMaxBack = uSizeFile;

	buf = new unsigned char[BUFREADCOMMENT + 4];

	uBackRead = 4;
	while (uBackRead < uMaxBack) {
		uLong uReadSize;
		uLong uReadPos ;
		int i;
		if (uBackRead + BUFREADCOMMENT > uMaxBack)
			uBackRead = uMaxBack;
		else
			uBackRead += BUFREADCOMMENT;
		uReadPos = uSizeFile - uBackRead ;

		uReadSize = ((BUFREADCOMMENT + 4) < (uSizeFile - uReadPos)) ?
		            (BUFREADCOMMENT + 4) : (uSizeFile - uReadPos);
		if (fseek(fin, uReadPos, SEEK_SET) != 0)
			break;

		if (fread(buf, uReadSize, 1, fin) != 1)
			break;

		for (i = uReadSize - 3; (i--) > 0;)
			if ((buf[i] == 0x50) && (buf[i + 1] == 0x4b) &&
			        (buf[i + 2] == 0x05) && (buf[i + 3] == 0x06)) {
				uPosFound = uReadPos + i;
				break;
			}

		if (uPosFound != 0)
			break;
	}
	delete [] buf;
	return uPosFound;
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
extern unzFile ZEXPORT unzOpen(const char *path) {
	unz_s us;
	unz_s *s;
	uLong central_pos;
	uLong uL;
	FILE *fin ;

	uLong number_disk;          /* number of the current dist, used for
                                   spaning ZIP, unsupported, always 0*/
	uLong number_disk_with_CD;  /* number the the disk with central dir, used
                                   for spaning ZIP, unsupported, always 0*/
	uLong number_entry_CD;      /* total number of entries in
                                   the central dir
                                   (same than number_entry on nospan) */

	int err = UNZ_OK;

	if (unz_copyright[0] != ' ')
		return nullptr;

	fin = fopen(path, "rb");
	if (fin == nullptr)
		return nullptr;

	central_pos = unzlocal_SearchCentralDir(fin);
	if (central_pos == 0)
		err = UNZ_ERRNO;

	if (fseek(fin, central_pos, SEEK_SET) != 0)
		err = UNZ_ERRNO;

	/* the signature, already checked */
	if (unzlocal_getLong(fin, &uL) != UNZ_OK)
		err = UNZ_ERRNO;

	/* number of this disk */
	if (unzlocal_getShort(fin, &number_disk) != UNZ_OK)
		err = UNZ_ERRNO;

	/* number of the disk with the start of the central directory */
	if (unzlocal_getShort(fin, &number_disk_with_CD) != UNZ_OK)
		err = UNZ_ERRNO;

	/* total number of entries in the central dir on this disk */
	if (unzlocal_getShort(fin, &us.gi.number_entry) != UNZ_OK)
		err = UNZ_ERRNO;

	/* total number of entries in the central dir */
	if (unzlocal_getShort(fin, &number_entry_CD) != UNZ_OK)
		err = UNZ_ERRNO;

	if ((number_entry_CD != us.gi.number_entry) ||
	        (number_disk_with_CD != 0) ||
	        (number_disk != 0))
		err = UNZ_BADZIPFILE;

	/* size of the central directory */
	if (unzlocal_getLong(fin, &us.size_central_dir) != UNZ_OK)
		err = UNZ_ERRNO;

	/* offset of start of central directory with respect to the
	      starting disk number */
	if (unzlocal_getLong(fin, &us.offset_central_dir) != UNZ_OK)
		err = UNZ_ERRNO;

	/* zipfile comment length */
	if (unzlocal_getShort(fin, &us.gi.size_comment) != UNZ_OK)
		err = UNZ_ERRNO;

	if ((central_pos < us.offset_central_dir + us.size_central_dir) &&
	        (err == UNZ_OK))
		err = UNZ_BADZIPFILE;

	if (err != UNZ_OK) {
		fclose(fin);
		return nullptr;
	}

	us.file = fin;
	us.byte_before_the_zipfile = central_pos -
	                             (us.offset_central_dir + us.size_central_dir);
	us.central_pos = central_pos;
	us.pfile_in_zip_read = nullptr;
	us.num_file = 0;

	s = new unz_s(us);
	unzGoToFirstFile(s);
	return s;
}


/*
  Close a ZipFile opened with unzipOpen.
  If there is files inside the .Zip opened with unzipOpenCurrentFile (see later),
    these files MUST be closed with unzipCloseCurrentFile before call unzipClose.
  return UNZ_OK if there is no problem. */
extern int ZEXPORT unzClose(unzFile file) {
	if (file == nullptr)
		return UNZ_PARAMERROR;

	if (file->pfile_in_zip_read != nullptr)
		unzCloseCurrentFile(file);

	fclose(file->file);
	delete file;
	return UNZ_OK;
}


/*
  Write info about the ZipFile in the *pglobal_info structure.
  No preparation of the structure is needed
  return UNZ_OK if there is no problem. */
extern int ZEXPORT unzGetGlobalInfo(unzFile file, unz_global_info *pglobal_info) {
	if (file == nullptr)
		return UNZ_PARAMERROR;
	*pglobal_info = file->gi;
	return UNZ_OK;
}


/*
   Translate date/time from Dos format to tm_unz (readable more easilty)
*/
static void unzlocal_DosDateToTmuDate(uLong ulDosDate, tm_unz *ptm) {
	uLong uDate;
	uDate = ulDosDate >> 16;
	ptm->tm_mday = uDate & 0x1fu;
	ptm->tm_mon = ((uDate & 0x1E0u) / 0x20u) - 1u;
	ptm->tm_year = ((uDate & 0x0FE00u) / 0x0200u) + 1980u;

	ptm->tm_hour = (ulDosDate & 0xF800u) / 0x800u;
	ptm->tm_min = (ulDosDate & 0x7E0u) / 0x20u;
	ptm->tm_sec = 2u * (ulDosDate & 0x1fu);
}

/*
  Get Info about the current file in the zipfile, with internal only info
*/
static int unzlocal_GetCurrentFileInfoInternal(unzFile file,
        unz_file_info *pfile_info,
        unz_file_info_internal *pfile_info_internal,
        char *szFileName,
        uLong fileNameBufferSize,
        void *extraField,
        uLong extraFieldBufferSize,
        char *szComment,
        uLong commentBufferSize);

static int unzlocal_GetCurrentFileInfoInternal(unzFile file,
        unz_file_info *pfile_info,
        unz_file_info_internal *pfile_info_internal,
        char *szFileName,
        uLong fileNameBufferSize,
        void *extraField,
        uLong extraFieldBufferSize,
        char *szComment,
        uLong commentBufferSize) {
	unz_file_info file_info;
	unz_file_info_internal file_info_internal;
	int err = UNZ_OK;
	uLong uMagic;
	long lSeek = 0;

	if (file == nullptr)
		return UNZ_PARAMERROR;
	if (fseek(file->file, file->pos_in_central_dir + file->byte_before_the_zipfile, SEEK_SET) != 0)
		err = UNZ_ERRNO;


	/* we check the magic */
	if (err == UNZ_OK) {
		if (unzlocal_getLong(file->file, &uMagic) != UNZ_OK)
			err = UNZ_ERRNO;
		else if (uMagic != 0x02014b50)
			err = UNZ_BADZIPFILE;
	}

	if (unzlocal_getShort(file->file, &file_info.version) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getShort(file->file, &file_info.version_needed) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getShort(file->file, &file_info.flag) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getShort(file->file, &file_info.compression_method) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getLong(file->file, &file_info.dosDate) != UNZ_OK)
		err = UNZ_ERRNO;

	unzlocal_DosDateToTmuDate(file_info.dosDate, &file_info.tmu_date);

	if (unzlocal_getLong(file->file, &file_info.crc) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getLong(file->file, &file_info.compressed_size) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getLong(file->file, &file_info.uncompressed_size) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getShort(file->file, &file_info.size_filename) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getShort(file->file, &file_info.size_file_extra) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getShort(file->file, &file_info.size_file_comment) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getShort(file->file, &file_info.disk_num_start) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getShort(file->file, &file_info.internal_fa) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getLong(file->file, &file_info.external_fa) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getLong(file->file, &file_info_internal.offset_curfile) != UNZ_OK)
		err = UNZ_ERRNO;

	lSeek += file_info.size_filename;
	if ((err == UNZ_OK) && (szFileName != nullptr)) {
		uLong uSizeRead ;
		if (file_info.size_filename < fileNameBufferSize) {
			*(szFileName + file_info.size_filename) = '\0';
			uSizeRead = file_info.size_filename;
		} else
			uSizeRead = fileNameBufferSize;

		if ((file_info.size_filename > 0) && (fileNameBufferSize > 0))
			if (fread(szFileName, uSizeRead, 1, file->file) != 1)
				err = UNZ_ERRNO;
		lSeek -= uSizeRead;
	}


	if ((err == UNZ_OK) && (extraField != nullptr)) {
		uLong uSizeRead ;
		if (file_info.size_file_extra < extraFieldBufferSize)
			uSizeRead = file_info.size_file_extra;
		else
			uSizeRead = extraFieldBufferSize;

		if (lSeek != 0) {
			if (fseek(file->file, lSeek, SEEK_CUR) == 0)
				lSeek = 0;
			else
				err = UNZ_ERRNO;
		}
		if ((file_info.size_file_extra > 0) && (extraFieldBufferSize > 0))
			if (fread(extraField, uSizeRead, 1, file->file) != 1)
				err = UNZ_ERRNO;
		lSeek += file_info.size_file_extra - uSizeRead;
	} else
		lSeek += file_info.size_file_extra;


	if ((err == UNZ_OK) && (szComment != nullptr)) {
		uLong uSizeRead ;
		if (file_info.size_file_comment < commentBufferSize) {
			*(szComment + file_info.size_file_comment) = '\0';
			uSizeRead = file_info.size_file_comment;
		} else
			uSizeRead = commentBufferSize;

		if (lSeek != 0) {
			if (fseek(file->file, lSeek, SEEK_CUR) == 0)
				lSeek = 0;
			else
				err = UNZ_ERRNO;
		}
		if ((file_info.size_file_comment > 0) && (commentBufferSize > 0))
			if (fread(szComment, uSizeRead, 1, file->file) != 1)
				err = UNZ_ERRNO;
		lSeek += file_info.size_file_comment - uSizeRead;
	} else
		lSeek += file_info.size_file_comment;

	if ((err == UNZ_OK) && (pfile_info != nullptr))
		*pfile_info = file_info;

	if ((err == UNZ_OK) && (pfile_info_internal != nullptr))
		*pfile_info_internal = file_info_internal;

	return err;
}



/*
  Write info about the ZipFile in the *pglobal_info structure.
  No preparation of the structure is needed
  return UNZ_OK if there is no problem.
*/
extern int ZEXPORT unzGetCurrentFileInfo(unzFile file,
        unz_file_info *pfile_info,
        char *szFileName,
        uLong fileNameBufferSize,
        void *extraField,
        uLong extraFieldBufferSize,
        char *szComment,
        uLong commentBufferSize) {
	return unzlocal_GetCurrentFileInfoInternal(file, pfile_info, nullptr,
	        szFileName, fileNameBufferSize,
	        extraField, extraFieldBufferSize,
	        szComment, commentBufferSize);
}

/*
  Set the current file of the zipfile to the first file.
  return UNZ_OK if there is no problem
*/
extern int ZEXPORT unzGoToFirstFile(unzFile file) {
	int err = UNZ_OK;
	if (file == nullptr)
		return UNZ_PARAMERROR;
	file->pos_in_central_dir = file->offset_central_dir;
	file->num_file = 0;
	err = unzlocal_GetCurrentFileInfoInternal(file, &file->cur_file_info,
	        &file->cur_file_info_internal,
	        nullptr, 0, nullptr, 0, nullptr, 0);
	file->current_file_ok = (err == UNZ_OK);
	return err;
}


/*
  Set the current file of the zipfile to the next file.
  return UNZ_OK if there is no problem
  return UNZ_END_OF_LIST_OF_FILE if the actual file was the latest.
*/
extern int ZEXPORT unzGoToNextFile(unzFile file) {
	int err;

	if (file == nullptr)
		return UNZ_PARAMERROR;
	if (!file->current_file_ok)
		return UNZ_END_OF_LIST_OF_FILE;
	if (file->num_file + 1 == file->gi.number_entry)
		return UNZ_END_OF_LIST_OF_FILE;

	file->pos_in_central_dir += SIZECENTRALDIRITEM + file->cur_file_info.size_filename +
	                         file->cur_file_info.size_file_extra + file->cur_file_info.size_file_comment ;
	file->num_file++;
	err = unzlocal_GetCurrentFileInfoInternal(file, &file->cur_file_info,
	        &file->cur_file_info_internal,
	        nullptr, 0, nullptr, 0, nullptr, 0);
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
extern int ZEXPORT unzLocateFile(unzFile file, const char *szFileName, int iCaseSensitivity) {
	int err;


	uLong num_fileSaved;
	uLong pos_in_central_dirSaved;


	if (file == nullptr)
		return UNZ_PARAMERROR;

	if (strlen(szFileName) >= UNZ_MAXFILENAMEINZIP)
		return UNZ_PARAMERROR;

	if (!file->current_file_ok)
		return UNZ_END_OF_LIST_OF_FILE;

	num_fileSaved = file->num_file;
	pos_in_central_dirSaved = file->pos_in_central_dir;

	err = unzGoToFirstFile(file);

	while (err == UNZ_OK) {
		char szCurrentFileName[UNZ_MAXFILENAMEINZIP + 1];
		unzGetCurrentFileInfo(file, nullptr,
		                      szCurrentFileName, sizeof(szCurrentFileName) - 1,
		                      nullptr, 0, nullptr, 0);
		if (unzStringFileNameCompare(szCurrentFileName,
		                             szFileName, iCaseSensitivity) == 0)
			return UNZ_OK;
		err = unzGoToNextFile(file);
	}

	file->num_file = num_fileSaved ;
	file->pos_in_central_dir = pos_in_central_dirSaved ;
	return err;
}


/*
  Read the local header of the current zipfile
  Check the coherency of the local header and info in the end of central
        directory about this file
  store in *piSizeVar the size of extra info in local header
        (filename and size of extra field data)
*/
static int unzlocal_CheckCurrentFileCoherencyHeader(unz_s *s,
        uInt *piSizeVar,
        uLong *poffset_local_extrafield,
        uInt *psize_local_extrafield) {
	uLong uMagic;
	uLong uData;
	uLong uFlags;
	uLong size_filename;
	uLong size_extra_field;
	int err = UNZ_OK;

	*piSizeVar = 0;
	*poffset_local_extrafield = 0;
	*psize_local_extrafield = 0;

	if (fseek(s->file, s->cur_file_info_internal.offset_curfile +
	          s->byte_before_the_zipfile, SEEK_SET) != 0)
		return UNZ_ERRNO;


	if (err == UNZ_OK) {
		if (unzlocal_getLong(s->file, &uMagic) != UNZ_OK)
			err = UNZ_ERRNO;
		else if (uMagic != 0x04034b50)
			err = UNZ_BADZIPFILE;
	}

	if (unzlocal_getShort(s->file, &uData) != UNZ_OK)
		err = UNZ_ERRNO;
	/*
	    else if ((err==UNZ_OK) && (uData!=s->cur_file_info.wVersion))
	        err=UNZ_BADZIPFILE;
	*/
	if (unzlocal_getShort(s->file, &uFlags) != UNZ_OK)
		err = UNZ_ERRNO;

	if (unzlocal_getShort(s->file, &uData) != UNZ_OK)
		err = UNZ_ERRNO;
	else if ((err == UNZ_OK) && (uData != s->cur_file_info.compression_method))
		err = UNZ_BADZIPFILE;

	if ((err == UNZ_OK) && (s->cur_file_info.compression_method != 0) &&
	        (s->cur_file_info.compression_method != Z_DEFLATED))
		err = UNZ_BADZIPFILE;

	if (unzlocal_getLong(s->file, &uData) != UNZ_OK) /* date/time */
		err = UNZ_ERRNO;

	if (unzlocal_getLong(s->file, &uData) != UNZ_OK) /* crc */
		err = UNZ_ERRNO;
	else if ((err == UNZ_OK) && (uData != s->cur_file_info.crc) &&
	         ((uFlags & 8) == 0))
		err = UNZ_BADZIPFILE;

	if (unzlocal_getLong(s->file, &uData) != UNZ_OK) /* size compr */
		err = UNZ_ERRNO;
	else if ((err == UNZ_OK) && (uData != s->cur_file_info.compressed_size) &&
	         ((uFlags & 8) == 0))
		err = UNZ_BADZIPFILE;

	if (unzlocal_getLong(s->file, &uData) != UNZ_OK) /* size uncompr */
		err = UNZ_ERRNO;
	else if ((err == UNZ_OK) && (uData != s->cur_file_info.uncompressed_size) &&
	         ((uFlags & 8) == 0))
		err = UNZ_BADZIPFILE;


	if (unzlocal_getShort(s->file, &size_filename) != UNZ_OK)
		err = UNZ_ERRNO;
	else if ((err == UNZ_OK) && (size_filename != s->cur_file_info.size_filename))
		err = UNZ_BADZIPFILE;

	*piSizeVar += size_filename;

	if (unzlocal_getShort(s->file, &size_extra_field) != UNZ_OK)
		err = UNZ_ERRNO;
	*poffset_local_extrafield = s->cur_file_info_internal.offset_curfile +
	                            SIZEZIPLOCALHEADER + size_filename;
	*psize_local_extrafield = size_extra_field;

	*piSizeVar += size_extra_field;

	return err;
}

/*
  Open for reading data the current file in the zipfile.
  If there is no error and the file is opened, the return value is UNZ_OK.
*/
extern int ZEXPORT unzOpenCurrentFile(unzFile file) {
	int err = UNZ_OK;
	bool Store;
	uInt iSizeVar;
	file_in_zip_read_info_s *pfile_in_zip_read_info;
	uLong offset_local_extrafield;  /* offset of the local extra field */
	uInt  size_local_extrafield;    /* size of the local extra field */

	if (file == nullptr)
		return UNZ_PARAMERROR;
	if (!file->current_file_ok)
		return UNZ_PARAMERROR;

	if (file->pfile_in_zip_read != nullptr)
		unzCloseCurrentFile(file);

	if (unzlocal_CheckCurrentFileCoherencyHeader(file, &iSizeVar,
	        &offset_local_extrafield, &size_local_extrafield) != UNZ_OK)
		return UNZ_BADZIPFILE;

	pfile_in_zip_read_info = new file_in_zip_read_info_s();
	if (pfile_in_zip_read_info == nullptr)
		return UNZ_INTERNALERROR;

	pfile_in_zip_read_info->read_buffer = new char[UNZ_BUFSIZE];
	pfile_in_zip_read_info->offset_local_extrafield = offset_local_extrafield;
	pfile_in_zip_read_info->size_local_extrafield = size_local_extrafield;
	pfile_in_zip_read_info->pos_local_extrafield = 0;

	if (pfile_in_zip_read_info->read_buffer == nullptr) {
		delete pfile_in_zip_read_info;
		return UNZ_INTERNALERROR;
	}

	pfile_in_zip_read_info->stream_initialised = 0;

	if ((file->cur_file_info.compression_method != 0) &&
	        (file->cur_file_info.compression_method != Z_DEFLATED))
		err = UNZ_BADZIPFILE;
	Store = file->cur_file_info.compression_method == 0;

	pfile_in_zip_read_info->crc32_wait = file->cur_file_info.crc;
	pfile_in_zip_read_info->crc32 = 0;
	pfile_in_zip_read_info->compression_method =
	    file->cur_file_info.compression_method;
	pfile_in_zip_read_info->file = file->file;
	pfile_in_zip_read_info->byte_before_the_zipfile = file->byte_before_the_zipfile;

	pfile_in_zip_read_info->stream.total_out = 0;

	if (!Store) {
	  pfile_in_zip_read_info->stream.zalloc = nullptr;
	  pfile_in_zip_read_info->stream.zfree = nullptr;
	  pfile_in_zip_read_info->stream.opaque = nullptr;

		err = inflateInit2(&pfile_in_zip_read_info->stream, -MAX_WBITS);
		if (err == Z_OK)
			pfile_in_zip_read_info->stream_initialised = 1;
		/* windowBits is passed < 0 to tell that there is no zlib header.
		 * Note that in this case inflate *requires* an extra "dummy" byte
		 * after the compressed stream in order to complete decompression and
		 * return Z_STREAM_END.
		 * In unzip, i don't wait absolutely Z_STREAM_END because I known the
		 * size of both compressed and uncompressed data
		 */
	}
	pfile_in_zip_read_info->rest_read_compressed =
	    file->cur_file_info.compressed_size ;
	pfile_in_zip_read_info->rest_read_uncompressed =
	    file->cur_file_info.uncompressed_size ;


	pfile_in_zip_read_info->pos_in_zipfile =
	    file->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER +
	    iSizeVar;

	pfile_in_zip_read_info->stream.avail_in = 0;


	file->pfile_in_zip_read = pfile_in_zip_read_info;
	return UNZ_OK;
}


/*
  Read bytes from the current file.
  buf contain buffer where data must be copied
  len the size of buf.

  return the number of byte copied if somes bytes are copied
  return 0 if the end of file was reached
  return <0 with error code if there is an error
    (UNZ_ERRNO for IO error, or zLib error for uncompress error)
*/
extern int ZEXPORT unzReadCurrentFile(unzFile file, voidp buf, unsigned len) {
	int err = UNZ_OK;
	uInt iRead = 0;
	file_in_zip_read_info_s *pfile_in_zip_read_info;
	if (file == nullptr)
		return UNZ_PARAMERROR;
	pfile_in_zip_read_info = file->pfile_in_zip_read;

	if (pfile_in_zip_read_info == nullptr)
		return UNZ_PARAMERROR;


	if (pfile_in_zip_read_info->read_buffer == nullptr)
		return UNZ_END_OF_LIST_OF_FILE;
	if (len == 0)
		return 0;

	pfile_in_zip_read_info->stream.next_out = static_cast<Bytef *>(buf);

	pfile_in_zip_read_info->stream.avail_out = len;

	if (len > pfile_in_zip_read_info->rest_read_uncompressed)
		pfile_in_zip_read_info->stream.avail_out =
		    pfile_in_zip_read_info->rest_read_uncompressed;

	while (pfile_in_zip_read_info->stream.avail_out > 0) {
		if ((pfile_in_zip_read_info->stream.avail_in == 0) &&
		        (pfile_in_zip_read_info->rest_read_compressed > 0)) {
			uInt uReadThis = UNZ_BUFSIZE;
			if (pfile_in_zip_read_info->rest_read_compressed < uReadThis)
				uReadThis = pfile_in_zip_read_info->rest_read_compressed;
			if (uReadThis == 0)
				return UNZ_EOF;
			if (fseek(pfile_in_zip_read_info->file,
			          pfile_in_zip_read_info->pos_in_zipfile +
			          pfile_in_zip_read_info->byte_before_the_zipfile, SEEK_SET) != 0)
				return UNZ_ERRNO;
			if (fread(pfile_in_zip_read_info->read_buffer, uReadThis, 1,
			          pfile_in_zip_read_info->file) != 1)
				return UNZ_ERRNO;
			pfile_in_zip_read_info->pos_in_zipfile += uReadThis;

			pfile_in_zip_read_info->rest_read_compressed -= uReadThis;

			pfile_in_zip_read_info->stream.next_in =
			    reinterpret_cast<Bytef *>(pfile_in_zip_read_info->read_buffer);
			pfile_in_zip_read_info->stream.avail_in = uReadThis;
		}

		if (pfile_in_zip_read_info->compression_method == 0) {
			uInt uDoCopy;
			uInt i ;
			if (pfile_in_zip_read_info->stream.avail_out <
			        pfile_in_zip_read_info->stream.avail_in)
				uDoCopy = pfile_in_zip_read_info->stream.avail_out ;
			else
				uDoCopy = pfile_in_zip_read_info->stream.avail_in ;

			for (i = 0; i < uDoCopy; i++)
				*(pfile_in_zip_read_info->stream.next_out + i) =
				    *(pfile_in_zip_read_info->stream.next_in + i);

			pfile_in_zip_read_info->crc32 = crc32(pfile_in_zip_read_info->crc32,
			                                      pfile_in_zip_read_info->stream.next_out,
			                                      uDoCopy);
			pfile_in_zip_read_info->rest_read_uncompressed -= uDoCopy;
			pfile_in_zip_read_info->stream.avail_in -= uDoCopy;
			pfile_in_zip_read_info->stream.avail_out -= uDoCopy;
			pfile_in_zip_read_info->stream.next_out += uDoCopy;
			pfile_in_zip_read_info->stream.next_in += uDoCopy;
			pfile_in_zip_read_info->stream.total_out += uDoCopy;
			iRead += uDoCopy;
		} else {
			uLong uTotalOutBefore;
			uLong uTotalOutAfter;
			const Bytef *bufBefore;
			uLong uOutThis;
			int flush = Z_SYNC_FLUSH;

			uTotalOutBefore = pfile_in_zip_read_info->stream.total_out;
			bufBefore = pfile_in_zip_read_info->stream.next_out;

			/*
			if ((pfile_in_zip_read_info->rest_read_uncompressed ==
			         pfile_in_zip_read_info->stream.avail_out) &&
			    (pfile_in_zip_read_info->rest_read_compressed == 0))
			    flush = Z_FINISH;
			*/
			err = inflate(&pfile_in_zip_read_info->stream, flush);

			uTotalOutAfter = pfile_in_zip_read_info->stream.total_out;
			uOutThis = uTotalOutAfter - uTotalOutBefore;

			pfile_in_zip_read_info->crc32 =
			    crc32(pfile_in_zip_read_info->crc32, bufBefore,
			          (uOutThis));

			pfile_in_zip_read_info->rest_read_uncompressed -=
			    uOutThis;

			iRead += (uTotalOutAfter - uTotalOutBefore);

			if (err == Z_STREAM_END)
				return (iRead == 0) ? UNZ_EOF : iRead;
			if (err != Z_OK)
				break;
		}
	}

	if (err == Z_OK)
		return iRead;
	return err;
}


/*
  Give the current position in uncompressed data
*/
extern z_off_t ZEXPORT unztell(unzFile file) {
	file_in_zip_read_info_s *pfile_in_zip_read_info;
	if (file == nullptr)
		return UNZ_PARAMERROR;
	pfile_in_zip_read_info = file->pfile_in_zip_read;

	if (pfile_in_zip_read_info == nullptr)
		return UNZ_PARAMERROR;

	return static_cast<z_off_t>(pfile_in_zip_read_info->stream.total_out);
}


/*
  return 1 if the end of file was reached, 0 elsewhere
*/
extern int ZEXPORT unzeof(unzFile file) {
	file_in_zip_read_info_s *pfile_in_zip_read_info;
	if (file == nullptr)
		return UNZ_PARAMERROR;
	pfile_in_zip_read_info = file->pfile_in_zip_read;

	if (pfile_in_zip_read_info == nullptr)
		return UNZ_PARAMERROR;

	if (pfile_in_zip_read_info->rest_read_uncompressed == 0)
		return 1;
	else
		return 0;
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
extern int ZEXPORT unzGetLocalExtrafield(unzFile file, voidp buf, unsigned len) {
	file_in_zip_read_info_s *pfile_in_zip_read_info;
	uInt read_now;
	uLong size_to_read;

	if (file == nullptr)
		return UNZ_PARAMERROR;
	pfile_in_zip_read_info = file->pfile_in_zip_read;

	if (pfile_in_zip_read_info == nullptr)
		return UNZ_PARAMERROR;

	size_to_read = (pfile_in_zip_read_info->size_local_extrafield -
	                pfile_in_zip_read_info->pos_local_extrafield);

	if (buf == nullptr)
		return size_to_read;

	if (len > size_to_read)
		read_now = size_to_read;
	else
		read_now = len ;

	if (read_now == 0)
		return 0;

	if (fseek(pfile_in_zip_read_info->file,
	          pfile_in_zip_read_info->offset_local_extrafield +
	          pfile_in_zip_read_info->pos_local_extrafield, SEEK_SET) != 0)
		return UNZ_ERRNO;

	if (fread(buf, size_to_read, 1, pfile_in_zip_read_info->file) != 1)
		return UNZ_ERRNO;

	return read_now;
}

/*
  Close the file in zip opened with unzipOpenCurrentFile
  Return UNZ_CRCERROR if all the file was read but the CRC is not good
*/
extern int ZEXPORT unzCloseCurrentFile(unzFile file) {
	int err = UNZ_OK;

	file_in_zip_read_info_s *pfile_in_zip_read_info;
	if (file == nullptr)
		return UNZ_PARAMERROR;
	pfile_in_zip_read_info = file->pfile_in_zip_read;

	if (pfile_in_zip_read_info == nullptr)
		return UNZ_PARAMERROR;


	if (pfile_in_zip_read_info->rest_read_uncompressed == 0) {
		if (pfile_in_zip_read_info->crc32 != pfile_in_zip_read_info->crc32_wait)
			err = UNZ_CRCERROR;
	}


	delete [] pfile_in_zip_read_info->read_buffer;
	pfile_in_zip_read_info->read_buffer = nullptr;
	if (pfile_in_zip_read_info->stream_initialised)
		inflateEnd(&pfile_in_zip_read_info->stream);

	pfile_in_zip_read_info->stream_initialised = 0;
	delete pfile_in_zip_read_info;

	file->pfile_in_zip_read = nullptr;

	return err;
}


/*
  Get the global comment string of the ZipFile, in the szComment buffer.
  uSizeBuf is the size of the szComment buffer.
  return the number of byte copied or an error code <0
*/
extern int ZEXPORT unzGetGlobalComment(unzFile file, char *szComment, uLong uSizeBuf) {
	uLong uReadThis ;
	if (file == nullptr)
		return UNZ_PARAMERROR;

	uReadThis = uSizeBuf;
	if (uReadThis > file->gi.size_comment)
		uReadThis = file->gi.size_comment;

	if (fseek(file->file, file->central_pos + 22, SEEK_SET) != 0)
		return UNZ_ERRNO;

	if (uReadThis > 0) {
		*szComment = '\0';
		if (fread(szComment, uReadThis, 1, file->file) != 1)
			return UNZ_ERRNO;
	}

	if ((szComment != nullptr) && (uSizeBuf > file->gi.size_comment))
		*(szComment + file->gi.size_comment) = '\0';
	return uReadThis;
}

/* Added by Ryan Nunn */
#endif /*HAVE_ZIP_SUPPORT*/
