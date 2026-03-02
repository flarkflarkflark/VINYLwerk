--[[
 * ReaScript Name: VINYLwerk
 * Description: Audio Restoration integration for REAPER (Click removal, AI Denoise, EQ)
 * Author: flarkAUDIO
 * Version: 1.0.2
 * About:
 *   Professional audio restoration toolkit.
 *   Includes click removal, spectral noise reduction, and vintage filtering.
 * Provides:
 *   [main] .
 *   [windows64] vinylwerk_cli.exe
 *   [linux64] vinylwerk_cli
 *   [macos] vinylwerk_cli
--]]

-- Get script path
local info = debug.getinfo(1, "S")
local script_path = info.source:sub(2):match("(.*[\\\\/])")

-- Configuration
local cli_name = "vinylwerk_cli"
local os_name = reaper.GetOS()
if os_name == "Win32" or os_name == "Win64" then
    cli_name = "vinylwerk_cli.exe"
end

local cli_exec = script_path .. cli_name

-- Check if binary exists
local f = io.open(cli_exec, "r")
if f then 
    f:close() 
else
    reaper.MB("VINYLwerk CLI backend not found!\n\nExpected at: " .. cli_exec .. "\n\nPlease ensure you installed the backend correctly.", "Error", 0)
    return
end

-- Print helper
function msg(m)
    reaper.ShowConsoleMsg(tostring(m) .. "\n")
end

-- Check selection
local item = reaper.GetSelectedMediaItem(0, 0)
if not item then
    reaper.MB("Please select an audio item to process.", "VINYLwerk", 0)
    return
end

local take = reaper.GetActiveTake(item)
if not take or reaper.TakeIsMIDI(take) then
    reaper.MB("Selected item must be an audio take.", "VINYLwerk", 0)
    return
end

-- Get User Inputs
local retval, retvals_csv = reaper.GetUserInputs("VINYLwerk Settings", 4,
    "Click Sensitivity (0-100),Noise Reduction (dB),Rumble Filter (Hz),Hum Filter (Hz)",
    "60,0.0,20.0,60.0")

if not retval then return end -- User canceled

local click_sens, noise_red, rumble, hum = retvals_csv:match("([^,]+),([^,]+),([^,]+),([^,]+)")

-- Process Item function
function process_item()
    local source = reaper.GetMediaItemTake_Source(take)
    local source_file = reaper.GetMediaSourceFileName(source, "")
    
    if source_file == "" then
        reaper.MB("Source file not found or unsupported.", "Error", 0)
        return
    end

    local out_file = source_file:gsub("%.([^%.]+)$", "_restored.%%1")
    
    -- Build command
    local cmd = string.format("\"%s\" \"%s\" \"%s\" --click-sens %s --noise-red %s --rumble %s --hum %s",
        cli_exec, source_file, out_file, click_sens, noise_red, rumble, hum)
        
    msg("Running VINYLwerk...")
    
    -- Execute
    local result = reaper.ExecProcess(cmd, 0)
    
    -- Import as new take
    if result ~= nil then
        reaper.AddTakeToMediaItem(item)
        local new_take = reaper.GetActiveTake(item)
        local new_source = reaper.PCM_Source_CreateFromFile(out_file)
        reaper.SetMediaItemTake_Source(new_take, new_source)
        reaper.UpdateItemInProject(item)
    else
        reaper.MB("Error executing VINYLwerk CLI.", "Error", 0)
    end
end

reaper.Undo_BeginBlock()
process_item()
reaper.Undo_EndBlock("VINYLwerk Processing", -1)
reaper.UpdateArrange()
