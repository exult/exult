/* zip.cc -- IO on .zip files using zlib
   Version 0.15 beta, Mar 19th, 1998,

   Modified by Ryan Nunn. Nov 9th 2001

   Read zip.h for more info
*/

/* Added by Ryan Nunn */
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "ignore_unused_variable_warning.h"

#ifdef HAVE_ZIP_SUPPORT

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
#include "zip.h"

/* Added by Ryan Nunn to overcome DEF_MEM_LEVEL being undeclared */
#if MAX_MEM_LEVEL >= 8
#  define DEF_MEM_LEVEL 8
#else
#  define DEF_MEM_LEVEL  MAX_MEM_LEVEL
#endif

#ifndef VERSIONMADEBY
# define VERSIONMADEBY   (0x0) /* platform depedent */
#endif

#ifndef Z_BUFSIZE
#define Z_BUFSIZE (16384)
#endif

#ifndef Z_MAXFILENAMEINZIP
#define Z_MAXFILENAMEINZIP (256)
#endif

/*
#define SIZECENTRALDIRITEM (0x2e)
#define SIZEZIPLOCALHEADER (0x1e)
*/

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

//const char zip_copyright[] = " zip 0.15 Copyright 1998 Gilles Vollant ";


#define SIZEDATA_INDATABLOCK (4096-(4*4))

#define LOCALHEADERMAGIC    (0x04034b50)
#define CENTRALHEADERMAGIC  (0x02014b50)
#define ENDHEADERMAGIC      (0x06054b50)

#define FLAG_LOCALHEADER_OFFSET (0x06)
#define CRC_LOCALHEADER_OFFSET  (0x0e)

#define SIZECENTRALHEADER (0x2e) /* 46 */

struct linkedlist_datablock_internal {
	struct linkedlist_datablock_internal *next_datablock;
	uLong  avail_in_this_block;
	uLong  filled_in_this_block;
	uLong  unused; /* for future use and alignement */
	unsigned char data[SIZEDATA_INDATABLOCK];
};

struct linkedlist_data {
	linkedlist_datablock_internal *first_block;
	linkedlist_datablock_internal *last_block;
};

struct curfile_info {
	z_stream stream;            /* zLib stream structure for inflate */
	int  stream_initialised;    /* 1 is stream is initialised */
	uInt pos_in_buffered_data;  /* last written byte in buffered_data */

	uLong pos_local_header;     /* offset of the local header of the file
                                     currenty writing */
	char *central_header;       /* central header data for the current file */
	uLong size_centralheader;   /* size of the central header for cur file */
	uLong flag;                 /* flag of the file currently writing */

	int  method;                /* compression method of file currenty wr.*/
	Byte buffered_data[Z_BUFSIZE];/* buffer contain compressed data to be writ*/
	uLong dosDate;
	uLong crc32;
};

struct zip_internal {
	FILE *filezip;
	linkedlist_data central_dir;/* datablock with central dir in construction*/
	int  in_opened_file_inzip;  /* 1 if a file in the zip is currently writ.*/
	curfile_info ci;            /* info on the file curretly writing */

	uLong begin_pos;            /* position of the beginning of the zipfile */
	uLong number_entry;
};

static linkedlist_datablock_internal *allocate_new_datablock() {
	linkedlist_datablock_internal *ldi;
	ldi = new linkedlist_datablock_internal();
	if (ldi != nullptr) {
		ldi->next_datablock = nullptr ;
		ldi->filled_in_this_block = 0 ;
		ldi->avail_in_this_block = SIZEDATA_INDATABLOCK ;
	}
	return ldi;
}

static void free_datablock(linkedlist_datablock_internal *ldi) {
	while (ldi != nullptr) {
		linkedlist_datablock_internal *ldinext = ldi->next_datablock;
		delete ldi;
		ldi = ldinext;
	}
}

static void init_linkedlist(linkedlist_data *ll) {
	ll->first_block = ll->last_block = nullptr;
}

