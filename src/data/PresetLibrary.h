#pragma once
#include <juce_events/juce_events.h>
#include <atomic>
#include <vector>

namespace aura
{
struct PresetInfo
{
    juce::File file;
    juce::String name, category, author, tags, layers;
    bool favorite = false;
    juce::Time modified;
};

// Bibliotheque de presets : scan du dossier utilisateur sur un thread dedie, index en memoire.
// (Etape suivante prevue : index SQLite/FTS5 quand la bibliotheque depassera quelques milliers de presets.)
class PresetLibrary : public juce::ChangeBroadcaster
{
public:
    static constexpr int kFormatVersion = 1;
    static constexpr const char* kExt = ".aurapreset";

    PresetLibrary() = default;
    ~PresetLibrary() override { pool.removeAllJobs (true, 4000); }

    static juce::File rootDir()    { return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("AURA"); }
    static juce::File presetsDir() { return rootDir().getChildFile ("Presets"); }
    static juce::File samplesDir() { return rootDir().getChildFile ("Samples"); }
    static const juce::StringArray& categories();

    void rescanAsync();                      // thread message
    bool hasScanned() const { return scannedOnce.load(); }
    bool isScanning() const { return scanning.load(); }
    std::vector<PresetInfo> snapshot() const;

    static bool readInfo (const juce::File& f, PresetInfo& out);
    static bool setFavorite (const juce::File& f, bool fav);

private:
    void scan();

    mutable juce::CriticalSection lock;
    std::vector<PresetInfo> items;
    std::atomic<bool> scanning { false }, scannedOnce { false };
    juce::ThreadPool pool { 1 };             // declare en dernier : detruit en premier
};
} // namespace aura
