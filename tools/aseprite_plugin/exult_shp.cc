/*
 *  exult_shape.cc - Aseprite plugin converter for SHP files
 *
 *  Copyright (C) 2025  The Exult Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "U7obj.h"
#include "databuf.h"
#include "ibuf8.h"
#include "ignore_unused_variable_warning.h"
#include "vgafile.h"

#include <fcntl.h>
#include <png.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using Palette_t = std::array<unsigned char, 768>;

namespace {
	// Default Exult palette - copied from u7shp-old.cc
	Palette_t DefaultPalette
			= {0x00, 0x00, 0x00, 0xF8, 0xF0, 0xCC, 0xF4, 0xE4, 0xA4, 0xF0, 0xDC,
			   0x78, 0xEC, 0xD0, 0x50, 0xEC, 0xC8, 0x28, 0xD8, 0xAC, 0x20, 0xC4,
			   0x94, 0x18, 0xB0, 0x80, 0x10, 0x9C, 0x68, 0x0C, 0x88, 0x54, 0x08,
			   0x74, 0x44, 0x04, 0x60, 0x30, 0x00, 0x4C, 0x24, 0x00, 0x38, 0x14,
			   0x00, 0xF8, 0xFC, 0xFC, 0xFC, 0xD8, 0xD8, 0xFC, 0xB8, 0xB8, 0xFC,
			   0x98, 0x9C, 0xFC, 0x78, 0x80, 0xFC, 0x58, 0x64, 0xFC, 0x38, 0x4C,
			   0xFC, 0x1C, 0x34, 0xDC, 0x14, 0x28, 0xC0, 0x0C, 0x1C, 0xA4, 0x08,
			   0x14, 0x88, 0x04, 0x0C, 0x6C, 0x00, 0x04, 0x50, 0x00, 0x00, 0x34,
			   0x00, 0x00, 0x18, 0x00, 0x00, 0xFC, 0xEC, 0xD8, 0xFC, 0xDC, 0xB8,
			   0xFC, 0xCC, 0x98, 0xFC, 0xBC, 0x7C, 0xFC, 0xAC, 0x5C, 0xFC, 0x9C,
			   0x3C, 0xFC, 0x8C, 0x1C, 0xFC, 0x7C, 0x00, 0xE0, 0x6C, 0x00, 0xC0,
			   0x60, 0x00, 0xA4, 0x50, 0x00, 0x88, 0x44, 0x00, 0x6C, 0x34, 0x00,
			   0x50, 0x24, 0x00, 0x34, 0x18, 0x00, 0x18, 0x08, 0x00, 0xFC, 0xFC,
			   0xD8, 0xF4, 0xF4, 0x9C, 0xEC, 0xEC, 0x60, 0xE4, 0xE4, 0x2C, 0xDC,
			   0xDC, 0x00, 0xC0, 0xC0, 0x00, 0xA4, 0xA4, 0x00, 0x88, 0x88, 0x00,
			   0x6C, 0x6C, 0x00, 0x50, 0x50, 0x00, 0x34, 0x34, 0x00, 0x18, 0x18,
			   0x00, 0xD8, 0xFC, 0xD8, 0xB0, 0xFC, 0xAC, 0x8C, 0xFC, 0x80, 0x6C,
			   0xFC, 0x54, 0x50, 0xFC, 0x28, 0x38, 0xFC, 0x00, 0x28, 0xDC, 0x00,
			   0x1C, 0xC0, 0x00, 0x14, 0xA4, 0x00, 0x0C, 0x88, 0x00, 0x04, 0x6C,
			   0x00, 0x00, 0x50, 0x00, 0x00, 0x34, 0x00, 0x00, 0x18, 0x00, 0xD4,
			   0xD8, 0xFC, 0xB8, 0xB8, 0xFC, 0x98, 0x98, 0xFC, 0x7C, 0x7C, 0xFC,
			   0x5C, 0x5C, 0xFC, 0x3C, 0x3C, 0xFC, 0x00, 0x00, 0xFC, 0x00, 0x00,
			   0xE0, 0x00, 0x00, 0xC0, 0x00, 0x00, 0xA4, 0x00, 0x00, 0x88, 0x00,
			   0x00, 0x6C, 0x00, 0x00, 0x50, 0x00, 0x00, 0x34, 0x00, 0x00, 0x18,
			   0xE8, 0xC8, 0xE8, 0xD4, 0x98, 0xD4, 0xC4, 0x6C, 0xC4, 0xB0, 0x48,
			   0xB0, 0xA0, 0x28, 0xA0, 0x8C, 0x10, 0x8C, 0x7C, 0x00, 0x7C, 0x6C,
			   0x00, 0x6C, 0x60, 0x00, 0x60, 0x50, 0x00, 0x50, 0x44, 0x00, 0x44,
			   0x34, 0x00, 0x34, 0x24, 0x00, 0x24, 0x18, 0x00, 0x18, 0xF4, 0xE8,
			   0xE4, 0xEC, 0xDC, 0xD4, 0xE4, 0xCC, 0xC0, 0xE0, 0xC0, 0xB0, 0xD8,
			   0xB0, 0xA0, 0xD0, 0xA4, 0x90, 0xC8, 0x98, 0x80, 0xC4, 0x8C, 0x74,
			   0xAC, 0x7C, 0x64, 0x98, 0x6C, 0x58, 0x80, 0x5C, 0x4C, 0x6C, 0x4C,
			   0x3C, 0x54, 0x3C, 0x30, 0x3C, 0x2C, 0x24, 0x28, 0x1C, 0x14, 0x10,
			   0x0C, 0x08, 0xEC, 0xEC, 0xEC, 0xDC, 0xDC, 0xDC, 0xCC, 0xCC, 0xCC,
			   0xBC, 0xBC, 0xBC, 0xAC, 0xAC, 0xAC, 0x9C, 0x9C, 0x9C, 0x8C, 0x8C,
			   0x8C, 0x7C, 0x7C, 0x7C, 0x6C, 0x6C, 0x6C, 0x60, 0x60, 0x60, 0x50,
			   0x50, 0x50, 0x44, 0x44, 0x44, 0x34, 0x34, 0x34, 0x24, 0x24, 0x24,
			   0x18, 0x18, 0x18, 0x08, 0x08, 0x08, 0xE8, 0xE0, 0xD4, 0xD8, 0xC8,
			   0xB0, 0xC8, 0xB0, 0x90, 0xB8, 0x98, 0x70, 0xA8, 0x84, 0x58, 0x98,
			   0x70, 0x40, 0x88, 0x5C, 0x2C, 0x7C, 0x4C, 0x18, 0x6C, 0x3C, 0x0C,
			   0x5C, 0x34, 0x0C, 0x4C, 0x2C, 0x0C, 0x3C, 0x24, 0x0C, 0x2C, 0x1C,
			   0x08, 0x20, 0x14, 0x08, 0xEC, 0xE8, 0xE4, 0xDC, 0xD4, 0xD0, 0xCC,
			   0xC4, 0xBC, 0xBC, 0xB0, 0xAC, 0xAC, 0xA0, 0x98, 0x9C, 0x90, 0x88,
			   0x8C, 0x80, 0x78, 0x7C, 0x70, 0x68, 0x6C, 0x60, 0x5C, 0x60, 0x54,
			   0x50, 0x50, 0x48, 0x44, 0x44, 0x3C, 0x38, 0x34, 0x30, 0x2C, 0x24,
			   0x20, 0x20, 0x18, 0x14, 0x14, 0xE0, 0xE8, 0xD4, 0xC8, 0xD4, 0xB4,
			   0xB4, 0xC0, 0x98, 0x9C, 0xAC, 0x7C, 0x88, 0x98, 0x60, 0x70, 0x84,
			   0x4C, 0x5C, 0x70, 0x38, 0x4C, 0x5C, 0x28, 0x40, 0x50, 0x20, 0x38,
			   0x44, 0x1C, 0x30, 0x3C, 0x18, 0x28, 0x30, 0x14, 0x20, 0x24, 0x10,
			   0x18, 0x1C, 0x08, 0x0C, 0x10, 0x04, 0xEC, 0xD8, 0xCC, 0xDC, 0xB8,
			   0xA0, 0xCC, 0x98, 0x7C, 0xBC, 0x80, 0x5C, 0xAC, 0x64, 0x3C, 0x9C,
			   0x50, 0x24, 0x8C, 0x3C, 0x0C, 0x7C, 0x2C, 0x00, 0x6C, 0x24, 0x00,
			   0x60, 0x20, 0x00, 0x50, 0x1C, 0x00, 0x44, 0x14, 0x00, 0x34, 0x10,
			   0x00, 0x24, 0x0C, 0x00, 0xF0, 0xF0, 0xFC, 0xE4, 0xE4, 0xFC, 0xD8,
			   0xD8, 0xFC, 0xCC, 0xCC, 0xFC, 0xC0, 0xC0, 0xFC, 0xB4, 0xB4, 0xFC,
			   0xA8, 0xA8, 0xFC, 0x9C, 0x9C, 0xFC, 0x84, 0xD0, 0x00, 0x84, 0xB0,
			   0x00, 0x7C, 0x94, 0x00, 0x68, 0x78, 0x00, 0x50, 0x58, 0x00, 0x3C,
			   0x40, 0x00, 0x2C, 0x24, 0x00, 0x1C, 0x08, 0x00, 0x20, 0x00, 0x00,
			   0xEC, 0xD8, 0xC4, 0xDC, 0xC0, 0xB4, 0xCC, 0xB4, 0xA0, 0xBC, 0x9C,
			   0x94, 0xAC, 0x90, 0x80, 0x9C, 0x84, 0x74, 0x8C, 0x74, 0x64, 0x7C,
			   0x64, 0x58, 0x6C, 0x54, 0x4C, 0x60, 0x48, 0x44, 0x50, 0x40, 0x38,
			   0x44, 0x34, 0x2C, 0x34, 0x2C, 0x24, 0x24, 0x18, 0x18, 0x18, 0x10,
			   0x10, 0xFC, 0xF8, 0xFC, 0xAC, 0xD4, 0xF0, 0x70, 0xAC, 0xE4, 0x34,
			   0x8C, 0xD8, 0x00, 0x6C, 0xD0, 0x30, 0x8C, 0xD8, 0x6C, 0xB0, 0xE4,
			   0xB0, 0xD4, 0xF0, 0xFC, 0xFC, 0xF8, 0xFC, 0xEC, 0x40, 0xFC, 0xC0,
			   0x28, 0xFC, 0x8C, 0x10, 0xFC, 0x50, 0x00, 0xC8, 0x38, 0x00, 0x98,
			   0x28, 0x00, 0x68, 0x18, 0x00, 0x7C, 0xDC, 0x7C, 0x44, 0xB4, 0x44,
			   0x18, 0x90, 0x18, 0x00, 0x6C, 0x00, 0xF8, 0xB8, 0xFC, 0xFC, 0x64,
			   0xEC, 0xFC, 0x00, 0xB4, 0xCC, 0x00, 0x70, 0xFC, 0xFC, 0x00, 0x00,
			   0x00, 0xFF, 0x00, 0xFC, 0x00, 0xFC, 0x00, 0x00, 0xFC, 0xFC, 0xFC,
			   0x61, 0x61, 0x61, 0xC0, 0xC0, 0xC0, 0xFC, 0x00, 0xF1};

	// Sanitize a filename to prevent path traversal attacks
	std::string sanitizeFilename(const std::string& input) {
		std::string sanitized;
		sanitized.reserve(input.size());

		for (const char c : input) {
			// Allow only alphanumeric characters, underscore, hyphen, and
			// period
			auto uchr = static_cast<unsigned char>(c);
			if (isalnum(uchr) != 0 || c == '_' || c == '-' || c == '.') {
				sanitized += c;
			} else {
				// Replace potentially dangerous characters with underscore
				sanitized += '_';
			}
		}

		return sanitized;
	}

	// Sanitize a file path to prevent directory traversal attacks
	std::string sanitizeFilePath(const std::string& path) {
		if (path.empty()) {
			return "";
		}

		std::string sanitized;
		std::string filename;

		// Extract the path and filename
		size_t lastSlash = path.find_last_of("/\\");
		if (lastSlash != std::string::npos) {
			sanitized = path.substr(0, lastSlash + 1);
			filename  = path.substr(lastSlash + 1);
		} else {
			filename = path;
		}

		// Sanitize just the filename part
		sanitized += sanitizeFilename(filename);

		return sanitized;
	}

	// Save a frame to a PNG file
	bool saveFrameToPNG(
			const std::string& filename, const unsigned char* data,
			size_t width, size_t height, const Palette_t& palette) {
		// Create file for writing with restricted permissions
		FILE* fp = nullptr;
#ifdef _WIN32
		// Windows implementation
		fp = fopen(filename.c_str(), "wb");
#else
		// Unix/Mac implementation with restricted permissions (0644 =
		// rw-r--r--)
		int fd = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
		fp     = (fd >= 0) ? fdopen(fd, "wb") : nullptr;
		if ((fp == nullptr) && fd >= 0) {
			close(fd);
		}
#endif

		if (fp == nullptr) {
			std::cerr << "Error: Failed to open file for writing: " << filename
					  << std::endl;
			return false;
		}

		// Initialize libpng structures
		png_structp png_ptr = png_create_write_struct(
				PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
		if (png_ptr == nullptr) {
			fclose(fp);
			return false;
		}

		png_infop info_ptr = png_create_info_struct(png_ptr);
		if (info_ptr == nullptr) {
			png_destroy_write_struct(&png_ptr, nullptr);
			fclose(fp);
			return false;
		}

		if (setjmp(png_jmpbuf(png_ptr))) {
			png_destroy_write_struct(&png_ptr, &info_ptr);
			fclose(fp);
			return false;
		}

		png_init_io(png_ptr, fp);

		// Set image attributes for indexed color PNG
		png_set_IHDR(
				png_ptr, info_ptr, width, height,
				8,    // bit depth
				PNG_COLOR_TYPE_PALETTE, PNG_INTERLACE_NONE,
				PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

		// Set up the palette
		png_color png_palette[256];
		for (size_t i = 0; i < std::size(png_palette); i++) {
			png_palette[i].red   = palette[(i * 3)];
			png_palette[i].green = palette[(i * 3) + 1];
			png_palette[i].blue  = palette[(i * 3) + 2];
		}

		png_set_PLTE(png_ptr, info_ptr, png_palette, std::size(png_palette));

		// Create full transparency array - 0 is fully transparent, 255 is fully
		// opaque
		png_byte trans[256];
		for (size_t i = 0; i < std::size(trans); i++) {
			// Make only index 255 transparent
			trans[i] = (i == 255) ? 0 : 255;
		}

		// Set transparency for all palette entries, with index 255 being
		// transparent
		png_set_tRNS(png_ptr, info_ptr, trans, std::size(trans), nullptr);

		// Write the PNG info
		png_write_info(png_ptr, info_ptr);

		// Allocate memory for row pointers
		std::vector<png_bytep> row_pointers(height);
		for (size_t y = 0; y < height; ++y) {
			row_pointers[y] = const_cast<png_bytep>(&data[y * width]);
		}

		// Write image data
		png_write_image(png_ptr, row_pointers.data());
		png_write_end(png_ptr, nullptr);

		// Clean up
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);

		std::cout << "Successfully saved PNG: " << filename << std::endl;
		return true;
	}

	// Import SHP to PNG
	bool importSHP(
			const std::string& shpFilename,
			const std::string& outputPngFilename,
			const std::string& paletteFile) {
		// Load palette - either from specified file or use default game palette
		Palette_t palette;

		if (!paletteFile.empty()) {
			// Load from specified palette file
			U7object pal_obj(paletteFile, 0);
			size_t   len;
			auto     buf = pal_obj.retrieve(len);
			if (buf && len >= palette.size()) {
				std::copy_n(palette.begin(), palette.size(), buf.get());
				std::cout << "Using palette from file: " << paletteFile
						  << std::endl;
			} else {
				std::cerr
						<< "Warning: Could not load palette file, using default"
						<< std::endl;
				// Use default palette as fallback
				palette = DefaultPalette;
			}
		} else {
			// Always use the default palette
			palette = DefaultPalette;
			std::cout << "Using default palette" << std::endl;
		}

		// Sanitize the output path to prevent path traversal attacks
		std::string outputPath = sanitizeFilePath(outputPngFilename);

		// Extract the base filename from shpFilename (remove path and
		// extension)
		std::string baseFilename = shpFilename;

		// Remove path if present
		size_t lastSlash = baseFilename.find_last_of("/\\");
		if (lastSlash != std::string::npos) {
			baseFilename = baseFilename.substr(lastSlash + 1);
		}

		// Remove extension if present
		size_t lastDot = baseFilename.find_last_of('.');
		if (lastDot != std::string::npos) {
			baseFilename = baseFilename.substr(0, lastDot);
		}

		// Sanitize the filename to prevent path traversal attacks
		baseFilename = sanitizeFilename(baseFilename);

		// Create output directory path if needed
		std::string outputDir;

		// Extract directory from outputPath if there is one
		size_t lastPathSep = outputPath.find_last_of("/\\");
		if (lastPathSep != std::string::npos) {
			outputDir = outputPath.substr(0, lastPathSep + 1);
		}

		// Combine output directory with base filename
		std::string finalOutputBase = outputDir + baseFilename;

		// Log the filename extraction
		std::cout << "Using base filename: " << baseFilename << std::endl;
		std::cout << "Final output base: " << finalOutputBase << std::endl;

		// Load SHP file using Shape_file constructor
		Shape_file shape(shpFilename.c_str());

		int frameCount = shape.get_num_frames();
		if (frameCount == 0) {
			std::cerr << "Error: No frames found in SHP file" << std::endl;
			return false;
		}

		// Find max dimensions
		int width  = 0;
		int height = 0;
		for (int i = 0; i < frameCount; i++) {
			Shape_frame* frame = shape.get_frame(i);
			if (frame == nullptr) {
				continue;
			}
			width  = std::max(width, frame->get_width());
			height = std::max(height, frame->get_height());
		}

		// Create individual PNG for each frame
		for (int i = 0; i < frameCount; i++) {
			Shape_frame* frame = shape.get_frame(i);
			if (frame == nullptr) {
				continue;
			}

			// Use baseFilename instead of outputPngFilename
			std::string frameFilename
					= finalOutputBase + "_" + std::to_string(i) + ".png";
			std::string metadataFilename
					= finalOutputBase + "_" + std::to_string(i) + ".meta";

			// Sanitize the final complete paths before using them
			std::string sanitizedFrameFilename
					= sanitizeFilePath(frameFilename);
			std::string sanitizedMetadataFilename
					= sanitizeFilePath(metadataFilename);

			// Get frame data and dimensions - make sure to get the actual
			// dimensions for this frame
			int frameWidth  = frame->get_width();
			int frameHeight = frame->get_height();
			int offset_x    = frame->get_xleft();
			int offset_y    = frame->get_yabove();

			// Debug output for frame dimensions
			std::cout << "Frame " << i << " dimensions: " << frameWidth << "x"
					  << frameHeight << std::endl;

// Write frame info to metadata file
#ifdef _WIN32
			// Windows implementation
			FILE* metaFile = fopen(sanitizedMetadataFilename.c_str(), "w");
#else
			// Unix/Mac implementation with restricted permissions (0644 =
			// rw-r--r--)
			int fd
					= open(sanitizedMetadataFilename.c_str(),
						   O_WRONLY | O_CREAT | O_TRUNC, 0644);
			FILE* metaFile = (fd >= 0) ? fdopen(fd, "w") : nullptr;
			if ((metaFile == nullptr) && fd >= 0) {
				close(fd);
			}
#endif

			if (metaFile != nullptr) {
				// Write frame dimensions to metadata
				fprintf(metaFile, "frame_width=%d\nframe_height=%d\n",
						frameWidth, frameHeight);

				fprintf(metaFile, "offset_x=%d\noffset_y=%d\n", offset_x,
						offset_y);
				fclose(metaFile);
			}

			// Create an image buffer with the CORRECT dimensions for this frame
			Image_buffer8 imagebuf(frameWidth, frameHeight);

			// Fill with index 255 for transparency
			imagebuf.fill8(255);

			// Paint with x_offset=offset_x, y_offset=offset_y to position
			// correctly
			frame->paint(&imagebuf, offset_x, offset_y);
			const unsigned char* pixels = imagebuf.get_bits();

			// Create PNG from frame data with the correct dimensions
			if (!saveFrameToPNG(
						sanitizedFrameFilename, pixels, frameWidth, frameHeight,
						palette)) {
				std::cerr << "Error: Failed to save frame " << i << " to PNG"
						  << std::endl;
				return false;
			}
		}

		return true;
	}

	// Export PNG to SHP
	bool exportSHP(
			const std::string& basePath, const std::string& outputShpFilename,
			int defaultOffsetX, int defaultOffsetY,
			const std::string& metadataFile) {
		std::cout << "Exporting to SHP: " << outputShpFilename << std::endl;
		std::cout << "Using base path: " << basePath << std::endl;
		std::cout << "Using metadata file: " << metadataFile << std::endl;

		// Read metadata to get number of frames and offsets
		FILE* metaFile = fopen(metadataFile.c_str(), "r");
		if (metaFile == nullptr) {
			std::cerr << "Error: Unable to open metadata file: " << metadataFile
					  << std::endl;
			return false;
		}

		// Parse metadata
		int                              numFrames = 0;
		char                             line[256];
		std::vector<std::pair<int, int>> offsets;

		while (fgets(line, sizeof(line), metaFile) != nullptr) {
			// Remove newline if present
			size_t len = strlen(line);
			if (len > 0 && line[len - 1] == '\n') {
				line[len - 1] = '\0';
			}

			if (strncmp(line, "num_frames=", 11) == 0) {
				numFrames = atoi(line + 11);
				offsets.resize(
						numFrames,
						std::make_pair(defaultOffsetX, defaultOffsetY));
			} else if (
					strncmp(line, "frame", 5) == 0
					&& (strstr(line, "offset_x=") != nullptr)) {
				// Parse frame index and offset_x value
				int frameIndex = 0;
				int offsetX    = 0;
				sscanf(line, "frame%d_offset_x=%d", &frameIndex, &offsetX);
				if (frameIndex >= 0 && frameIndex < numFrames) {
					offsets[frameIndex].first = offsetX;
				}
			} else if (
					strncmp(line, "frame", 5) == 0
					&& (strstr(line, "offset_y=") != nullptr)) {
				// Parse frame index and offset_y value
				int frameIndex = 0;
				int offsetY    = 0;
				sscanf(line, "frame%d_offset_y=%d", &frameIndex, &offsetY);
				if (frameIndex >= 0 && frameIndex < numFrames) {
					offsets[frameIndex].second = offsetY;
				}
			}
		}

		fclose(metaFile);

		if (numFrames <= 0) {
			std::cerr << "Error: No frames specified in metadata" << std::endl;
			return false;
		}

		std::cout << "Exporting " << numFrames << " frames" << std::endl;

		// Create the Shape_file
		Shape_file shapeFile;
		shapeFile.resize(numFrames);

		// Load each PNG frame and create Shape_frames
		volatile int frameIdx = 0;
		while (frameIdx < numFrames) {
			// Construct the PNG filename
			std::string pngFilename = basePath;
			pngFilename += std::to_string(frameIdx);
			pngFilename += ".png";

			// Open the PNG file
			FILE* fp = fopen(pngFilename.c_str(), "rb");
			if (fp == nullptr) {
				std::cerr << "Error: Could not open frame file: " << pngFilename
						  << std::endl;
				return false;
			}

			// Initialize PNG read structures
			png_structp png_ptr = png_create_read_struct(
					PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
			if (png_ptr == nullptr) {
				fclose(fp);
				return false;
			}

			png_infop info_ptr = png_create_info_struct(png_ptr);
			if (info_ptr == nullptr) {
				png_destroy_read_struct(&png_ptr, nullptr, nullptr);
				fclose(fp);
				return false;
			}

			if (setjmp(png_jmpbuf(png_ptr))) {
				png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
				fclose(fp);
				return false;
			}

			png_init_io(png_ptr, fp);
			png_read_info(png_ptr, info_ptr);

			// Get image dimensions
			size_t width      = png_get_image_width(png_ptr, info_ptr);
			size_t height     = png_get_image_height(png_ptr, info_ptr);
			int    color_type = png_get_color_type(png_ptr, info_ptr);

			// Make sure it's an indexed color image
			if (color_type != PNG_COLOR_TYPE_PALETTE) {
				std::cerr << "Error: Frame " << frameIdx
						  << " is not an indexed color image" << std::endl;
				png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
				fclose(fp);
				return false;
			}

			// Read the image data
			std::vector<unsigned char> imageData(width * height);
			std::vector<png_bytep>     row_pointers(height);

			for (size_t y = 0; y < height; y++) {
				row_pointers[y] = &imageData[y * width];
			}

			png_read_image(png_ptr, row_pointers.data());

			// Clean up PNG read
			png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
			fclose(fp);

			// Process the image data for Shape_frame creation
			unsigned char transparentIndex
					= 255;    // Default transparent index in shapes
			ignore_unused_variable_warning(transparentIndex);

			// Create a buffer for the frame data - DON'T pre-fill with
			// transparency
			auto pixels = std::make_unique<unsigned char[]>(width * height);

			// Copy all pixels from PNG, treating index 255 as transparent
			for (size_t y = 0; y < height; y++) {
				for (size_t x = 0; x < width; x++) {
					unsigned char pixel = imageData[(y * width) + x];

					// Store the original pixel value - Shape_frame expects
					// index 255 to be transparent
					pixels[(y * width) + x] = pixel;
				}
			}

			// Get offset for this frame
			int xleft  = offsets[frameIdx].first;
			int yabove = offsets[frameIdx].second;

			std::cout << "Frame " << frameIdx << ": " << width << "x" << height
					  << " offset(" << xleft << "," << yabove << ")"
					  << std::endl;

			// Create Shape_frame with the correct constructor
			try {
				// Create the frame - must use the constructor that takes raw
				// pixel data
				auto frame = std::make_unique<Shape_frame>(
						std::move(pixels),    // raw pixel data
						width, height,        // dimensions
						xleft, yabove,        // offsets
						true                  // make a copy of the data
				);

				// Add the frame to the shape file
				shapeFile.set_frame(std::move(frame), frameIdx);
			} catch (std::exception& e) {
				std::cerr << "Exception creating Shape_frame: " << e.what()
						  << std::endl;
				return false;
			}

			// Increment at the end of the loop
			frameIdx = frameIdx + 1;
		}

		// Create an OFileDataSource for writing
		OFileDataSource out(outputShpFilename);
		if (!out.good()) {
			std::cerr << "Error: Could not open output file for writing: "
					  << outputShpFilename << std::endl;
			return false;
		}

		// Write the shape file to output
		shapeFile.write(out);

		std::cout << "Successfully exported SHP file: " << outputShpFilename
				  << std::endl;
		return true;
	}
}    // namespace

// Main function
int main(int argc, char* argv[]) {
	if (argc < 3) {
		std::cout << "Usage:" << std::endl;
		std::cout << "  Import mode: " << argv[0]
				  << " import <shp_file> <output_png> [palette_file]"
				  << std::endl;
		std::cout << "  Export mode: " << argv[0]
				  << " export <png_file> <output_shp> [use_transparency] "
					 "[offset_x] [offset_y] [metadata_file]"
				  << std::endl;
		return 1;
	}

	std::string mode = argv[1];

	if (mode == "import") {
		if (argc < 4) {
			std::cerr << "Import mode requires SHP file and output path"
					  << std::endl;
			return 1;
		}

		std::string shpFilename = argv[2];
		std::string outputPath  = argv[3];

		// Handle optional parameters
		std::string paletteFile;

		// Parse remaining arguments
		for (int i = 4; i < argc; i++) {
			if (argv[i][0] != '\0' && strcmp(argv[i], "separate") != 0) {
				paletteFile = argv[i];
			}
		}

		std::cout << "Loading SHP file: " << shpFilename << std::endl;
		std::cout << "Output path: " << outputPath << std::endl;
		std::cout << "Palette file: "
				  << (!paletteFile.empty() ? paletteFile : "default")
				  << std::endl;

		if (!importSHP(shpFilename, outputPath, paletteFile)) {
			std::cerr << "Failed to import SHP" << std::endl;
			return 1;
		}

		return 0;
	}
	if (mode == "export") {
		// Sanitize input paths to prevent path traversal attacks
		std::string sanitizedPngPath = sanitizeFilePath(argv[2]);
		std::string sanitizedShpPath = sanitizeFilePath(argv[3]);

		int offsetX = (argc > 5) ? atoi(argv[5]) : 0;
		int offsetY = (argc > 6) ? atoi(argv[6]) : 0;

		// Sanitize the metadata file path
		std::string metadataPath;
		if (argc > 7) {
			metadataPath = sanitizeFilePath(argv[7]);
		}

		if (exportSHP(
					sanitizedPngPath, sanitizedShpPath, offsetX, offsetY,
					metadataPath)) {
			std::cout << "Successfully converted PNG to SHP" << std::endl;
			return 0;
		}
		std::cerr << "Failed to convert PNG to SHP" << std::endl;
		return 1;
	}
	std::cerr << "Unknown mode. Use 'import' or 'export'." << std::endl;
	return 1;
}