static int add_data_in_datablock(linkedlist_data *ll, const void *buf, uLong len) {
	linkedlist_datablock_internal *ldi;
	const unsigned char *from_copy;

	if (ll == nullptr)
		return ZIP_INTERNALERROR;

	if (ll->last_block == nullptr) {
		ll->first_block = ll->last_block = allocate_new_datablock();
		if (ll->first_block == nullptr)
			return ZIP_INTERNALERROR;
	}

	ldi = ll->last_block;
	from_copy = static_cast<const unsigned char *>(buf);

	while (len > 0) {
		uInt copy_this;
		uInt i;
		unsigned char *to_copy;

		if (ldi->avail_in_this_block == 0) {
			ldi->next_datablock = allocate_new_datablock();
			if (ldi->next_datablock == nullptr)
				return ZIP_INTERNALERROR;
			ldi = ldi->next_datablock ;
			ll->last_block = ldi;
		}

		if (ldi->avail_in_this_block < len)
			copy_this = ldi->avail_in_this_block;
		else
			copy_this = len;

		to_copy = &(ldi->data[ldi->filled_in_this_block]);

		for (i = 0; i < copy_this; i++)
			*(to_copy + i) = *(from_copy + i);

		ldi->filled_in_this_block += copy_this;
		ldi->avail_in_this_block -= copy_this;
		from_copy += copy_this ;
		len -= copy_this;
	}
	return ZIP_OK;
}

/****************************************************************************/

/* ===========================================================================
   Outputs a long in LSB order to the given file
   nbByte == 1, 2 or 4 (byte, short or long)
*/

static int ziplocal_putValue(FILE *file, uLong x, int nbByte);
static int ziplocal_putValue(FILE *file, uLong x, int nbByte) {
	unsigned char buf[4];
	int n;
	for (n = 0; n < nbByte; n++) {
		buf[n] = static_cast<unsigned char>(x & 0xff);
		x >>= 8;
	}
	if (fwrite(buf, nbByte, 1, file) != 1)
		return ZIP_ERRNO;
	else
		return ZIP_OK;
}

static void ziplocal_putValue_inmemory(void *dest, uLong x, int nbByte);
static void ziplocal_putValue_inmemory(void *dest, uLong x, int nbByte) {
	unsigned char *buf = static_cast<unsigned char *>(dest);
	int n;
	for (n = 0; n < nbByte; n++) {
		buf[n] = static_cast<unsigned char>(x & 0xff);
		x >>= 8;
	}
}
/****************************************************************************/


static uLong ziplocal_TmzDateToDosDate(const tm_zip *ptm, uLong uLongdosDate) {
	ignore_unused_variable_warning(uLongdosDate);
	uLong year = ptm->tm_year;
	if (year > 1980)
		year -= 1980;
	else if (year > 80)
		year -= 80;
	return
	    (((ptm->tm_mday) + (32 * (ptm->tm_mon + 1)) + (512 * year)) << 16) |
	    ((ptm->tm_sec / 2) + (32 * ptm->tm_min) + (2048 * ptm->tm_hour));
}


/****************************************************************************/

extern zipFile ZEXPORT zipOpen(const char *pathname, int append) {
	zip_internal ziinit;
	zip_internal *zi;

	ziinit.filezip = nullptr;

	/* Start changes by Ryan Nunn to fix append mode bug */

	/* If append, use r+b mode, will fail if not exist */
	if (append != 0)
		ziinit.filezip = fopen(pathname, "r+b");

	/* If not append, or failed, use wb */
	if (ziinit.filezip == nullptr)
		ziinit.filezip = fopen(pathname, "wb");

	/* Still doesn't exist, means can't create */
	if (ziinit.filezip == nullptr)
		return nullptr;

	/* Make sure we are at the end of the file */
	fseek(ziinit.filezip, 0, SEEK_END);

	/* End changes by Ryan Nunn to fix append mode bug */

	ziinit.begin_pos = ftell(ziinit.filezip);
	ziinit.in_opened_file_inzip = 0;
	ziinit.ci.stream_initialised = 0;
	ziinit.number_entry = 0;
	init_linkedlist(&(ziinit.central_dir));


	zi = new zip_internal(ziinit);
	return zi;
}

