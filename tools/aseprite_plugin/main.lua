--[[
 *
 *  main.lua - Aseprite plugin script for SHP files
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
 *
 ]]

local pluginName = "exult-shp"
local pluginDir = app.fs.joinPath(app.fs.userConfigPath, "extensions", pluginName)
local converterPath = app.fs.joinPath(pluginDir, "exult_shp")

-- Debug system with toggle
local debugEnabled = false  -- toggle debug messages

local function debug(message)
  if debugEnabled then
    print("[Exult SHP] " .. message)
  end
end

local function logError(message)
  -- Always print errors regardless of debug setting
  print("[Exult SHP ERROR] " .. message)
end

-- Global utility function for quoting paths with spaces
function quoteIfNeeded(path)
  if path:find(" ") then
    return '"' .. path .. '"'
  else
    return path
  end
end

-- Helper to run commands with hidden output
function executeHidden(cmd)
  -- For debugging, run raw command instead with output captured
  if debugEnabled then
    debug("Executing with output capture: " .. cmd)
    local tmpFile = app.fs.joinPath(app.fs.tempPath, "exult-shp-output-" .. os.time() .. ".txt")

    -- Add output redirection to file
    local redirectCmd
    if app.fs.pathSeparator == "\\" then
      -- Windows
      redirectCmd = cmd .. " > " .. quoteIfNeeded(tmpFile) .. " 2>&1"
    else
      -- Unix-like (macOS, Linux)
      redirectCmd = cmd .. " > " .. quoteIfNeeded(tmpFile) .. " 2>&1"
    end

    -- Execute the command
    local success = os.execute(redirectCmd)

    -- Read and log the output
    if app.fs.isFile(tmpFile) then
      local file = io.open(tmpFile, "r")
      if file then
        debug("Command output:")
        local output = file:read("*all")
        debug(output or "<no output>")
        file:close()
      end
    end

    return success
  else
    -- Check operating system and add appropriate redirection
    local redirectCmd
    if app.fs.pathSeparator == "\\" then
      -- Windows
      redirectCmd = cmd .. " > NUL 2>&1"
    else
      -- Unix-like (macOS, Linux)
      redirectCmd = cmd .. " > /dev/null 2>&1"
    end

    return os.execute(redirectCmd)
  end
end

debug("Plugin initializing...")
debug("Temp path: " .. app.fs.tempPath)
debug("User config path: " .. app.fs.userConfigPath)
debug("Converter expected at: " .. converterPath)

-- Check if converterPath exists with OS-specific extension
if not app.fs.isFile(converterPath) then
  debug("Converter not found, checking for extensions...")
  if app.fs.isFile(converterPath..".exe") then
    converterPath = converterPath..".exe"
    debug("Found Windows converter: " .. converterPath)
  elseif app.fs.isFile(converterPath..".bin") then
    converterPath = converterPath..".bin"
    debug("Found binary converter: " .. converterPath)
  end
end

-- Verify converter exists at startup
local converterExists = app.fs.isFile(converterPath)
debug("Converter exists: " .. tostring(converterExists))

-- Check if converter is executable and only chmod if needed (Unix-like systems only)
-- Installing the plug-in might unset the executable bit
if converterExists and app.fs.pathSeparator == "/" then
  -- Check if a file is executable
  local function isExecutable(path)
    local cmd = "test -x " .. quoteIfNeeded(path)
    local result = os.execute(cmd)
    -- os.execute returns different values based on Lua version
    -- In Lua 5.2+, it returns success, exit_type, code
    -- In Lua 5.1, it returns the exit code
    if type(result) == "boolean" then
      return result
    else
      return result == 0
    end
  end

  -- Only chmod if the file exists but isn't executable
  if not isExecutable(converterPath) then
    debug("Converter found but not executable, setting permissions")
    local chmodCmd = "chmod +x " .. quoteIfNeeded(converterPath)
    executeHidden(chmodCmd)
  end
end

-- Error display helper
function showError(message)
  logError(message)
  app.alert{
    title="Exult SHP Error",
    text=message
  }
end

-- Don't detect Animation sequences when opening files
function disableAnimationDetection()
  -- Store the original preference value if it exists
  if app.preferences and app.preferences.open_file and app.preferences.open_file.open_sequence ~= nil then
    _G._originalSeqPref = app.preferences.open_file.open_sequence
    -- Set to 2 which means "skip the prompt without loading as animation"
    app.preferences.open_file.open_sequence = 2
  end
