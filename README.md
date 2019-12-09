# PluginRunner

A minimal command-line application for running audio through an audio
plugin. Made with [JUCE](https://www.github.com/WeAreROLI/JUCE).
Mostly only tested on Windows.

## Usage

```bash
$ ./PluginRunner.exe <plugin> <input wav file> <output wav file (optional)>
```

For example:

```bash
$ ./PluginRunner.exe myPlugin.vst3 test.wav test-out.wav
```

## Building

Build with JUCE version 5.4.4. Make sure you have the VST2 SDK set up
in your JUCE global paths.

## License

Licensed under the [WTFPL](http://www.wtfpl.net/). Enjoy!