extern int ZEXPORT zipOpenNewFileInZip(zipFile file,
                                       const char *filename,
                                       const zip_fileinfo *zipfi,
                                       const void *extrafield_local,
                                       uInt size_extrafield_local,
                                       const void *extrafield_global,
                                       uInt size_extrafield_global,
                                       const char *comment,
                                       int method,
                                       int level) {
	uInt size_filename;
	uInt size_comment;
	uInt i;
	int err = ZIP_OK;

	if (file == nullptr)
		return ZIP_PARAMERROR;
	if ((method != 0) && (method != Z_DEFLATED))
		return ZIP_PARAMERROR;

	if (file->in_opened_file_inzip == 1) {
		err = zipCloseFileInZip(file);
		if (err != ZIP_OK)
			return err;
	}


	if (filename == nullptr)
		filename = "-";

	if (comment == nullptr)
		size_comment = 0;
	else
		size_comment = strlen(comment);

	size_filename = strlen(filename);

	if (zipfi == nullptr)
		file->ci.dosDate = 0;
	else {
		if (zipfi->dosDate != 0)
			file->ci.dosDate = zipfi->dosDate;
		else file->ci.dosDate = ziplocal_TmzDateToDosDate(&zipfi->tmz_date, zipfi->dosDate);
	}

	file->ci.flag = 0;
	if ((level == 8) || (level == 9))
		file->ci.flag |= 2;
	if (level == 2)
		file->ci.flag |= 4;
	if (level == 1)
		file->ci.flag |= 6;

	file->ci.crc32 = 0;
	file->ci.method = method;
	file->ci.stream_initialised = 0;
	file->ci.pos_in_buffered_data = 0;
	file->ci.pos_local_header = ftell(file->filezip);
	file->ci.size_centralheader = SIZECENTRALHEADER + size_filename +
	                            size_extrafield_global + size_comment;
	file->ci.central_header = new char[file->ci.size_centralheader];

	ziplocal_putValue_inmemory(file->ci.central_header, CENTRALHEADERMAGIC, 4);
	/* version info */
	ziplocal_putValue_inmemory(file->ci.central_header + 4, VERSIONMADEBY, 2);
	ziplocal_putValue_inmemory(file->ci.central_header + 6, 20, 2);
	ziplocal_putValue_inmemory(file->ci.central_header + 8, file->ci.flag, 2);
	ziplocal_putValue_inmemory(file->ci.central_header + 10, file->ci.method, 2);
	ziplocal_putValue_inmemory(file->ci.central_header + 12, file->ci.dosDate, 4);
	ziplocal_putValue_inmemory(file->ci.central_header + 16, 0, 4); /*crc*/
	ziplocal_putValue_inmemory(file->ci.central_header + 20, 0, 4); /*compr size*/
	ziplocal_putValue_inmemory(file->ci.central_header + 24, 0, 4); /*uncompr size*/
	ziplocal_putValue_inmemory(file->ci.central_header + 28, size_filename, 2);
	ziplocal_putValue_inmemory(file->ci.central_header + 30, size_extrafield_global, 2);
	ziplocal_putValue_inmemory(file->ci.central_header + 32, size_comment, 2);
	ziplocal_putValue_inmemory(file->ci.central_header + 34, 0, 2); /*disk nm start*/

	if (zipfi == nullptr)
		ziplocal_putValue_inmemory(file->ci.central_header + 36, 0, 2);
	else
		ziplocal_putValue_inmemory(file->ci.central_header + 36, zipfi->internal_fa, 2);

	if (zipfi == nullptr)
		ziplocal_putValue_inmemory(file->ci.central_header + 38, 0, 4);
	else
		ziplocal_putValue_inmemory(file->ci.central_header + 38, zipfi->external_fa, 4);

	ziplocal_putValue_inmemory(file->ci.central_header + 42, file->ci.pos_local_header, 4);

	for (i = 0; i < size_filename; i++)
		*(file->ci.central_header + SIZECENTRALHEADER + i) = *(filename + i);

	for (i = 0; i < size_extrafield_global; i++)
		*(file->ci.central_header + SIZECENTRALHEADER + size_filename + i) =
		    *(static_cast<const char *>(extrafield_global) + i);

	for (i = 0; i < size_comment; i++)
		*(file->ci.central_header + SIZECENTRALHEADER + size_filename +
		  size_extrafield_global + i) = *(filename + i);
	if (file->ci.central_header == nullptr)
		return ZIP_INTERNALERROR;

	/* write the local header */
	err = ziplocal_putValue(file->filezip, LOCALHEADERMAGIC, 4);

	if (err == ZIP_OK)
		err = ziplocal_putValue(file->filezip, 20, 2); /* version needed to extract */
	if (err == ZIP_OK)
		err = ziplocal_putValue(file->filezip, file->ci.flag, 2);

	if (err == ZIP_OK)
		err = ziplocal_putValue(file->filezip, file->ci.method, 2);

	if (err == ZIP_OK)
		err = ziplocal_putValue(file->filezip, file->ci.dosDate, 4);

	if (err == ZIP_OK)
		err = ziplocal_putValue(file->filezip, 0, 4); /* crc 32, unknown */
	if (err == ZIP_OK)
		err = ziplocal_putValue(file->filezip, 0, 4); /* compressed size, unknown */
	if (err == ZIP_OK)
		err = ziplocal_putValue(file->filezip, 0, 4); /* uncompressed size, unknown */

	if (err == ZIP_OK)
		err = ziplocal_putValue(file->filezip, size_filename, 2);

	if (err == ZIP_OK)
		err = ziplocal_putValue(file->filezip, size_extrafield_local, 2);

	if ((err == ZIP_OK) && (size_filename > 0))
		if (fwrite(filename, size_filename, 1, file->filezip) != 1)
			err = ZIP_ERRNO;

	if ((err == ZIP_OK) && (size_extrafield_local > 0))
		if (fwrite(extrafield_local, size_extrafield_local, 1, file->filezip)
		        != 1)
			err = ZIP_ERRNO;

	file->ci.stream.avail_in = 0;
	file->ci.stream.avail_out = Z_BUFSIZE;
	file->ci.stream.next_out = file->ci.buffered_data;
	file->ci.stream.total_in = 0;
	file->ci.stream.total_out = 0;

	if ((err == ZIP_OK) && (file->ci.method == Z_DEFLATED)) {
		file->ci.stream.zalloc = nullptr;
		file->ci.stream.zfree = nullptr;
		file->ci.stream.opaque = nullptr;

		err = deflateInit2(&file->ci.stream, level,
		                   Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, 0);

		if (err == Z_OK)
			file->ci.stream_initialised = 1;
	}


	if (err == Z_OK)
		file->in_opened_file_inzip = 1;
	return err;
}

