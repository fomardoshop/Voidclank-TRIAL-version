#pragma once
#include <JuceHeader.h>

// Tiny per-user settings file (Fomardo/Voidclank/settings.txt, "key=value" lines).
#ifndef VC_VERSION
 #define VC_VERSION "1.3.0"
#endif

namespace vcset
{
    inline juce::String versionString() { return VC_VERSION; }

    inline juce::File file()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Fomardo").getChildFile ("Voidclank").getChildFile ("settings.txt");
    }

    inline int getInt (const juce::String& key, int def)
    {
        juce::StringArray lines;
        lines.addLines (file().loadFileAsString());
        for (auto& l : lines)
            if (l.upToFirstOccurrenceOf ("=", false, false).trim() == key)
                return l.fromFirstOccurrenceOf ("=", false, false).trim().getIntValue();
        return def;
    }

    inline void setInt (const juce::String& key, int value)
    {
        juce::StringArray lines, out;
        lines.addLines (file().loadFileAsString());
        for (auto& l : lines)
            if (l.upToFirstOccurrenceOf ("=", false, false).trim() != key && l.contains ("="))
                out.add (l);
        out.add (key + "=" + juce::String (value));
        file().getParentDirectory().createDirectory();
        file().replaceWithText (out.joinIntoString ("\n") + "\n");
    }
}
