# TICK Metronome - Live Automation Edition 🎸💡

**Disclaimer:** This is a customized fork of the excellent [TICK Metronome](https://github.com/talaviram/TICK) originally created by Tal Aviram. 

While the original TICK is a fantastic, cross-platform, sample-accurate metronome, this specific fork has been heavily modified and optimized for **Automated Live Performances**. It is designed to act as the "brain" in a real-time live rig, receiving commands from setlist apps (like SongBook) and sending rock-solid MIDI Beat Clocks to lighting software (like TouchDesigner).

## 🚀 Key Features in this Fork

### 1. SongBook / Setlist App Integration (Universal BPM routing)
To bypass the strict 127 maximum value limit of the MIDI protocol, this version uses a smart multi-channel system. It accepts **Program Change (PC)**, **Note On**, or **Control Change (CC)** to set the BPM directly, bypassing any DAW VST3 filtering:
* **MIDI Channel 10:** Value 1-99 sets the BPM directly to 1-99.
* **MIDI Channel 11:** Adds 100 to the value (e.g., Note 70 = 170 BPM).
* **MIDI Channel 12:** Adds 200 to the value (e.g., Note 70 = 270 BPM).
*(Channel 16 is completely ignored by the metronome, allowing it to be used exclusively for lighting/video cues).*

### 2. Jitter-Free MIDI Beat Clock Generator
This version generates a mathematically perfect, jitter-free **MIDI Beat Clock (24 PPQN)** directly from the audio thread's sample buffer. This clock is broadcasted out and can be used to drive external LFOs, lasers, and lighting cues in software like TouchDesigner with zero latency.

### 3. Hardware Tap-Tempo (With 3-Click Debounce)
Optimized for physical hardware (like Line 6 Helix footswitches). It listens for MIDI CC 68 (value > 64) and features a custom algorithm:
* **Debounce:** Ignores double-triggers occurring within 80ms.
* **3-Tap Rule:** Requires at least 3 consistent physical taps before updating the master BPM, preventing accidental tempo changes from "ghost" clicks on stage.

### 4. C++20 & Extreme DSP Optimization
* **C++20 Standard:** The codebase and CMake configuration have been updated to utilize C++20.
* **Zero-Allocation Audio Thread:** Cleaned up legacy C-style memory allocations in the audio processing block. The click-sample buffers are now pre-allocated and safely mixed, meaning this metronome will never cause CPU spikes or audio dropouts, no matter how heavily loaded the DAW project is.

## 🛠 Building the Plugin
* **Windows:** Simply run `build_win.bat` to generate the `.exe` installer via CMake and NSIS.

---

### Original Credits & License
All core metronome functionality, UI, and standard sample-playback architecture were created by Tal Aviram. This project is released under the MIT License. See `LICENSE.md` for details.