extern int ZEXPORT zipWriteInFileInZip(zipFile file, const voidp buf, unsigned len) {
	int err = ZIP_OK;

	if (file == nullptr)
		return ZIP_PARAMERROR;

	if (file->in_opened_file_inzip == 0)
		return ZIP_PARAMERROR;

	file->ci.stream.next_in = static_cast<Bytef *>(buf);
	file->ci.stream.avail_in = len;
	file->ci.crc32 = crc32(file->ci.crc32, static_cast<Bytef *>(buf), len);

	while ((err == ZIP_OK) && (file->ci.stream.avail_in > 0)) {
		if (file->ci.stream.avail_out == 0) {
			if (fwrite(file->ci.buffered_data, file->ci.pos_in_buffered_data, 1, file->filezip)
			        != 1)
				err = ZIP_ERRNO;
			file->ci.pos_in_buffered_data = 0;
			file->ci.stream.avail_out = Z_BUFSIZE;
			file->ci.stream.next_out = file->ci.buffered_data;
		}

		if (file->ci.method == Z_DEFLATED) {
			uLong uTotalOutBefore = file->ci.stream.total_out;
			err = deflate(&file->ci.stream,  Z_NO_FLUSH);
			file->ci.pos_in_buffered_data += (file->ci.stream.total_out - uTotalOutBefore) ;

		} else {
			uInt copy_this;
			uInt i;
			if (file->ci.stream.avail_in < file->ci.stream.avail_out)
				copy_this = file->ci.stream.avail_in;
			else
				copy_this = file->ci.stream.avail_out;
			for (i = 0; i < copy_this; i++)
				*(reinterpret_cast<char *>(file->ci.stream.next_out) + i) =
				    *(reinterpret_cast<const char *>(file->ci.stream.next_in) + i);
			{
				file->ci.stream.avail_in -= copy_this;
				file->ci.stream.avail_out -= copy_this;
				file->ci.stream.next_in += copy_this;
				file->ci.stream.next_out += copy_this;
				file->ci.stream.total_in += copy_this;
				file->ci.stream.total_out += copy_this;
				file->ci.pos_in_buffered_data += copy_this;
			}
		}
	}

	return 0;
}

