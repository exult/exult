/*
Copyright (C) 2000-2022 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "studio.h"

#if defined(MACOSX) || defined(_WIN32)
#	include "utils.h"
#endif

int main(int argc, char** argv) {
#ifdef MACOSX
	// setting up environment for Exult Studio in an app bundle
	setup_app_bundle_resource();
	if (is_system_path_defined("<APP_BUNDLE_RES>")) {
		std::string bundle_res = get_system_path("<APP_BUNDLE_RES>");
		if (U7exists(bundle_res)) {
			std::string bundle_share;
			std::string bundle_pixbuf;
			std::string bundle_im;
			bundle_share = bundle_res;
			bundle_share += "/share";
			bundle_pixbuf = bundle_res;
			bundle_pixbuf += "/lib/gdk-pixbuf-2.0/2.10.0/loaders.cache";
			bundle_im = bundle_res;
			bundle_im += "/lib/gtk-3.0/3.0.0/immodules.cache";
			const gchar* b_res    = bundle_res.c_str();
			const gchar* b_share  = bundle_share.c_str();
			const gchar* b_pixbuf = bundle_pixbuf.c_str();
			const gchar* b_im     = bundle_im.c_str();
			g_setenv("XDG_DATA_DIRS", b_share, 1);
			g_setenv("GTK_DATA_PREFIX", b_res, 1);
			g_setenv("GTK_EXE_PREFIX", b_res, 1);
			g_setenv("GTK_PATH", b_res, 1);
			g_setenv("GDK_PIXBUF_MODULE_FILE", b_pixbuf, 1);
			g_setenv("GTK_IM_MODULE_FILE", b_im, 1);
		}
	}
#endif

	try {
		ExultStudio studio(argc, argv);
		studio.run();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

#ifdef _WIN32
	cleanup_output("studio_");
#endif
	return 0;
}
