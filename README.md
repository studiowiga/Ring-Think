# Ring Think (Studio Wiga) - VST3 (Windows + macOS) and AU (macOS) - personal use

## Repository layout (everything at the ROOT of the repo, no sub-folder except .github)
    CMakeLists.txt
    PluginProcessor.h / PluginProcessor.cpp
    PluginEditor.h / PluginEditor.cpp
    background.png
    pitchshift_white.png
    pitchshift_yellow.png
    .github/workflows/build.yml

## Get the .vst3
1. Create a NEW empty GitHub repository, then "Add file > Upload files" and drop the files above.
2. Create .github/workflows/build.yml by hand if the upload skipped the hidden folder
   (Add file > Create new file, type the path .github/workflows/build.yml and paste the file content).
3. Actions tab > "Build Windows + macOS" > open the green run > Artifacts:
   - RingThink-VST3-Windows  (Windows)
   - RingThink-macOS-VST3-AU (macOS: universal Apple Silicon + Intel)

### Windows
Unzip, copy "Ring Think.vst3" to C:\Program Files\Common Files\VST3\ and rescan plugins in your DAW.

### macOS
1. Unzip the artifact. Copy "Ring Think.vst3" to /Library/Audio/Plug-Ins/VST3/
   and "Ring Think.component" (Audio Unit, for Logic/GarageBand) to /Library/Audio/Plug-Ins/Components/
   (or the same folders inside your user ~/Library).
2. The plugin is not notarized by Apple, so macOS blocks downloaded files. Open Terminal and run once:
       xattr -cr "/Library/Audio/Plug-Ins/VST3/Ring Think.vst3"
       xattr -cr "/Library/Audio/Plug-Ins/Components/Ring Think.component"
3. Rescan plugins in your DAW (Logic: restart it; AU validation runs automatically).

## User presets
Windows: %APPDATA%\WigaRingThink\user_presets.xml
macOS: ~/Library/WigaRingThink/user_presets.xml
(shared by every project).

## v2.2.2
- The plugin now opens on the "Ring Mod" sound (knob defaults were giving a ~15 Hz carrier, heard as a tremolo).
- Pitch shift (Sub Octave, Octave Up, Detune...): the grain window now locks onto the period of the played note, so the shifted note is in tune and no longer beats or wobbles. Chords and silence fall back to the previous fixed window.

## Files (v2.2.2)
All the PNG files at the repo root are the pedal artwork (background, knob caps, LEDs, labels...) and are embedded in the plugin by CMakeLists.txt: upload them all.
