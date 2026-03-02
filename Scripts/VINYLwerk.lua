--[[
 * ReaScript Name: VINYLwerk
 * Description: Audio Restoration integration for REAPER (Click removal, AI Denoise, EQ)
 * Author: flarkAUDIO
 * Version: 1.0.0
--]]

-- Configuration
local cli_exec = "vinylwerk_cli" -- Assumes it's in the system PATH or alongside the script
local os_name = reaper.GetOS()
if os_name == "Win32" or os_name == "Win64" then
    cli_exec = "vinylwerk_cli.exe"
end

-- Print helper
function msg(m)
    reaper.ShowConsoleMsg(tostring(m) .. "
")
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

-- Get User Inputs (Simple Native GUI for now, can be upgraded to ReaImGui)
local retval, retvals_csv = reaper.GetUserInputs("VINYLwerk Settings", 4,
    "Click Sensitivity (0-100),Noise Reduction (dB),Rumble Filter (Hz),Hum Filter (Hz)",
    "60,0.0,20.0,60.0")

if not retval then return end -- User canceled

local click_sens, noise_red, rumble, hum = retvals_csv:match("([^,]+),([^,]+),([^,]+),([^,]+)")

-- Process Item function
function process_item()
    -- 1. Create a temporary output file path
    local source = reaper.GetMediaItemTake_Source(take)
    local source_file = reaper.GetMediaSourceFileName(source, "")
    
    if source_file == "" then
        reaper.MB("Source file not found or unsupported.", "Error", 0)
        return
    end

    local out_file = source_file:gsub("%.([^%.]+)$", "_restored.%1")
    
    -- 2. Build the command line
    -- E.g.: vinylwerk_cli "input.wav" "output.wav" --click-sens 60 --noise-red 0.0 --rumble 20.0 --hum 60.0
    local cmd = string.format('"%s" "%s" "%s" --click-sens %s --noise-red %s --rumble %s --hum %s',
        cli_exec, source_file, out_file, click_sens, noise_red, rumble, hum)
        
    msg("Running VINYLwerk...")
    msg(cmd)
    
    -- 3. Execute the CLI tool
    local result = reaper.ExecProcess(cmd, 0)
    
    -- 4. Import the processed file as a new take
    if result then
        reaper.AddTakeToMediaItem(item)
        local new_take = reaper.GetActiveTake(item)
        local new_source = reaper.PCM_Source_CreateFromFile(out_file)
        reaper.SetMediaItemTake_Source(new_take, new_source)
        reaper.UpdateItemInProject(item)
        reaper.MB("Audio restoration complete!", "VINYLwerk", 0)
    else
        reaper.MB("Error executing VINYLwerk CLI. Is it installed?", "Error", 0)
    end
end

reaper.Undo_BeginBlock()
process_item()
reaper.Undo_EndBlock("VINYLwerk Processing", -1)
reaper.UpdateArrange()