end

function restoreAnimationDetection()
  -- Restore the original preference if we saved it
  if app.preferences and app.preferences.open_file and _G._originalSeqPref ~= nil then
    app.preferences.open_file.open_sequence = _G._originalSeqPref
  end
end

-- File format registration function
function registerSHPFormat()
  if not converterExists then
    showError("SHP converter tool not found at:\n" .. converterPath .. 
              "\nSHP files cannot be opened until this is fixed.")
    return false
  end
  return true
end

function getCelOffsetData(cel)
  if not cel then return nil end

  -- Only extract from cel's user data
  if cel.data and cel.data:match("offset:") then
    local x, y = cel.data:match("offset:(%d+),(%d+)")
    if x and y then
      return {
        offsetX = tonumber(x),
        offsetY = tonumber(y)
      }
    end
  end

  return nil
end

function setCelOffsetData(cel, offsetX, offsetY)
  if not cel then return end

  -- Store offset directly in the cel's user data only
  cel.data = string.format("offset:%d,%d", offsetX or 0, offsetY or 0)
  debug("Stored offset data in cel user data: " .. cel.data)
end

-- Check for offset tags
function spriteHasCelOffsetData(sprite)
  -- Check if any cel has offset data stored
  for i, layer in ipairs(sprite.layers) do
    local cel = layer:cel(1)
    if cel and cel.data and cel.data:match("offset:") then
      return true
    end
  end
  return false
end

-- Read offset data from a metadata file
function readFrameMetadata(metaPath)
  local offsetX, offsetY = 0, 0

  if app.fs.isFile(metaPath) then
    local meta = io.open(metaPath, "r")
    if meta then
      for line in meta:lines() do
        local key, value = line:match("(.+)=(.+)")
        if key == "offset_x" then offsetX = tonumber(value) end
        if key == "offset_y" then offsetY = tonumber(value) end
      end
      meta:close()
    end
  end

  return offsetX, offsetY
end

-- Scan frames and return dimensions and metadata
function scanFramesForDimensions(basePath)
  local maxWidth = 0
  local maxHeight = 0
  local maxOffsetX = 0
  local maxOffsetY = 0
  local maxRightEdge = -999999
  local maxBottomEdge = -999999
  local frameIndex = 0
  local frameData = {}

  while true do
    local framePath = basePath .. "_" .. frameIndex .. ".png"
    local metaPath = basePath .. "_" .. frameIndex .. ".meta"

    if not app.fs.isFile(framePath) then break end

    local image = Image{fromFile=framePath}
    local offsetX, offsetY = readFrameMetadata(metaPath)

    -- Store frame data
    frameData[frameIndex] = {
      path = framePath,
      width = image.width,
      height = image.height,
      offsetX = offsetX,
      offsetY = offsetY
    }

    -- Track maximums
    maxOffsetX = math.max(maxOffsetX, offsetX)
    maxOffsetY = math.max(maxOffsetY, offsetY)

    -- Track extents
    local rightEdge = offsetX + image.width
    local bottomEdge = offsetY + image.height
    maxRightEdge = math.max(maxRightEdge, rightEdge)
    maxBottomEdge = math.max(maxBottomEdge, bottomEdge)

    frameIndex = frameIndex + 1
  end

  return {
    frameCount = frameIndex,
    frameData = frameData,
    maxWidth = maxRightEdge,
    maxHeight = maxBottomEdge,
    maxOffsetX = maxOffsetX,
    maxOffsetY = maxOffsetY
  }
end

-- Position a cel based on offset
function positionCelWithOffset(cel, offsetX, offsetY, maxOffsetX, maxOffsetY)
  if not cel then return end

  -- Calculate position based on offset from top-left
  local adjustedX = maxOffsetX - offsetX
  local adjustedY = maxOffsetY - offsetY

  -- Set position
  cel.position = Point(adjustedX, adjustedY)

  -- Store offset data
  setCelOffsetData(cel, offsetX, offsetY)

  return adjustedX, adjustedY
end

