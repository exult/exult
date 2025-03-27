# Eventual location of the APK we'll be building.
ANDROID_APK=$(abs_builddir)/app/build/outputs/apk/$(APK_BUILD_TYPE)/app-$(APK_BUILD_TYPE)$(APK_SUFFIX).apk

# APK is nominally a data file and we'll install it with the other data files if asked.
androiddir = $(datadir)/exult
android_DATA=$(ANDROID_APK)

# These are shared with iOS, but need to be staged in the android tree and are not allowed to have
# a '-' in the filename.
DPAD_DIRECTIONS = \
	$(abs_builddir)/app/src/main/res/drawable/dpad_east.png \
	$(abs_builddir)/app/src/main/res/drawable/dpad_north.png \
	$(abs_builddir)/app/src/main/res/drawable/dpad_northeast.png \
	$(abs_builddir)/app/src/main/res/drawable/dpad_northwest.png \
	$(abs_builddir)/app/src/main/res/drawable/dpad_south.png \
	$(abs_builddir)/app/src/main/res/drawable/dpad_southeast.png \
	$(abs_builddir)/app/src/main/res/drawable/dpad_southwest.png \
	$(abs_builddir)/app/src/main/res/drawable/dpad_west.png
DPAD_CENTER = \
	$(abs_builddir)/app/src/main/res/drawable/dpad_center.png
BUTTON = \
	$(abs_builddir)/app/src/main/res/drawable/btn.png \
	$(abs_builddir)/app/src/main/res/drawable/btnpressed.png
SHARED_IMAGE_ASSETS = $(DPAD_DIRECTIONS) $(DPAD_CENTER) $(BUTTON)

# All the DATA files that getinstalled by a normal build.
# TODO: Would be ideal to infer this list rather than hard-coding it here.  
DATA_FILES = \
	exult.flx \
	exult_si.flx \
	exult_bg.flx \
	midisfx.flx

# We'll place the data files in the apk's assets folder so that they get bundled into the package.
APK_ASSETS_DIR=$(abs_builddir)/app/src/main/assets/data
APK_ASSETS = $(DATA_FILES:%=$(APK_ASSETS_DIR)/%)

# We'll use the java sources for Android integration provided in the SDL source tree.
LIBSDL_JAVA_DIR = \
	$(abs_builddir)/app/src/main/java/org/libsdl

# Gradle doesn't appear to have a simple way to do out-of-tree builds, so instead we'll symbolic link the source tree
# into the build directory and run the build from the links.  Note that the final source tree includes elements from
# multiple locations and we end up installing an assets folder as part of the build in app/src/main/assets which we don't
# want to end up in the source tree, so we can't simply have one link to the top-level source tree.
APK_SOURCE_TREE = \
	$(abs_builddir)/build.gradle \
	$(abs_builddir)/gradle.properties \
	$(abs_builddir)/settings.gradle \
	$(abs_builddir)/app/src/main/AndroidManifest.xml \
	$(abs_builddir)/app/src/main/cpp \
	$(abs_builddir)/app/src/main/java/info/exult \
	$(abs_builddir)/app/src/main/res/layout \
	$(abs_builddir)/app/src/main/res/values \
	$(abs_builddir)/app/src/main/res/xml \
	$(abs_builddir)/app/src/main/res/drawable/ankh.png \
	$(abs_builddir)/app/src/main/res/drawable/back.gif \
	$(abs_builddir)/app/src/main/res/drawable/exult_logo_nice.png \
	$(abs_builddir)/app/src/main/res/drawable/tile_back.xml

# Pull together all the components into the full staged APK build tree
APK_BUILD_TREE = $(APK_SOURCE_TREE) $(LIBSDL_JAVA_DIR) $(SHARED_IMAGE_ASSETS) $(APK_ASSETS)

# Pass SDL and Exult source directories to gradle as project properties
SDL_SOURCE_DIR=$(abs_builddir)/sdl-src
GRADLE_PROJECT_PROPS=--project-prop EXULT_SOURCE_DIR=$(abs_top_srcdir) --project-prop SDL_SOURCE_DIR=$(SDL_SOURCE_DIR)

# Always call the gradle wrapper to update the APK
.PHONY: $(ANDROID_APK)
$(ANDROID_APK): $(abs_builddir)/gradlew
	ANDROID_HOME=$(ANDROID_HOME) ./gradlew assemble$(APK_BUILD_TYPE) $(GRADLE_PROJECT_PROPS) --info

# Generate the gradle wrapper
$(abs_builddir)/gradlew: $(APK_BUILD_TREE) android_packages
	ANDROID_HOME=$(ANDROID_HOME) $(GRADLE) wrapper $(GRADLE_PROJECT_PROPS)

# Always try to install any missing android packages
.PHONY: android_packages
android_packages:
	$(SDKMANAGER) --install "platforms;android-$(ANDROID_SDK_VERSION)"
	$(SDKMANAGER) --install "build-tools;$(ANDROID_BUILD_TOOLS_VERSION)"
	$(SDKMANAGER) --install "ndk;$(ANDROID_NDK_VERSION)"
	$(SDKMANAGER) --install "cmake;$(ANDROID_CMAKE_VERSION)"
	$(SDKMANAGER) --licenses

# Various locations in the source and native build to copy files from that need to be staged for the APK build
# TODO: prefer $(LN_S) for everything, but gradle doesn't seem to like symbolic links for assets

$(APK_SOURCE_TREE): $(abs_builddir)/%: $(abs_srcdir)/%
	@$(MKDIR_P) $(@D)
	$(LN_S) $< $@

$(abs_builddir)/app/src/main/java/org/libsdl: $(SDL_SOURCE_DIR)
	@$(MKDIR_P) $(@D)
	$(LN_S) $(SDL_SOURCE_DIR)/android-project/app/src/main/java/org/libsdl $@

$(abs_builddir)/app/src/main/res/drawable/dpad_center.png: $(abs_top_srcdir)/ios/Images/joypad-glass.png
	@$(MKDIR_P) $(@D)
	$(LN_S) $< $@

$(DPAD_DIRECTIONS): $(abs_builddir)/app/src/main/res/drawable/dpad_%.png: $(abs_top_srcdir)/ios/Images/joypad-glass-%.png
	@$(MKDIR_P) $(@D)
	$(LN_S) $< $@

$(BUTTON): $(abs_builddir)/app/src/main/res/drawable/%: $(abs_top_srcdir)/ios/Images/%
	@$(MKDIR_P) $(@D)
	$(LN_S) $< $@

$(APK_ASSETS): $(APK_ASSETS_DIR)/%: $(abs_top_srcdir)/data/%
	@$(MKDIR_P) $(@D)
	$(INSTALL) $< $@

# In addition to building the SDL C library for NDK, we also need Java
# sources compiled into the app.  Rather than let the NDK CMake pull
# in SDL like the other dependencies, pre-load the codebase here at
# the top level so that we have access to the Java sources too.
# The NDK build also utilizes this codebase rather than downloading a
# separate copy.
$(SDL_SOURCE_DIR):
	$(GIT) -c advice.detachedHead=false clone --depth 1 https://github.com/libsdl-org/SDL.git $@
