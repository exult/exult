# Exult instructions for Android

## To Play
First you need to get Ultima VII: The Black Gate or Ultima VII Part 2: Serpent Isle. Either you own it already, or you buy it somewhere.

Second, you need the Android Exult app itself.  It is not yet available in any app stores, but you can download builds from [here](https://exult.info/download.php) (recommended) or build from source as described below.

The Android app starts up with a launcher screen that assists with installing the content (game files, music pack, and mods).  You will need to get the content archives copied/downloaded to your Android device.  Then you can browse to them from the launcher, which will extract the archives and install the content in the correct locations so Exult finds them automatically.

The launcher has been tested and can automatically install content from the following sources:
- [Ultima 7 Complete at Good Old Games](https://www.gog.com/game/ultima_7_complete)
  - From your android device, log into your GOG account through the browser.
  - Select Mac as your platform
  - Look, under "DOWNLOAD OFFLINE BACKUP GAME INSTALLERS"
  - Download the `.pkg` file(s)
- [Zipped all-in-one audio pack for manual installation](http://prdownloads.sourceforge.net/exult/exult_audio.zip)
- [Marzo's Black Gate Keyring (requires FoV)](https://exult.info/snapshots/Keyring.zip)
- [Marzo's Serpent Isle Fixes (requires SS)](https://exult.info/snapshots/Sifixes.zip)
- [SourceForge Island (requires FoV)](https://exult.info/snapshots/SFisland.zip)

Now download Exult, and have fun!

## More Information

More information can be found in the accompanying files README and FAQ.  In addition, you might want to check out our homepage at https://exult.info

## How to compile for Android

The following guide is only for people that want to compile Exult for Android on their own. This is not needed if there is a current snapshot for Android available on our download page.

The Android APK is built as an optional component of a regular native Exult build. Before attempting to build the Android APK, follow [the instructions](..//INSTALL) to install any dependencies and build Exult for your desktop.

In addition to the regular desktop Exult dependencies, you will need the android SDK and NDK. The easiest way to get these is to install Android Studio. You will also need to edit your path so that the build can find the `gradle` and `sdkmanager` commands.  Android Studio can be installed for free from [Google](https://developer.android.com/studio).

Now you also need Gradle, follow the instructions at https://linuxize.com/post/how-to-install-gradle-on-ubuntu-20-04/ and install Gradle version 8.7 (or newer).

With those dependencies in place, you can build Exult, but pass the `--enable-android-apk` flag to the `configure` script. However you need to make sure to build Exult outside of its source to prevent any problems when you cross compile the various Android arches. You can also disable most build options as the Android build is done via CMake. 
The following is a quick summary of the commands to build (assuming you are in the exult source):

```
mkdir ./../build && cd ./../build
$ autoreconf -v -i ./../exult
$ ./../exult/configure --enable-data --enable-android-apk=debug \
            --disable-exult --disable-tools --disable-timidity-midi --disable-alsa \
            --disable-fluidsynth --disable-mt32emu --disable-all-hq-scalers \
            --disable-nxbr --disable-zip-support
$ cd files && make
$ cd ./../data && make
$ cd ./../android && make
```
The final debug APK will be located in `./android/app/build/outputs/apk/debug/app-debug.apk`.
On debug builds you can also pass through the debug level and optimization level with `--with-debug=` and `--with-optimization=`, while it defaults to `--with-debug=messages`.

Building a release build is similar, but requires signing the binary:

```
mkdir ./../build && cd ./../build
$ autoreconf -v -i ./../exult
$ ./../exult/configure --enable-data --enable-android-apk=release \
            --disable-exult --disable-tools --disable-timidity-midi --disable-alsa \
            --disable-fluidsynth --disable-mt32emu --disable-all-hq-scalers \
            --disable-nxbr --disable-zip-support
$ cd files && make
$ cd ./../data && make
$ cd ./../android && make
$ /path/to/Android/Sdk/build-tools/version/zipalign -v -p 4 ./../exult/android/app/build/outputs/apk/release/app-release-unsigned.apk ./../exult/android/app/build/outputs/apk/release/app-release-unsigned-aligned.apk
$ /path/to/Android/Sdk/build-tools/version/apksigner sign --ks path/to/my/android-keystore.jks --out ./../exult/android/app/build/outputs/apk/release/app-release-signed.apk ./../exult/android/app/build/outputs/apk/release/app-release-unsigned-aligned.apk

```
The final release APK will be located in `app-release-signed.apk`.

## Issues

There are currently several issues (mostly related to SDK and NDKs):
- Currently mods installation is hardcoded to accept only the mods from the Exult team - see https://github.com/exult/exult/issues/371
