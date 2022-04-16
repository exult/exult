# Exult instructions for Android

## To Play
First you need to get Ultima 7 or Serpent Isle. Either you own it already, or you buy it somewhere.

Second, you need the Android Exult app itself.  It is not yet available in any app stores, but you can download builds from [here](http://exult.info/download.php) (recommended) or build from source as described below.

The Android app starts up with a launcher screen that assists with installing the content (game files, music pack, and mods).  You will need to get the content archives copied/downloaded to your Android device.  Then you can browse to them from the launcher, which will extract the archives and install the content in the correct locations so Exult finds them automatically.

The launcher has been tested and can automatically install content from the following sources:
- [Ultima 7 Complete at Good Old Games](https://www.gog.com/game/ultima_7_complete)
  - From your android device, log into your GOG account through the browser.
  - Select Mac as your platform
  - Look, under "DOWNLOAD OFFLINE BACKUP GAME INSTALLERS"
  - Download the `.pkg` file(s)
- [Zipped all-in-one audio pack for manual installation](http://prdownloads.sourceforge.net/exult/exult_audio.zip)
- [Marzo's Black Gate Keyring (requires FoV)](http://exult.info/snapshots/Keyring.zip)
- [Marzo's Serpent Isle Fixes (requires SS)](http://exult.info/snapshots/Sifixes.zip)
- [SourceForge Island (requires FoV)](http://exult.info/snapshots/SFisland.zip)

Now download Exult, and have fun!

## More Information

More information can be found in the accompanying files README and FAQ.  In addition, you might want to check out our homepage at http://exult.info

## How to compile for Android

The following guide is only for people that want to compile Exult for Android on their own. This is not needed if there is a current snapshot for Android available on our download page.

The Android APK is built as an optional component of a regular native Exult build.  Before attempting to build the Android APK, follow [the instructions](..//INSTALL) to install any dependencies and build Exult for your desktop.

In addition to the regular desktop Exult dependencies, you will need the android SDK and NDK.  The easiest way to get these is to install Android Studio.  You will also need to edit your path so that the build can find the `gradle` and `sdkmanager` commands.  Android Studio can be installed for free from [Google](https://developer.android.com/studio).

With those dependencies in place, you can build Exult as normal, but pass the `--enable-android-apk` flag to the `configure` script.  The following is a quick summary of the commands to build:

```
$ ./autogen.sh
$ ./configure --enable-android-apk=debug
$ make
```

The final APK will be located in `./android/app/build/outputs/apk/debug/app-debug.apk`.

Building a release build is similar, but requires signing the binary:

```
$ ./autogen.sh
$ ./configure --enable-android-apk=release
$ make
$ /path/to/Android/Sdk/build-tools/version/zipalign -v -p 4 ./android/app/build/outputs/apk/release/app-release-unsigned.apk ./android/app/build/outputs/apk/release/app-release-unsigned-aligned.apk
$ /path/to/Android/Sdk/build-tools/version/apksigner sign --ks path/to/my/android-keystore.jks --out ./android/app/build/outputs/apk/release/app-release-signed.apk ./android/app/build/outputs/apk/release/app-release-unsigned-aligned.apk

```
