dnl Check if we have SDL (sdl-config, header and library) version >= 2.0.1
dnl Extra options: --with-sdl-prefix=PATH
dnl Output:
dnl SDL_CFLAGS and SDL_LIBS are set and AC_SUBST-ed
dnl HAVE_SDL_H is AC_DEFINE-d

AC_DEFUN([EXULT_CHECK_SDL],[
  exult_backupcppflags="$CPPFLAGS"
  exult_backupldflags="$LDFLAGS"
  exult_backuplibs="$LIBS"

  exult_sdlok=yes

  AC_ARG_WITH(sdl-prefix,[  --with-sdl-prefix=PFX   Prefix where SDL is installed (optional)], sdl_prefix="$withval", sdl_prefix="")
  AC_ARG_WITH(sdl-exec-prefix,[  --with-sdl-exec-prefix=PFX Exec prefix where SDL is installed (optional)], sdl_exec_prefix="$withval", sdl_exec_prefix="")

  dnl First: find sdl-config or sdl2-config
  exult_extra_path=$prefix/bin:$prefix/usr/bin
  sdl_args=""
  if test x$sdl_exec_prefix != x ; then
     sdl_args="$sdl_args --exec-prefix=$sdl_exec_prefix"
     exult_extra_path=$sdl_exec_prefix/bin
  fi
  if test x$sdl_prefix != x ; then
     sdl_args="$sdl_args --prefix=$sdl_prefix"
     exult_extra_path=$sdl_prefix/bin
  fi
  AC_PATH_PROGS(SDL_CONFIG, sdl2-config, no, [$exult_extra_path:$PATH])
  if test "$SDL_CONFIG" = "no" ; then
    exult_sdlok=no
  else
    SDL_CFLAGS=`$SDL_CONFIG $sdl_args --cflags`
    SDL_LIBS=`$SDL_CONFIG $sdl_args --libs`

    sdl_major_version=`$SDL_CONFIG $sdl_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    sdl_minor_version=`$SDL_CONFIG $sdl_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    sdl_patchlevel=`$SDL_CONFIG $sdl_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
  fi

  REQ_MAJOR=2
  REQ_MINOR=0
  REQ_PATCHLEVEL=1
  REQ_VERSION=$REQ_MAJOR.$REQ_MINOR.$REQ_PATCHLEVEL

  AC_MSG_CHECKING([for SDL - version >= $REQ_VERSION])


  dnl Second: include "SDL.h"

  if test x$exult_sdlok = xyes ; then
    CPPFLAGS="$CPPFLAGS $SDL_CFLAGS"
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include "SDL.h"
    ]],)],sdlh_found=yes,sdlh_found=no)

    if test x$sdlh_found = xno; then
      exult_sdlok=no
    else
      AC_DEFINE(HAVE_SDL_H, 1, [Define to 1 if you have the "SDL.h" header file])
    fi
  fi

  dnl Next: version check (cross-compile-friendly idea borrowed from autoconf)
  dnl (First check version reported by sdl-config, then confirm
  dnl  the version in SDL.h matches it)

  if test x$exult_sdlok = xyes ; then

    if test ! \( \( $sdl_major_version -gt $REQ_MAJOR \) -o \( \( $sdl_major_version -eq $REQ_MAJOR \) -a \( \( $sdl_minor_version -gt $REQ_MINOR \) -o \( \( $sdl_minor_version -eq $REQ_MINOR \) -a \( $sdl_patchlevel -gt $REQ_PATCHLEVEL \) \) \) \) \); then
      exult_sdlok="no, version < $REQ_VERSION found"
    else
      AC_COMPILE_IFELSE([AC_LANG_SOURCE([[
      #include "SDL.h"

      int main()
      {
        static int test_array[1-2*!(SDL_MAJOR_VERSION==$sdl_major_version&&SDL_MINOR_VERSION==$sdl_minor_version&&SDL_PATCHLEVEL==$sdl_patchlevel)];
        test_array[0] = 0;
        return 0;
      }
      ]])],,[[exult_sdlok="no, version of SDL.h doesn't match that of sdl-config"]])
    fi
  fi

  dnl Next: try linking

  if test "x$exult_sdlok" = xyes; then
    LIBS="$LIBS $SDL_LIBS"

    AC_LINK_IFELSE([AC_LANG_SOURCE([[
    #include "SDL.h"

    int main(int argc, char* argv[]) {
      SDL_Init(0);
      return 0;
    }
    ]])],sdllinkok=yes,sdllinkok=no)
    if test x$sdllinkok = xno; then
      exult_sdlok=no
    fi
  fi

  AC_MSG_RESULT($exult_sdlok)

  LDFLAGS="$exult_backupldflags"
  CPPFLAGS="$exult_backupcppflags"
  LIBS="$exult_backuplibs"

  if test "x$exult_sdlok" = xyes; then
    AC_SUBST(SDL_CFLAGS)
    AC_SUBST(SDL_LIBS)
    ifelse([$1], , :, [$1])
  else
    ifelse([$2], , :, [$2])
  fi
]);


