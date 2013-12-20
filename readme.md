# MOTU `auexamine`

This application performs a thorough test on a Mac [Audio Unit](https://developer.apple.com/library/mac/documentation/MusicAudio/Conceptual/AudioUnitProgrammingGuide/Introduction/Introduction.html) plug-in to ensure it is free of common defects.


## Usage

`auexamine` can be run from a terminal with the following command:

    auexamine <au type> <au subtype> <au manufacturer> <[Optional] requires initialization (defaults true)> <[Optional] numeric arguments (defaults false)>

### Arguments
<dl>
  <dt>au type</dt>
  <dd>The type of the au (example: <code>aufx</code> for an audio unit effect)</dd>
  <dt>au subtype</dt>
  <dd>The manufacturer-defined subtype (example: <code>pmeq</code> for parametric eq.)</dd>
  <dt>au manufacturer</dt>
  <dd>The manufacturer code (example: <code>appl</code> for Apple, Inc.)</dd>
  <dt>requires initialization</dt>
  <dd>An optional numeric argument to the test, defaulting to 1.  If set to 0, this will make the test skip initialization of the audio-unit.  Many audio units need initialization.</dd>
  <dt>numeric arguments</dt>
  <dd>An optional numeric argument to the test, defaulting to 0.  If set to 1, this will make the test interpret the first three arguments as numbers rather than strings.</dd>
</dl>

### Exit codes

`auexamine` uses non-standard exit codes for use as part of a build process.  The meaning of each exit code is defined in the `AUValStatus.h` file.  In addition to exit codes, `auexamine` reports on its status through informative messages to standard out and error.

## Building

Build the `auexamine` app under Xcode 4 or 5 on Mac 10.7 and above.  Requires C++11 support.

## License

The code is under copyright, but provided under an MIT-style license in the LICENSE file.
The third_party directory contains sources from other projects.