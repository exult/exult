# Eventual location of the APK we'll be building.
ANDROID_APK=$(abs_builddir)/app/build/outputs/apk/$(APK_BUILD_TYPE)/app-$(APK_BUILD_TYPE)$(APK_SUFFIX).apk

# APK is nominally a data file and we'll install it with the other data files if asked.
androiddir = $(datadir)/exult
android_DATA=$(ANDROID_APK)

# All the DATA files that getinstalled by a normal build.
# TODO: Would be ideal to infer this list rather than hard-coding it here.  
DATA_FILES = \
	bg_paperdol.vga \
	exult.flx \
	exult_si.flx \
	u7bgintrinsics.data \
	u7opcodes.data \
	u7siintrinsics.data \
	bg_mr_faces.vga \
	exult_bg.flx \
	exultmsg.txt \
	midisfx.flx \
	u7misc.data \
	u7sibetaintrinsics.data

# We'll place the data files in the apk's assets folder so that they get bundled into the package.
APK_ASSETS_DIR=$(abs_builddir)/app/src/main/assets/data
APK_ASSETS = $(patsubst %,$(APK_ASSETS_DIR)/%,$(DATA_FILES))

# Gradle doesn't appear to have a simple way to do out-of-tree builds, so instead we'll symbolic link the source tree
# into the build directory and run the build from the links.  Note that the final source tree includes elements from
# multiple locations and we end up installing an assets folder as part of the build in app/src/main/assets which we don't
# want to end up in the source tree, so we can't simply have one link to the top-level source tree.
APK_SOURCE_TREE = \
	$(patsubst $(srcdir)/%,$(abs_builddir)/%,$(wildcard $(srcdir)/*gradle*)) \
	$(abs_builddir)/app/src/main/AndroidManifest.xml \
	$(abs_builddir)/app/src/main/cpp \
	$(abs_builddir)/app/src/main/java/info/exult \
	$(abs_builddir)/app/src/main/java/org/libsdl \
	$(abs_builddir)/app/src/main/res \
	$(APK_ASSETS)

# Pass Exult source directory to gradle as project property
GRADLE_PROJECT_PROPS=--project-prop EXULT_SOURCE_DIR=$(abs_top_srcdir)

# Always call the gradle wrapper to update the APK
.PHONY: $(ANDROID_APK)
$(ANDROID_APK): $(abs_builddir)/gradlew
	ANDROID_SDK_ROOT=$(ANDROID_SDK_ROOT) ./gradlew assemble$(APK_BUILD_TYPE) $(GRADLE_PROJECT_PROPS) --info

# Generate the gradle wrapper
$(abs_builddir)/gradlew: $(APK_SOURCE_TREE) android_packages
	ANDROID_SDK_ROOT=$(ANDROID_SDK_ROOT) $(GRADLE) wrapper $(GRADLE_PROJECT_PROPS)

# Always try to install any missing android packages
.PHONY: android_packages
android_packages:
	$(SDKMANAGER) --install "platforms;android-$(ANDROID_SDK_VERSION)"
	$(SDKMANAGER) --install "build-tools;$(ANDROID_BUILD_TOOLS_VERSION)"
	$(SDKMANAGER) --licenses

# Various locations in the source and native build to copy files from that need to be staged for the APK build
# TODO: prefer $(LN_S) for everything, but gradle doesn't seem to like symbolic links for assets

$(abs_builddir)/%: $(abs_srcdir)/%
	@$(MKDIR_P) $(dir $@)
	$(LN_S) $< $@

SDL_SOURCE_DIR=$(abs_builddir)/sdl-src
$(abs_builddir)/app/src/main/java/org/libsdl: $(SDL_SOURCE_DIR)
	@$(MKDIR_P) $(dir $@)
	$(LN_S) $(SDL_SOURCE_DIR)/android-project/app/src/main/java/org/libsdl $@

$(APK_ASSETS_DIR)/%: $(abs_top_srcdir)/data/bg/%
	@$(MKDIR_P) $(dir $@)
	$(INSTALL) $< $@

$(APK_ASSETS_DIR)/%: $(abs_top_srcdir)/data/%
	@$(MKDIR_P) $(dir $@)
	$(INSTALL) $< $@

$(APK_ASSETS_DIR)/%: $(abs_top_srcdir)/usecode/ucxt/data/%
	@$(MKDIR_P) $(dir $@)
	$(INSTALL) $< $@

$(APK_ASSETS_DIR)/%: $(abs_top_builddir)/usecode/ucxt/data/%
	@$(MKDIR_P) $(dir $@)
	$(INSTALL) $< $@

# In addition to building the SDL C library for NDK, we also need Java
# sources compiled into the app.  Rather than let the NDK CMake pull
# in SDL like the other dependencies, pre-load the codebase here at
# the top level so that we have access to the Java sources too.
# The NDK build also utilizes this codebase rather than downloading a
# separate copy.
$(SDL_SOURCE_DIR):
	$(GIT) clone --branch release-2.0.14 --depth 1 https://github.com/libsdl-org/SDL.git $@
