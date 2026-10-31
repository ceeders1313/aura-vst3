#include "PresetLibrary.h"
#include <algorithm>

namespace aura
{
const juce::StringArray& PresetLibrary::categories()
{
    static const juce::StringArray c { "Bass", "Lead", "Pad", "Keys", "Pluck", "Arp / Seq",
                                       "Strings", "Brass", "Drums", "FX", "Other" };
    return c;
}

bool PresetLibrary::readInfo (const juce::File& f, PresetInfo& out)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("AuraPreset")) return false;
    out.file     = f;
    out.name     = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
    out.category = xml->getStringAttribute ("category", "Other");
    out.author   = xml->getStringAttribute ("author");
    out.tags     = xml->getStringAttribute ("tags");
    out.layers   = xml->getStringAttribute ("layers");
    out.favorite = xml->getBoolAttribute ("favorite", false);
    out.modified = f.getLastModificationTime();
    return true;
}

bool PresetLibrary::setFavorite (const juce::File& f, bool fav)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr || ! xml->hasTagName ("AuraPreset")) return false;
    xml->setAttribute ("favorite", fav ? 1 : 0);
    return xml->writeTo (f);
}

void PresetLibrary::rescanAsync()
{
    if (scanning.exchange (true)) return;
    pool.addJob ([this] { scan(); });
}

void PresetLibrary::scan()
{
    presetsDir().createDirectory();
    std::vector<PresetInfo> found;
    for (const auto& entry : juce::RangedDirectoryIterator (presetsDir(), true, juce::String ("*") + kExt))
    {
        PresetInfo info;
        if (readInfo (entry.getFile(), info)) found.push_back (std::move (info));
    }
    std::sort (found.begin(), found.end(), [] (const PresetInfo& a, const PresetInfo& b)
               { return a.name.compareNatural (b.name) < 0; });
    {
        const juce::ScopedLock sl (lock);
        items = std::move (found);
    }
    scannedOnce = true;
    scanning = false;
    sendChangeMessage();      // asynchrone : rappel sur le thread message
}

std::vector<PresetInfo> PresetLibrary::snapshot() const
{
    const juce::ScopedLock sl (lock);
    return items;
}
} // namespace aura