extern int ZEXPORT zipCloseFileInZip(zipFile file) {
	int err = ZIP_OK;

	if (file == nullptr)
		return ZIP_PARAMERROR;

	if (file->in_opened_file_inzip == 0)
		return ZIP_PARAMERROR;
	file->ci.stream.avail_in = 0;

	if (file->ci.method == Z_DEFLATED)
		while (err == ZIP_OK) {
			uLong uTotalOutBefore;
			if (file->ci.stream.avail_out == 0) {
				if (fwrite(file->ci.buffered_data, file->ci.pos_in_buffered_data, 1, file->filezip)
				        != 1)
					err = ZIP_ERRNO;
				file->ci.pos_in_buffered_data = 0;
				file->ci.stream.avail_out = Z_BUFSIZE;
				file->ci.stream.next_out = file->ci.buffered_data;
			}
			uTotalOutBefore = file->ci.stream.total_out;
			err = deflate(&file->ci.stream,  Z_FINISH);
			file->ci.pos_in_buffered_data += (file->ci.stream.total_out - uTotalOutBefore) ;
		}

	if (err == Z_STREAM_END)
		err = ZIP_OK; /* this is normal */

	if ((file->ci.pos_in_buffered_data > 0) && (err == ZIP_OK))
		if (fwrite(file->ci.buffered_data, file->ci.pos_in_buffered_data, 1, file->filezip)
		        != 1)
			err = ZIP_ERRNO;

	if ((file->ci.method == Z_DEFLATED) && (err == ZIP_OK)) {
		err = deflateEnd(&file->ci.stream);
		file->ci.stream_initialised = 0;
	}

	ziplocal_putValue_inmemory(file->ci.central_header + 16, file->ci.crc32, 4); /*crc*/
	ziplocal_putValue_inmemory(file->ci.central_header + 20,
	                           file->ci.stream.total_out, 4); /*compr size*/
	ziplocal_putValue_inmemory(file->ci.central_header + 24,
	                           file->ci.stream.total_in, 4); /*uncompr size*/

	if (err == ZIP_OK)
		err = add_data_in_datablock(&file->central_dir, file->ci.central_header,
		                            file->ci.size_centralheader);
	delete [] file->ci.central_header;

	if (err == ZIP_OK) {
		long cur_pos_inzip = ftell(file->filezip);
		if (fseek(file->filezip,
		          file->ci.pos_local_header + 14, SEEK_SET) != 0)
			err = ZIP_ERRNO;

		if (err == ZIP_OK)
			err = ziplocal_putValue(file->filezip, file->ci.crc32, 4); /* crc 32, unknown */

		if (err == ZIP_OK) /* compressed size, unknown */
			err = ziplocal_putValue(file->filezip, file->ci.stream.total_out, 4);

		if (err == ZIP_OK) /* uncompressed size, unknown */
			err = ziplocal_putValue(file->filezip, file->ci.stream.total_in, 4);

		if (fseek(file->filezip,
		          cur_pos_inzip, SEEK_SET) != 0)
			err = ZIP_ERRNO;
	}

	file->number_entry ++;
	file->in_opened_file_inzip = 0;

	return err;
}

extern int ZEXPORT zipClose(zipFile file, const char *global_comment) {
	int err = 0;
	uLong size_centraldir = 0;
	uLong centraldir_pos_inzip ;
	uInt size_global_comment;
	if (file == nullptr)
		return ZIP_PARAMERROR;

	if (file->in_opened_file_inzip == 1) {
		err = zipCloseFileInZip(file);
	}

	if (global_comment == nullptr)
		size_global_comment = 0;
	else
		size_global_comment = strlen(global_comment);


	centraldir_pos_inzip = ftell(file->filezip);
	if (err == ZIP_OK) {
		linkedlist_datablock_internal *ldi = file->central_dir.first_block ;
		while (ldi != nullptr) {
			if ((err == ZIP_OK) && (ldi->filled_in_this_block > 0))
				if (fwrite(ldi->data, ldi->filled_in_this_block,
				           1, file->filezip) != 1)
					err = ZIP_ERRNO;

			size_centraldir += ldi->filled_in_this_block;
			ldi = ldi->next_datablock;
		}
	}
	free_datablock(file->central_dir.first_block);

	if (err == ZIP_OK) /* Magic End */
		err = ziplocal_putValue(file->filezip, ENDHEADERMAGIC, 4);

	if (err == ZIP_OK) /* number of this disk */
		err = ziplocal_putValue(file->filezip, 0, 2);

	if (err == ZIP_OK) /* number of the disk with the start of the central directory */
		err = ziplocal_putValue(file->filezip, 0, 2);

	if (err == ZIP_OK) /* total number of entries in the central dir on this disk */
		err = ziplocal_putValue(file->filezip, file->number_entry, 2);

	if (err == ZIP_OK) /* total number of entries in the central dir */
		err = ziplocal_putValue(file->filezip, file->number_entry, 2);

	if (err == ZIP_OK) /* size of the central directory */
		err = ziplocal_putValue(file->filezip, size_centraldir, 4);

	if (err == ZIP_OK) /* offset of start of central directory with respect to the
                            starting disk number */
		err = ziplocal_putValue(file->filezip, centraldir_pos_inzip , 4);

	if (err == ZIP_OK) /* zipfile comment length */
		err = ziplocal_putValue(file->filezip, size_global_comment, 2);

	if ((err == ZIP_OK) && (size_global_comment > 0))
		if (fwrite(global_comment, size_global_comment, 1, file->filezip) != 1)
			err = ZIP_ERRNO;
	fclose(file->filezip);
	delete file;

	return err;
}

/* Added by Ryan Nunn */
#endif /*HAVE_ZIP_SUPPORT*/