-- Create and position a frame cel with proper metadata
function addFrameCel(sprite, layerIndex, frameImage, offsetX, offsetY, maxOffsetX, maxOffsetY)
  -- Create layer if needed (first layer already exists)
  local layer
  if layerIndex == 0 then
    layer = sprite.layers[1]
    layer.name = "Frame 0"
    -- Delete existing cel if present
    if layer:cel(1) then
      sprite:deleteCel(layer, 1)
    end
  else
    layer = sprite:newLayer()
    layer.name = "Frame " .. layerIndex
  end

  -- Calculate position
  local adjustedX = maxOffsetX - offsetX
  local adjustedY = maxOffsetY - offsetY

  -- Create new cel with correct position
  local cel = sprite:newCel(layer, 1, frameImage, Point(adjustedX, adjustedY))

  -- Store offset data
  setCelOffsetData(cel, offsetX, offsetY)

  debug("Positioned frame " .. layerIndex .. " at (" .. adjustedX .. "," .. adjustedY .. ") with dimensions " .. 
        frameImage.width .. "x" .. frameImage.height)
        
  return cel
end

-- Handle exporting a single frame
function exportSingleFrame(tempDir, layer, frameIndex, layerOffsets, metaFile)
  local filepath = app.fs.joinPath(tempDir, "frame" .. frameIndex .. ".png")
  debug("Exporting layer " .. frameIndex .. " as frame " .. frameIndex)
  
  -- Get the cel for this layer
  local cel = layer:cel(1)
  if not cel then return false end

  -- Get just the cel image (not including position)
  local celImage = cel.image

  -- Write the image directly to a file
  celImage:saveAs(filepath)

  -- Verify the file was created
  if not app.fs.isFile(filepath) then
    debug("WARNING: Failed to export frame " .. frameIndex)
    return false
  end

  -- Get offset from cel data
  local offsetData = getCelOffsetData(cel)
  local offsetX = offsetData and offsetData.offsetX or 0
  local offsetY = offsetData and offsetData.offsetY or 0

  -- Write to metadata file
  metaFile:write(string.format("frame%d_offset_x=%d\n", frameIndex, offsetX))
  metaFile:write(string.format("frame%d_offset_y=%d\n", frameIndex, offsetY))

  debug("Exported frame " .. frameIndex .. " with dimensions " .. 
        celImage.width .. "x" .. celImage.height .. 
        " and offsets: " .. offsetX .. "," .. offsetY)

  return true
end

