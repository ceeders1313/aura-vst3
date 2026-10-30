#include "BankLoader.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <set>
#include <vector>

namespace aura
{
namespace
{
constexpr size_t kMaxBytes = size_t (1500) * 1024 * 1024;   // plafond RAM v0.2

bool parseNote (const juce::String& tok, int& midi)
{
    if (tok.length() < 2 || tok.length() > 5) return false;

    int semis = 0;
    switch (juce::CharacterFunctions::toUpperCase (tok[0]))
    {
        case 'C': semis = 0;  break;
        case 'D': semis = 2;  break;
        case 'E': semis = 4;  break;
        case 'F': semis = 5;  break;
        case 'G': semis = 7;  break;
        case 'A': semis = 9;  break;
        case 'B': semis = 11; break;
        default: return false;
    }

    int idx = 1;
    if (tok[idx] == '#')      { ++semis; ++idx; }
    else if (tok[idx] == 'b') { --semis; ++idx; }

    const juce::String oct = tok.substring (idx);
    if (oct.isEmpty()) return false;

    int k = (oct[0] == '-') ? 1 : 0;
    if (k >= oct.length()) return false;
    for (int i = k; i < oct.length(); ++i)
        if (! juce::CharacterFunctions::isDigit (oct[i])) return false;

    midi = (oct.getIntValue() + 1) * 12 + semis;    // C4 = 60
    return midi >= 0 && midi <= 127;
}

bool parseIndexed (const juce::String& tok, const juce::String& prefix, int& value)
{
    const juce::String low = tok.toLowerCase();
    if (! low.startsWith (prefix) || low.length() <= prefix.length()) return false;
    const juce::String rest = low.substring (prefix.length());
    for (int i = 0; i < rest.length(); ++i)
        if (! juce::CharacterFunctions::isDigit (rest[i])) return false;
    value = rest.getIntValue();
    return true;
}

struct Item { juce::File file; int note = -1; int layer = 1; int rr = 1; };
struct Loaded { Zone zone; int layer = 1; };
} // namespace

SampleBank::Ptr BankLoader::build (const juce::Array<juce::File>& filesIn,
                                   const juce::String& bankName,
                                   juce::String& report)
{
    std::vector<juce::File> files;
    for (const auto& f : filesIn) files.push_back (f);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileName().compareNatural (b.getFileName()) < 0; });

    // 1) Analyse des noms de fichiers
    std::vector<Item> items;
    for (const auto& f : files)
    {
        Item it;
        it.file = f;
        juce::StringArray tokens;
        tokens.addTokens (f.getFileNameWithoutExtension(), "_ .", "");
        for (const auto& tok : tokens)
        {
            int v = 0;
            if (parseIndexed (tok, "rr", v))      it.rr = juce::jmax (1, v);
            else if (parseIndexed (tok, "v", v))  it.layer = juce::jmax (1, v);
            else if (it.note < 0 && parseNote (tok, v)) it.note = v;
        }
        items.push_back (it);
    }

    if (items.size() == 1 && items[0].note < 0)
        items[0].note = 60;
    else
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].note < 0) items[i].note = juce::jlimit (0, 127, 36 + int (i));

    // 2) Lecture des audios
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    std::vector<Loaded> loaded;
    size_t totalBytes = 0;
    int skipped = 0;

    for (const auto& it : items)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (it.file));
        if (reader == nullptr || reader->lengthInSamples <= 0) { ++skipped; continue; }
        if (reader->lengthInSamples > (juce::int64) (reader->sampleRate * 600.0)) { ++skipped; continue; }

        const int len = (int) reader->lengthInSamples;
        const size_t zbytes = size_t (len) * 2 * sizeof (float);
        if (totalBytes + zbytes > kMaxBytes) { ++skipped; continue; }

        Loaded l;
        l.zone.data.setSize (2, len);
        reader->read (&l.zone.data, 0, len, 0, true, true);
        if (reader->numChannels == 1)
            l.zone.data.copyFrom (1, 0, l.zone.data, 0, 0, len);

        l.zone.name    = it.file.getFileNameWithoutExtension();
        l.zone.srcRate = reader->sampleRate;
        l.zone.rootKey = it.note;
        l.zone.rr      = it.rr;
        l.layer        = it.layer;

        const int ls = reader->metadataValues.getValue ("Loop0Start", "-1").getIntValue();
        const int le = reader->metadataValues.getValue ("Loop0End", "-1").getIntValue();
        if (ls >= 0 && le > ls)
        {
            l.zone.loopStart = ls;
            l.zone.loopEnd   = std::min (len, le + 1);
            if (l.zone.loopEnd <= l.zone.loopStart) { l.zone.loopStart = -1; l.zone.loopEnd = -1; }
        }

        totalBytes += zbytes;
        loaded.push_back (std::move (l));
    }

    if (loaded.empty())
    {
        report = "No valid samples";
        return nullptr;
    }

    // 3) Cartographie : plages de touches (milieu entre racines voisines) et couches de velocite
    std::set<int> noteSet, layerSet;
    for (const auto& l : loaded) { noteSet.insert (l.zone.rootKey); layerSet.insert (l.layer); }
    const std::vector<int> notes (noteSet.begin(), noteSet.end());
    const std::vector<int> layers (layerSet.begin(), layerSet.end());
    const int numLayers = (int) layers.size();
    const int numNotes  = (int) notes.size();

    for (auto& l : loaded)
    {
        const int ni = int (std::lower_bound (notes.begin(), notes.end(), l.zone.rootKey) - notes.begin());
        l.zone.lowKey  = (ni == 0) ? 0 : (notes[size_t (ni - 1)] + notes[size_t (ni)]) / 2 + 1;
        l.zone.highKey = (ni == numNotes - 1) ? 127 : (notes[size_t (ni)] + notes[size_t (ni + 1)]) / 2;

        const int li = int (std::lower_bound (layers.begin(), layers.end(), l.layer) - layers.begin());
        l.zone.lowVel  = li * 127 / numLayers + 1;
        l.zone.highVel = (li + 1) * 127 / numLayers;
    }

    SampleBank::Ptr bank (new SampleBank());
    bank->name  = bankName;
    bank->bytes = totalBytes;
    for (auto& l : loaded) bank->zones.push_back (std::move (l.zone));

    report = juce::String ((int) bank->zones.size()) + " zones, "
           + juce::String ((int) (totalBytes / (1024 * 1024))) + " MB, "
           + juce::String (numLayers) + " vel layer(s)";
    if (skipped > 0) report << ", " << juce::String (skipped) << " skipped";
    return bank;
}
} // namespace aura
