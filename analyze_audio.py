import soundfile as sf
import numpy as np
import sys

def analyze_diff(file1, file2):
    print(f"Loading files...")
    print(f"Original: {file1}")
    print(f"Processed: {file2}")
    
    # Load audio
    data1, sr1 = sf.read(file1)
    data2, sr2 = sf.read(file2)
    
    if sr1 != sr2:
        print(f"Sample rate mismatch: {sr1} vs {sr2}")
        return
        
    print(f"Sample Rate: {sr1} Hz")
    
    # Make them same length if they differ slightly
    min_len = min(len(data1), len(data2))
    data1 = data1[:min_len]
    data2 = data2[:min_len]
    
    # Ensure stereo handling
    if len(data1.shape) == 1:
        data1 = np.column_stack((data1, data1))
    if len(data2.shape) == 1:
        data2 = np.column_stack((data2, data2))
        
    # Calculate difference (Delta)
    delta = data1 - data2
    
    # Global statistics
    max_diff = np.max(np.abs(delta))
    rms_diff = np.sqrt(np.mean(delta**2))
    
    print("\n--- GLOBAL DIFFERENCE STATS ---")
    print(f"Max Absolute Difference: {max_diff:.6f} ({20 * np.log10(max_diff + 1e-10):.1f} dBFS)")
    print(f"RMS Difference (Total energy removed): {rms_diff:.6f} ({20 * np.log10(rms_diff + 1e-10):.1f} dBFS)")
    
    if rms_diff < 1e-7:
        print("Conclusion: The files are essentially identical. No processing detected.")
        return

    # 1. Click Detection in Delta
    # A click in the delta is a sudden high-amplitude spike.
    # We look for samples in delta that exceed a certain threshold.
    spike_threshold = 0.05  # -26 dBFS spike
    spikes_l = np.sum(np.abs(delta[:, 0]) > spike_threshold)
    spikes_r = np.sum(np.abs(delta[:, 1]) > spike_threshold)
    print("\n--- TRANSIENT / CLICK ANALYSIS ---")
    print(f"High-energy spikes removed (L): {spikes_l}")
    print(f"High-energy spikes removed (R): {spikes_r}")
    if spikes_l > 10 or spikes_r > 10:
        print("-> Conclusion: De-clicking WAS applied. Distinct sharp transients were removed.")
    else:
        print("-> Conclusion: No aggressive de-clicking detected (or clicks were very soft).")

    # 2. Frequency Analysis of the Delta (to detect EQ, Rumble filter, or Noise reduction)
    print("\n--- FREQUENCY SPECTRUM OF REMOVED AUDIO ---")
    # Take a chunk in the middle to analyze frequencies (e.g., 10 seconds)
    mid_start = len(delta) // 2
    chunk_len = min(sr1 * 10, len(delta) - mid_start)
    delta_chunk = delta[mid_start:mid_start+chunk_len, 0] # Analyze Left channel
    
    # Simple FFT
    fft_res = np.abs(np.fft.rfft(delta_chunk))
    freqs = np.fft.rfftfreq(chunk_len, 1/sr1)
    
    # Calculate energy in bands
    bands = {
        "Sub-Bass / Rumble (0-30 Hz)": (0, 30),
        "Bass (30-100 Hz)": (30, 100),
        "Midrange (100-2000 Hz)": (100, 2000),
        "Highs / Hiss (2000-20000 Hz)": (2000, 20000)
    }
    
    total_fft_energy = np.sum(fft_res**2) + 1e-10
    
    for name, (low, high) in bands.items():
        idx = np.where((freqs >= low) & (freqs < high))[0]
        band_energy = np.sum(fft_res[idx]**2)
        pct = (band_energy / total_fft_energy) * 100
        print(f"{name}: {pct:.1f}% of removed energy")

    if bands["Sub-Bass / Rumble (0-30 Hz)"][1] and (np.sum(fft_res[np.where((freqs >= 0) & (freqs < 30))[0]]**2) / total_fft_energy) > 0.4:
         print("-> Conclusion: A Rumble filter / High-Pass filter WAS applied.")
    elif (np.sum(fft_res[np.where((freqs >= 2000) & (freqs < 20000))[0]]**2) / total_fft_energy) > 0.5:
         print("-> Conclusion: High-frequency Noise Reduction (De-hiss) WAS applied.")
    elif rms_diff > 1e-4 and spikes_l < 10:
         print("-> Conclusion: A broad EQ or phase-shift occurred (possibly a phase-leaky filter or broad noise reduction).")

if __name__ == "__main__":
    f1 = r"C:\Users\Administrator\Music\VINYL\test Kiril - Critical Presents- Systems 012 [Critical Music - CRITSYS012][2018][FLAC 24 VINYL]\A1 - Kiril - Reload.flac"
    f2 = r"C:\Users\Administrator\Music\VINYL\test Kiril - Critical Presents- Systems 012 [Critical Music - CRITSYS012][2018][FLAC 24 VINYL]\1 A1 - Kiril - Reload.flac"
    analyze_diff(f1, f2)
