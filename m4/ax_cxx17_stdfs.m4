# ===========================================================================
#  https://www.gnu.org/software/autoconf-archive/ax_cxx_compile_stdcxx.html
# ===========================================================================
#
# SYNOPSIS
#
#   AX_CXX17_CHECK_STDFS
#
# DESCRIPTION
#
#   Check for possibility of using C++17's std::filesystem. Should it be
#   possible to compile code containing elements of the namespace,
#   the macro shall also check if it is possible to link the program only
#   using libstdc++, or is it also needed to link against transitional
#   package libstdc++fs. Should the latter library be impossible to be
#   found, the existence of std::filesystem shall be marked as false.
#
#
# LICENSE
#
#   Copyright (c) 2020 Aleksander Miera <ammiera@hotmail.com>
#
#   Copying and distribution of this file, with or without modification, are
#   permitted in any medium without royalty provided the copyright notice
#   and this notice are preserved.  This file is offered as-is, without any
#   warranty.

AC_DEFUN([AX_CXX17_HAVE_STDFS],[
  AC_REQUIRE([AC_PROG_CXX])
  AX_REQUIRE_DEFINED([AX_CXX_COMPILE_STDCXX_17])
  AC_LANG_PUSH([C++])
  if test "x$HAVE_CXX17" = "x"; then
    AX_CXX_COMPILE_STDCXX_17([noext],[optional])
  fi

  if test "x$HAVE_CXX17" = "x1"; then
    dnl not sure if this check is needed, as the above
    dnl should also check compilation. Can sb with MacOS Mojave check it?
    AC_MSG_CHECKING([if std::filesystem compiles fine])
    AC_COMPILE_IFELSE([AC_LANG_SOURCE([
      #include <filesystem>

      int main() {
      	std::filesystem::path a{};
      }
      ])],
      	[ac_cv_compile_fs_stdlib=yes],
      	[ac_cv_compile_fs_stdlib=no]
      )
    AC_MSG_RESULT($ac_cv_compile_fs_stdlib)

    AC_MSG_CHECKING([if std::filesystem is included in standard library])
    AC_LINK_IFELSE([AC_LANG_SOURCE([
      #include <filesystem>
      #include <system_error>

      int main() {
      	std::error_code e;
      	std::filesystem::create_directory("/dev/null", e);
      }
    ])],
  	[ac_cv_link_fs_stdlib_only=yes],
  	[ac_cv_link_fs_stdlib_only=no]
  )
  AC_MSG_RESULT($ac_cv_link_fs_stdlib_only)

  if ! test "x$ac_cv_link_fs_stdlib_only" = xyes; then
    dnl I wish I could go better here, but due to name mangling
    dnl AC_CHECK_LIB becomes useless
    LIBS_OLD="$LIBS"
    LIBS="$LIBS -lstdc++fs"

    AC_MSG_CHECKING([if std::filesystem is in transitional stdc++fs library])
      AC_LINK_IFELSE([AC_LANG_SOURCE([
        #include <filesystem>
        #include <system_error>

        int main() {
        	std::error_code e;
        	std::filesystem::create_directory("/dev/null", e);
        }
      ])],
        [ac_cv_link_fs_stdlibfs=yes],
        [ac_cv_link_fs_stdlibfs=no]
      )
      AC_MSG_RESULT($ac_cv_link_fs_stdlibfs)
      LIBS="$LIBS_OLD"
    fi

    ac_cv_cxx17_use_stdfilesystem=no
    if test "x$ac_cv_compile_fs_stdlib" = "xyes"; then
      if test "x$ac_cv_link_fs_stdlibfs" = "xyes" -o "x$ac_cv_link_fs_stdlib_only" = "xyes"; then
        ac_cv_cxx17_use_stdfilesystem=yes
      fi
    fi

    AM_CONDITIONAL([HAVE_FILESYSTEM], [test "x$ac_cv_cxx17_use_stdfilesystem" = xyes])
    if test "x$ac_cv_cxx17_use_stdfilesystem" = "xyes"; then
      AC_DEFINE(HAVE_STDFILESYSTEM, 1, [Use std::filesystem])
    fi

  else
    AC_MSG_WARN([C++17 not supported])
    dnl BAIL OUT!!!
  fi

  STDFSLIB=""
  if test "x$ac_cv_link_fs_stdlibfs" = "xyes"; then
    STDFSLIB="-lstdc++fs"
  fi
  AC_SUBST(STDFSLIB)
  AC_LANG_POP([C++])
])