function processImport(shpFile, paletteFile, outputBasePath, createSeparateFrames)
  if not converterExists then
    showError("SHP converter not found at: " .. converterPath)
    return false
  end

  debug("Importing SHP: " .. shpFile)
  debug("Palette: " .. (paletteFile ~= "" and paletteFile or "default"))
  debug("Output: " .. outputBasePath)

  -- Check if file exists
  if not app.fs.isFile(shpFile) then
    showError("SHP file not found: " .. shpFile)
    return false
  end

  -- Extract base filename from the SHP file (without path and extension)
  local shpBaseName = shpFile:match("([^/\\]+)%.[^.]*$") or "output"
  shpBaseName = shpBaseName:gsub("%.shp$", "")
  debug("Extracted SHP base name: " .. shpBaseName)

  -- Extract output directory from outputBasePath
  local outputDir = outputBasePath:match("(.*[/\\])") or ""

  -- Combine to get the actual path where files will be created
  local actualOutputBase = outputDir .. shpBaseName
  debug("Expected output base: " .. actualOutputBase)

  -- Create command - always use separate frames mode
  local cmd = quoteIfNeeded(converterPath) .. " import " .. quoteIfNeeded(shpFile) .. " " .. quoteIfNeeded(outputBasePath)

  -- Only add palette if it's not empty
  if paletteFile and paletteFile ~= "" then
    cmd = cmd .. " " .. quoteIfNeeded(paletteFile)
  end

  -- Always use separate frames
  cmd = cmd .. " separate"

  debug("Executing: " .. cmd)

  -- Execute command
  local success = executeHidden(cmd)

  -- Check for output files
  local firstFrame = actualOutputBase .. "_0.png"

  debug("Looking for first frame at: " .. firstFrame)
  debug("File exists: " .. tostring(app.fs.isFile(firstFrame)))

  if not app.fs.isFile(firstFrame) then
    debug("ERROR: Failed to convert SHP file")
    return false
  end

  -- Continue with loading the frames
  debug("Loading output files into Aseprite")

  -- Scan all frames once to get dimensions and metadata
  local framesInfo = scanFramesForDimensions(actualOutputBase)
  
  if framesInfo.frameCount == 0 then
    debug("ERROR: No frames found after conversion")
    return false
  end
  
  debug("Found " .. framesInfo.frameCount .. " frames, canvas size: " 
        .. framesInfo.maxWidth .. "x" .. framesInfo.maxHeight)

  -- Open and setup the sprite
  debug("Opening first frame: " .. framesInfo.frameData[0].path)

  -- Disable animation detection before opening
  disableAnimationDetection()
  local sprite = app.open(framesInfo.frameData[0].path)
  restoreAnimationDetection()

  if not sprite then
    showError("Failed to open first frame")
    return false
  end

  -- Set filename
  sprite.filename = shpFile

  -- Save original first frame dimensions and image before resizing
  local frame0 = framesInfo.frameData[0]
  local originalImage = Image(sprite.cels[1].image)
  debug("Saved original first frame image: " .. originalImage.width .. "x" .. originalImage.height)

  -- Resize the sprite to the calculated dimensions
  if sprite.width ~= framesInfo.maxWidth or sprite.height ~= framesInfo.maxHeight then
    debug("Resizing from " .. sprite.width .. "x" .. sprite.height .. 
          " to " .. framesInfo.maxWidth .. "x" .. framesInfo.maxHeight)
    sprite:resize(framesInfo.maxWidth, framesInfo.maxHeight, 0, 0)
  end

  -- Rename the first layer
  local firstLayer = sprite.layers[1]
  firstLayer.name = "Frame 1"

  -- Replace the first cel with the original image
  -- or the offset is lost
  sprite:deleteCel(firstLayer, 1)

  -- Create a new cel with the original image
  local adjustedX = framesInfo.maxOffsetX - frame0.offsetX
  local adjustedY = framesInfo.maxOffsetY - frame0.offsetY
  local firstCel = sprite:newCel(firstLayer, 1, originalImage, Point(adjustedX, adjustedY))

  -- Store offset data
  setCelOffsetData(firstCel, frame0.offsetX, frame0.offsetY)

  debug("Positioned first cel at (" .. adjustedX .. "," .. adjustedY .. ") with dimensions " .. 
        originalImage.width .. "x" .. originalImage.height)

  -- Process each frame
  for frameIndex = 0, framesInfo.frameCount - 1 do
    local frame = framesInfo.frameData[frameIndex]
    local frameImage

    -- For first frame, use the saved original image
    if frameIndex == 0 then
      frameImage = originalImage
    else
      frameImage = Image{fromFile=frame.path}
    end

    -- Add as layer/cel with proper positioning
    addFrameCel(sprite, frameIndex, frameImage, frame.offsetX, frame.offsetY,
                framesInfo.maxOffsetX, framesInfo.maxOffsetY)
  end

  -- Set layer edges to be visible after import
  if app.preferences then
    -- Use the correct document preferences API
    local docPref = app.preferences.document(sprite)
    if docPref and docPref.show then
      docPref.show.layer_edges = true
      debug("Enabled layer edges display for this document")
    else
      debug("Could not set layer_edges preference (show section not found in document preferences)")
    end
  else
    debug("Could not set layer_edges preference (preferences not available)")
  end

  return true, sprite
end

function importSHP(filename)
  -- Normal dialog flow for manual import
  local dlg = Dialog("Import SHP File")
  dlg:file{
    id="shpFile",
    label="SHP File:",
    title="Select SHP File",
    open=true,
    filetypes={"shp"},
    focus=true
  }
  dlg:file{
    id="paletteFile",
    label="Palette File (optional):",
    open=true
  }

  -- Store dialog result in outer scope
  local dialogResult = false
  local importSettings = {}

  dlg:button{
    id="import",
    text="Import",
    onclick=function()
      dialogResult = true
      importSettings.shpFile = dlg.data.shpFile
      importSettings.paletteFile = dlg.data.paletteFile
      dlg:close()
    end
  }

  dlg:button{
    id="cancel",
    text="Cancel",
    onclick=function()
      dialogResult = false
      dlg:close()
    end
  }

  -- Show dialog
  dlg:show()

  -- Handle result
  if not dialogResult then return end

  if not importSettings.shpFile or importSettings.shpFile == "" then
    showError("Please select an SHP file to import")
    return
  end

  -- Create temp directory for files
  local tempDir = app.fs.joinPath(app.fs.tempPath, "exult-shp-" .. os.time())
  app.fs.makeDirectory(tempDir)

  -- Prepare output file path
  local outputBasePath = app.fs.joinPath(tempDir, "output")
  
  return processImport(importSettings.shpFile, 
                      importSettings.paletteFile or "", 
                      outputBasePath, 
                      true)
