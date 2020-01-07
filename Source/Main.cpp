#include "../JuceLibraryCode/JuceHeader.h"

class ScopedFormatManager : public AudioFormatManager
{
public:
    ScopedFormatManager()
    {
        registerBasicFormats();
    }

    ~ScopedFormatManager()
    {
        clearFormats();
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedFormatManager)
};

int checkFileHasExtention (const File& file, String extention);
int checkInputFile (const File& file);
int getPlugin (std::unique_ptr<AudioPluginInstance>& plugin, String file);
int process (const File& inputFile, ScopedFormatManager& formatManager, AudioPluginInstance* plugin, AudioBuffer<float>& buffer);
int writeToOutputFile (File& file, ScopedFormatManager& formatManager, AudioBuffer<float>& buffer);

struct FileInfo
{
    double sampleRate = 44100.0;
    AudioChannelSet channelSet;
    int bitsPerSample = 16;
};
FileInfo fileInfo;

/**
    Parameters:
    1. plugin to test (.vst, .vst3, .dll, .component)
    2. input wav file (.wav)
    3. (optional) output wav file (.wav)
*/
int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI scopedJuce;
    ScopedFormatManager formatManager;

    // Check number of args
    if (argc < 3 || argc > 4)
    {
        std::cout << "Error: Must have 2 or 3 parameters" << std::endl;
        return 1;
    }

    // load plugin...
    std::unique_ptr<AudioPluginInstance> plugin;
    if (int ret = getPlugin (plugin, argv[1]))
        return ret;

    // create input wav file
    File inputFile;
    if (! File::isAbsolutePath (String(argv[2])))
        inputFile = File (File::getCurrentWorkingDirectory().getFullPathName() + "/" + String (argv[2]));
    else
        inputFile = File (String (argv[2]));
    if (int ret = checkInputFile (inputFile))
        return ret;

    // Create output file
    File outFile;
    if (argc == 4)
    {
        if (! File::isAbsolutePath (String(argv[3])))
            outFile = File (File::getCurrentWorkingDirectory().getFullPathName() + "/" + String (argv[3]));
        else
            outFile = File (String (argv[3]));

        if (int ret = checkFileHasExtention (outFile, ".wav"))
            return ret;
    }
    else
    {
        outFile = inputFile.getParentDirectory().getChildFile (inputFile.getFileNameWithoutExtension() + "-out.wav");
    }

    outFile.create();

    // do processing
    AudioBuffer<float> buffer;
    if (int ret = process (inputFile, formatManager, plugin.get(), buffer))
        return ret;

    if (int ret = writeToOutputFile (outFile, formatManager, buffer))
        return ret;

    plugin.reset (nullptr);

    return 0;
}

int checkFileHasExtention (const File& file, String extention)
{
    if (file.getFileExtension() != extention)
    {
        std::cout << "Error: " << file.getFileName() << " is not a "
            << extention << " file" << std::endl;
        return 1;
    }

    return 0;
}

int checkInputFile (const File& file)
{
    if (! file.existsAsFile())
    {
        std::cout << "Error: " << file.getFileName() << " does not exist" << std::endl;
        return 1;
    }

    return checkFileHasExtention (file, ".wav");
}

int getPlugin (std::unique_ptr<AudioPluginInstance>& plugin, String file)
{
    AudioPluginFormatManager pluginManager;
    pluginManager.addDefaultFormats();

    OwnedArray<PluginDescription> plugins;
    KnownPluginList pluginList;

    // attempt to load plugin from file
    File pluginFile;
    if (! File::isAbsolutePath (String(file)))
        pluginFile = File (File::getCurrentWorkingDirectory().getFullPathName() + "/" + file);
    else
        pluginFile = File (file);
    pluginList.scanAndAddDragAndDroppedFiles(pluginManager, StringArray (pluginFile.getFullPathName()), plugins);

    if (plugins.isEmpty()) // check if loaded
    {
        std::cout << "Error: unable to load plugin" << std::endl;
        return 1;
    }

    // create plugin instance
    String error ("Unable to load plugin");
    plugin = pluginManager.createPluginInstance (*plugins.getFirst(), 44100.0, 256, error);

    return 0;
}

int process (const File& inputFile, ScopedFormatManager& formatManager, AudioPluginInstance* plugin, AudioBuffer<float>& buffer)
{
    // attempt to create file reader
    std::unique_ptr<AudioFormatReader> reader (formatManager.createReaderFor (inputFile));

    if (reader == nullptr)
    {
        std::cout << "Error: unable to read input file" << std::endl;
        return 1;
    }

    // set file info for write file
    fileInfo.sampleRate = reader->sampleRate;
    fileInfo.channelSet = reader->getChannelLayout();
    fileInfo.bitsPerSample = reader->bitsPerSample;

    // read into buffer
    buffer.setSize (reader->numChannels, (int) reader->lengthInSamples);
    reader->read (buffer.getArrayOfWritePointers(), reader->numChannels, 0, (int) reader->lengthInSamples);

    // prepare
    plugin->setNonRealtime (true);
    const int blockSize = 512; //@TODO: make this variable
    plugin->prepareToPlay (reader->sampleRate, blockSize);

    // process
    for (int n = 0; n < reader->lengthInSamples; n += blockSize)
    {
        int samplesToProcess = jmin (blockSize, (int) reader->lengthInSamples - n);

        // create sub-block and process
        AudioBuffer<float> subBuffer (buffer.getArrayOfWritePointers(), reader->numChannels, n, samplesToProcess);
        MidiBuffer midi;
        plugin->processBlock (subBuffer, midi);
    }

    plugin->releaseResources();

    return 0;
}

int writeToOutputFile (File& file, ScopedFormatManager& formatManager, AudioBuffer<float>& buffer)
{
    std::unique_ptr<AudioFormatWriter> writer (formatManager.findFormatForFileExtension ("wav")->createWriterFor (file.createOutputStream(),
        fileInfo.sampleRate, fileInfo.channelSet, fileInfo.bitsPerSample, StringPairArray(), 0));

    writer->flush();
    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());

    return 0;
}
