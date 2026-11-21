FILL MAKER @VERSION@
================

A generative MIDI drum fill instrument. Press GENERATE, hear it, regenerate or mutate it, edit
notes, then drag it into Live or export it. Anything from a plain snare roll to cross-rhythms,
ratchets and fills that fall apart on purpose.

Formats: VST3, AU and a standalone app for macOS (Apple silicon and Intel, macOS 11 or later),
VST3 for Windows (64-bit). Free and open source (AGPLv3). Source: @SOURCE_URL@

These builds aren't signed with a developer certificate, so each system needs one extra step.


INSTALL: MAC
------------
1. Copy "Fill Maker.component" to   ~/Library/Audio/Plug-Ins/Components/
   Copy "Fill Maker.vst3"      to   ~/Library/Audio/Plug-Ins/VST3/
   (In Finder: Go > Go to Folder..., then paste the path.)
2. Open Terminal and clear the download quarantine:
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"Fill Maker.component"
     xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"Fill Maker.vst3"
   (and the same for "Fill Maker.app" if you use the standalone app)
3. In your DAW, rescan plug-ins. In Live: Settings > Plug-Ins, turn on the AU and VST3 system
   folders, then Rescan.

If macOS says the plug-in "is damaged" or "can't be opened", step 2 was missed.


INSTALL: WINDOWS
----------------
1. Copy the "Fill Maker.vst3" folder to   C:\Program Files\Common Files\VST3\
2. In your DAW, rescan plug-ins. In Live: Options > Preferences > Plug-Ins, turn on the VST3 system
   folder, then Rescan.

If Windows SmartScreen warns about the file, choose "More info", then "Run anyway".


IN ABLETON LIVE (two MIDI tracks)
---------------------------------
Track A: Fill Maker. Put your groove's clip here too if you want groove-aware fills.
Track B: your drums. Set MIDI From to Track A, then "Fill Maker", and Monitor: In.
AUDITION plays the fill on the built-in preview voice. DRAG MIDI drops a fill into Live.