end

function exportSHP()
  -- Get active sprite
  local sprite = app.activeSprite
  if not sprite then
    showError("No active sprite to export")
    return
  end

  -- Store original default extension preference
  local originalDefaultExt = nil
  if app.preferences and app.preferences.save_file then
    originalDefaultExt = app.preferences.save_file.default_extension
    -- Set SHP as default extension during export
    app.preferences.save_file.default_extension = "shp"
    debug("Temporarily set default image extension to SHP")
  end

  -- Check if sprite uses indexed color mode
  if sprite.colorMode ~= ColorMode.INDEXED then
    showError("SHP format needs an indexed palette. Convert your sprite to Indexed color mode first.")
    return
  end

  -- Default offset values for fallback
  local defaultOffsetX = math.floor(sprite.width / 2)
  local defaultOffsetY = math.floor(sprite.height / 2)

  -- Count how many layers already have offset data
  local layersWithOffsets = 0
  for _, layer in ipairs(sprite.layers) do
    local cel = layer:cel(1)
    if cel and getCelOffsetData(cel) then
      layersWithOffsets = layersWithOffsets + 1
    end
  end

  -- Show initial export dialog
  local dlg = Dialog("Export SHP File")
  dlg:file{
    id="outFile",
    label="Output SHP File:",
    save=true,
    filetypes={"shp"},
    focus=true
  }

  -- Show informational text about layer offsets
  if layersWithOffsets == #sprite.layers then
    dlg:label{
      id="allOffsetsSet",
      text="All layers have offset data. Ready to export."
    }
  elseif layersWithOffsets > 0 then
    dlg:label{
      id="someOffsetsSet",
      text=layersWithOffsets .. " of " .. #sprite.layers .. " layers have offset data. You'll be prompted for the rest."
    }
  else
    dlg:label{
      id="noOffsets",
      text="No layers have offset data. You can set them individually or use defaults."
    }
    dlg:check{
      id="useDefaultOffsets",
      text="Use 0,0 as offset for all layers",
      selected=false
    }
  end

  -- Add option to edit existing offsets
  if layersWithOffsets > 0 then
    dlg:check{
      id="editExisting",
      text="Edit existing offsets",
      selected=false
    }
  end
  -- Store dialog result in outer scope
  local dialogResult = false
  local exportSettings = {}

  dlg:button{
    id="export",
    text="Export",
    onclick=function()
      dialogResult = true
      exportSettings.outFile = dlg.data.outFile
      exportSettings.editExisting = dlg.data.editExisting
      if layersWithOffsets == 0 then
        exportSettings.useDefaultOffsets = dlg.data.useDefaultOffsets
      else
        exportSettings.useDefaultOffsets = false
      end
      dlg:close()
    end
  }

  dlg:button{
    id="cancel",
    text="Cancel",
    onclick=function()
      dialogResult = false
      dlg:close()
    end
  }

  -- Show dialog
  dlg:show()

  -- Handle result
  if not dialogResult then return end

  if not exportSettings.outFile or exportSettings.outFile == "" then
    showError("Please specify an output SHP file")
    return
  end

  if not converterExists then
    showError("SHP converter not found at: " .. converterPath)
    return
  end

  -- Create temp directory for files
  local tempDir = app.fs.joinPath(app.fs.tempPath, "exult-shp-" .. os.time())
  app.fs.makeDirectory(tempDir)

  -- Prepare file paths
  local metaPath = app.fs.joinPath(tempDir, "metadata.txt")
  local basePath = app.fs.joinPath(tempDir, "frame")

  -- Create metadata file
  local meta = io.open(metaPath, "w")
  meta:write(string.format("num_frames=%d\n", #sprite.layers))

  -- Track frame offsets - the index in this array is the export frame number
  local layerOffsets = {}

  -- First pass: Check which layers need offset prompts
  for i, layer in ipairs(sprite.layers) do
    local offsetNeeded = true

    -- Check for cel user data first
    local cel = layer:cel(1)
    local offsetData = cel and getCelOffsetData(cel)

    if offsetData and not exportSettings.editExisting then
      layerOffsets[i] = {
        x = offsetData.offsetX,
        y = offsetData.offsetY,
        fromData = true
      }
      offsetNeeded = false
      debug("Using existing offset from cel data for layer " .. i .. ": " .. offsetData.offsetX .. "," .. offsetData.offsetY)
    end

    -- Mark for prompting if still needed
    if offsetNeeded then
      layerOffsets[i] = {
        needsPrompt = true
      }
    end
  end

  -- Second pass: Prompt for missing offsets OR use defaults if selected
  if layersWithOffsets == 0 and exportSettings.useDefaultOffsets then
    debug("Using default offsets for all layers as requested.")
    for i, layer in ipairs(sprite.layers) do
      layerOffsets[i] = {
        x = defaultOffsetX,
        y = defaultOffsetY
      }
      -- Store the default value in the cel for future use and consistency
      local cel = sprite.layers[i]:cel(1)
      if cel then
        setCelOffsetData(cel, layerOffsets[i].x, layerOffsets[i].y)
      end
      debug("Applied default offset to layer " .. i .. ": " .. defaultOffsetX .. "," .. defaultOffsetY)
    end
  else -- Original logic: prompt if needed
    for i, layerData in ipairs(layerOffsets) do
      if layerData.needsPrompt then
        -- Make this layer visible and others invisible for visual reference
        for j, otherLayer in ipairs(sprite.layers) do
          otherLayer.isVisible = (j == i)
        end

        -- Create prompt dialog for this specific layer
        local layerDlg = Dialog("Layer Offset")

        -- Get cleaner name (without offset info)
        local cleanName = sprite.layers[i].name:gsub(" %[.*%]$", "")

        layerDlg:label{
          id="info",
          text="Set offset for " .. cleanName .. " (" .. i .. " of " .. #sprite.layers .. ")"
        }

        -- If we have existing data, use it as default
        local cel = sprite.layers[i]:cel(1)
        local existingData = cel and getCelOffsetData(cel)
        local initialX = defaultOffsetX
        local initialY = defaultOffsetY

        if existingData then
          initialX = existingData.offsetX
          initialY = existingData.offsetY
        end

        layerDlg:number{
          id="offsetX",
          label="Offset X:",
          text=tostring(initialX),
          decimals=0
        }

        layerDlg:number{
          id="offsetY",
          label="Offset Y:",
          text=tostring(initialY),
          decimals=0
        }

        local layerResult = false

        layerDlg:button{
          id="ok",
          text="OK",
          onclick=function()
            layerResult = true
            layerOffsets[i] = {
              x = layerDlg.data.offsetX,
              y = layerDlg.data.offsetY
            }
            layerDlg:close()
          end
        }

        -- Show dialog and wait for result
        layerDlg:show{wait=true}

        -- If user cancelled, use defaults or existing data
        if not layerResult then
          if existingData then
            layerOffsets[i] = {
              x = existingData.offsetX,
              y = existingData.offsetY
            }
          else
            layerOffsets[i] = {
              x = defaultOffsetX,
              y = defaultOffsetY
            }
          end
        end

        -- Store the value in the cel for future use
        local cel = sprite.layers[i]:cel(1)
        if cel then
          setCelOffsetData(cel, layerOffsets[i].x, layerOffsets[i].y)
        end
      end
    end
  end

  -- Restore all layers to visible
  for _, layer in ipairs(sprite.layers) do
    layer.isVisible = true
  end

  -- Now export each layer as a frame
  for i, layer in ipairs(sprite.layers) do
    local frameIndex = i - 1 -- Convert to 0-based for the SHP file
    exportSingleFrame(tempDir, layer, frameIndex, layerOffsets, meta)
  end

  -- Close metadata file
  meta:close()

  -- Create and execute the export command
  local cmd = quoteIfNeeded(converterPath) .. 
             " export " .. 
             quoteIfNeeded(basePath) .. 
             " " .. quoteIfNeeded(exportSettings.outFile) .. 
             " 0" ..
             " " .. layerOffsets[1].x ..
             " " .. layerOffsets[1].y ..
             " " .. quoteIfNeeded(metaPath)

  debug("Executing: " .. cmd)

  -- Execute command
  local success = executeHidden(cmd)

  -- Restore original default extension preference
  if app.preferences and app.preferences.save_file and originalDefaultExt ~= nil then
    app.preferences.save_file.default_extension = originalDefaultExt
    debug("Restored original default image extension")
  end

  if success then
    app.alert("SHP file exported successfully.")
  else
    showError("Failed to export SHP file.")
  end
end

-- Function to convert active sprite's palette to U7 style (transparent index 255)
local function convertPaletteToU7()
  debug("Conversion to U7 palette command initiated.")
  local sprite = app.sprite
  if not sprite then
    showError("No active sprite to convert.")
    return
  end

  if sprite.colorMode ~= ColorMode.INDEXED then
    showError("Sprite is not in Indexed color mode. Cannot convert palette.")
    return
  end

  app.transaction( "Convert to U7 Palette", function()
    debug("Changing pixel format to RGB...")
    app.command.ChangePixelFormat {
      ui = false,
      format = "rgb"
    }
    debug("Pixel format changed to RGB.")

    -- Using a slightly altered default U7 palette.
    -- The color cycle and opaque colors (idx 224 - 255) are set to full transparency
    -- to prevent matching these colors in the RGB->Indexed mode step.
    local u7PalettePath = app.fs.joinPath(pluginDir, "u7.pal") 
    debug("Attempting to load U7 palette from: " .. u7PalettePath)

    if not app.fs.isFile(u7PalettePath) then
      showError("u7.pal not found in the extension folder: " .. u7PalettePath .. "\nPlease ensure u7.pal is in the " .. pluginName .. " extension directory.")
      return
    end

    local u7Palette = Palette{ fromFile = u7PalettePath }
    if not u7Palette then
      showError("Failed to load u7.pal from: " .. u7PalettePath)
      return
    end
    debug("u7.pal loaded successfully.")

    sprite:setPalette(u7Palette)
    debug("Sprite's active palette has been set to the loaded u7.pal.")

    debug("Changing pixel format back to Indexed (should use current sprite palette)")
    app.command.ChangePixelFormat {
      ui = false,
      format = "indexed",
      dithering = "",
      rgmap = "default"
    }
    debug("Pixel format changed back to Indexed using u7.pal.")

    debug("Replacing color indices 0->255 and 87->0...")
    if sprite.spec.colorMode == ColorMode.INDEXED then
      for _, cel in ipairs(sprite.cels) do
        if cel then
          local image = cel.image
          local newImage = Image(image.spec) 
          newImage:drawImage(image, 0, 0) 
          for y = 0, newImage.height - 1 do
            for x = 0, newImage.width - 1 do
              local currentPixel = newImage:getPixel(x, y)
              if currentPixel == 0 then
                newImage:putPixel(x, y, 255)
              elseif currentPixel == 87 then 
                newImage:putPixel(x, y, 0)
              end
            end
          end
          cel.image = newImage
        end
      end
      debug("Color index 0 replaced with 255, and index 87 replaced with 0, across all cels.")
      debug("Setting sprite palette's transparentColor to 255.")
      sprite.transparentColor = 255
    else
      debug("Sprite is not in Indexed mode after conversion, skipping color replacement.")
    end

    app.refresh()
    app.alert("Sprite palette converted to U7 style (using u7.pal, transparent index 255).")
  end)
end


function init(plugin)
  debug("Initializing plugin...")

  -- Register file format first
  local formatRegistered = registerSHPFormat()
  debug("SHP format registered: " .. tostring(formatRegistered))

  -- Register commands
  plugin:newCommand{
    id="ImportSHP",
    title="Import U7 SHP...",
    group="file_import",
    onclick=function() importSHP() end
  }

  plugin:newCommand{
    id="ExportSHP",
    title="Export to U7 SHP...",
    group="file_export",
    onclick=exportSHP
  }

  plugin:newCommand{
    id = pluginName .. "ConvertU7Palette",
    title = "Convert to U7 palette...",
    group = "sprite_color",
    onclick = function()
      convertPaletteToU7()
    end,
    onenabled = function()
      return app.sprite and app.sprite.colorMode == ColorMode.INDEXED
    end
  }
end

return { init=init }