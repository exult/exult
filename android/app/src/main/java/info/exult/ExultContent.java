/*
 *  Copyright (C) 2021-2022  The Exult Team
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

package info.exult;

import android.content.ContentResolver;
import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.util.ArraySet;
import android.util.Log;
import java.io.BufferedInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.nio.file.StandardCopyOption;
import java.util.ArrayDeque;
import java.util.Deque;
import java.util.Set;
import org.apache.commons.compress.archivers.ArchiveEntry;
import org.apache.commons.compress.archivers.ArchiveException;
import org.apache.commons.compress.archivers.ArchiveInputStream;
import org.apache.commons.compress.archivers.ArchiveStreamFactory;
import org.apache.commons.compress.compressors.CompressorException;
import org.apache.commons.compress.compressors.CompressorStreamFactory;

/// Base class for managing Exult content in the Android app
public abstract class ExultContent {
  private final String m_type;
  private final String m_name;
  private final ContentResolver m_contentResolver;
  private final SharedPreferences m_sharedPreferences;
  private static final String MANIFEST_KEY = "manifest";
  private Set<String> m_manifest;

  public interface ProgressReporter {
    public void report(String stage, String file);
  }

  public interface DoneReporter {
    public void report(boolean successful, String details);
  }

  protected ExultContent(String type, String name, Context context) {
    m_type = type;
    m_name = name;
    m_contentResolver = context.getContentResolver();
    m_sharedPreferences =
        context.getSharedPreferences("content." + type + "." + name, Context.MODE_PRIVATE);
    m_manifest = m_sharedPreferences.getStringSet(MANIFEST_KEY, null);
  }

  public String getType() {
    return m_type;
  }

  public String getName() {
    return m_name;
  }

  public boolean isInstalled() {
    return null != m_manifest;
  }

  public void install(Uri sourceUri, ProgressReporter progressReporter, DoneReporter doneReporter)
      throws ArchiveException, FileNotFoundException, IOException {
    boolean result = false;
    String details = null;
    if (isInstalled()) {
      details = "Already installed.";
    } else {
      m_manifest = new ArraySet<String>();
      ArchiveEntryVisitor identifyVisitor =
          (location, archiveEntry, inputStream) -> {
            return identify(location, archiveEntry, inputStream);
          };
      ArchiveEntryVisitor installVisitor =
          (location, archiveEntry, inputStream) -> {
            return install(location, archiveEntry, inputStream);
          };
      if (!traverse(sourceUri, identifyVisitor, "Verifying...", progressReporter)) {
        details = "Invalid/unrecognized archive.";
      } else if (!traverse(sourceUri, installVisitor, "Installing...", progressReporter)) {
        details = "Installation failed.";
      } else {
        result = true;
      }
    }

    progressReporter.report(null, null);
    doneReporter.report(result, details);
  }

  public boolean uninstall(ProgressReporter progressReporter) throws IOException {
    if (!isInstalled()) {
      // Already not installed.
      return true;
    }

    // First try to delete all the files.  For simplicity, not bothering to clean up empty
    // directories.
    // If there are problems deleting any individual files, we'll still proceed with the uninstall,
    // but return
    // false to indicate that it was not a clean uninstall.
    boolean cleanUninstall = true;
    for (String pathString : m_manifest) {
      if (null != progressReporter) {
        progressReporter.report("Uninstalling...", pathString);
      }
      Path path = Paths.get(pathString);
      if (Files.exists(path) && !Files.isDirectory(path)) {
        if (!Files.deleteIfExists(path)) {
          Log.d("ExultContent", "unable to delete " + path);
          cleanUninstall = false;
        }
      }
    }

    // Clear the manifest.
    SharedPreferences.Editor sharedPreferencesEditor = m_sharedPreferences.edit();
    sharedPreferencesEditor.remove(MANIFEST_KEY).commit();
    m_manifest = null;

    if (null != progressReporter) {
      // Clear the progress reporter
      progressReporter.report(null, null);
    }

    return cleanUninstall;
  }

  protected abstract boolean identify(
      Path location, ArchiveEntry archiveEntry, InputStream inputStream) throws IOException;

  protected abstract Path getContentRootInArchive();

  protected abstract Path getContentInstallRoot();

  protected Path getInstallDestination(Path location, ArchiveEntry archiveEntry) {
    // Skip over directories.
    if (archiveEntry.isDirectory()) {
      return null;
    }

    // If we haven't identified a data root, then we can't determine the installation destination.
    Path contentRootInArchive = getContentRootInArchive();
    if (null == contentRootInArchive) {
      return null;
    }

    // Skip over archive entries that fall outside the game root.
    Path fullPath = getFullArchivePath(location, archiveEntry);
    Path parent = fullPath.getParent();
    boolean nullParent = null == parent;
    boolean emptyRoot = contentRootInArchive.toString().isEmpty();
    if (!emptyRoot && (nullParent || !fullPath.startsWith(contentRootInArchive))) {
      return null;
    }

    // Strip off the root.
    Path relativePath = contentRootInArchive.relativize(fullPath);

    // Map into local install dir
    return getContentInstallRoot().resolve(relativePath);
  }

  private boolean install(Path location, ArchiveEntry entry, InputStream inputStream)
      throws IOException {
    // Null entry indicates this is the terminal call after completing the DFS traversal of the
    // content.
    if (null == entry) {
      // For an install pass, the terminal call is where we record the manifest and return true to
      // indicate a
      // successful installation.
      SharedPreferences.Editor sharedPreferencesEditor = m_sharedPreferences.edit();
      sharedPreferencesEditor.putStringSet(MANIFEST_KEY, m_manifest).commit();
      return true;
    }

    // Don't try to install directory entries; we'll create them when we have content to populate
    // them with.
    if (entry.isDirectory()) {
      return false;
    }

    // If this entry doesn't have a destination, move on.
    Path destination = getInstallDestination(location, entry);
    if (null == destination) {
      return false;
    }

    // Create any missing directories.
    Path parent = destination.getParent();
    if (null != parent) {
      Files.createDirectories(parent);
    }

    // Install the file.
    Files.copy(inputStream, destination, StandardCopyOption.REPLACE_EXISTING);

    // Record this file in the manifest for the content.
    m_manifest.add(destination.toString());

    return false;
  }

  private interface ArchiveEntryVisitor {
    public boolean accept(Path location, ArchiveEntry archiveEntry, InputStream inputStream)
        throws IOException;
  }

  private boolean traverse(
      Uri sourceUri, ArchiveEntryVisitor visitor, String stage, ProgressReporter progressReporter)
      throws ArchiveException, FileNotFoundException, IOException {
    class LocationAndArchive {
      LocationAndArchive(Path location, ArchiveInputStream archive) {
        this.location = location;
        this.archive = archive;
      }

      Path location;
      ArchiveInputStream archive;
    }
    ;
    Deque<LocationAndArchive> archiveStack = new ArrayDeque<LocationAndArchive>();
    InputStream inputStream = m_contentResolver.openInputStream(sourceUri);
    BufferedInputStream bufferedInputStream = new BufferedInputStream(inputStream);
    ArchiveStreamFactory archiveStreamFactory = new ArchiveStreamFactoryWithXar();
    CompressorStreamFactory compressorStreamFactory = new CompressorStreamFactory();

    // Set up the initial top-level archive in the stack to start processing.
    archiveStack.push(
        new LocationAndArchive(
            null, archiveStreamFactory.createArchiveInputStream(bufferedInputStream)));

    // Proceed with DFS archive processing until the visitor terminates the search.
    while (!archiveStack.isEmpty()) {
      LocationAndArchive locationAndArchive = archiveStack.pop();
      ArchiveEntry archiveEntry = null;

      // Process all the entries in the current archive.
      while (null != (archiveEntry = locationAndArchive.archive.getNextEntry())) {

        // Visit this entry and check if we can terminate the traversal.
        if (null != progressReporter) {
          progressReporter.report(stage, archiveEntry.getName());
        }
        if (visitor.accept(locationAndArchive.location, archiveEntry, locationAndArchive.archive)) {
          return true;
        }

        // If this is a directory, there's nothing more to do here; the subsequent archive entries
        // will
        // descend into the subdirectories.
        if (archiveEntry.isDirectory()) {
          continue;
        }

        // This is a non-directory entry; see if it is something we can decompress.
        InputStream nestedInputStream = new BufferedInputStream(locationAndArchive.archive);
        try {
          nestedInputStream =
              new BufferedInputStream(
                  compressorStreamFactory.createCompressorInputStream(nestedInputStream));
        } catch (CompressorException e) {
          // Ignore exception; this was a speculative attempt to decompress.
        }

        // Irrespective of whether we are decompressing, see if this is a nested archive we can
        // descend into.
        ArchiveInputStream nestedArchiveInputStream = null;
        try {
          nestedArchiveInputStream =
              archiveStreamFactory.createArchiveInputStream(nestedInputStream);
        } catch (ArchiveException e) {
          // Ignore exception; this was a speculative attempt to extract an archive.
        }

        // If it's not any sort of archive, there's nothing to descend into an we can move on.
        if (null == nestedArchiveInputStream) {
          continue;
        }

        // This looks like a nested archive; descend into it.
        archiveStack.push(locationAndArchive);
        Path nestedArchivePath = getFullArchivePath(locationAndArchive.location, archiveEntry);
        locationAndArchive = new LocationAndArchive(nestedArchivePath, nestedArchiveInputStream);
      }
    }

    // Make a terminating call to the visitor in case it has any final steps or mitigations to
    // attempt.
    return visitor.accept(null, null, null);
  }

  protected static Path getFullArchivePath(Path location, ArchiveEntry archiveEntry) {
    // Full path includes concatenating location and the archive entry.  Note that the archiveEntry
    // name may
    // include subdirectories.
    if (null == location) {
      return Paths.get(archiveEntry.getName());
    }
    return location.resolve(archiveEntry.getName());
  }
}
