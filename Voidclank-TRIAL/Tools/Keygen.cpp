// Voidclank licence tool (for the SELLER only - never ship this).
//
//   VoidclankKeygen gen                          -> creates private.key + public.txt
//   VoidclankKeygen sign private.key a@b.com     -> prints the licence code for that e-mail
//
// Paste the contents of public.txt into Source/LicensePublicKey.h and rebuild.
#include "../Source/License.h"
#include <cstdio>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc >= 2 && juce::String (argv[1]) == "gen")
    {
        juce::RSAKey pub, priv;
        juce::RSAKey::createKeyPair (pub, priv, 1024);

        juce::File ("private.key").replaceWithText (priv.toString());
        juce::File ("public.txt").replaceWithText (pub.toString());
        std::printf ("Wrote private.key (KEEP SECRET) and public.txt.\n");
        std::printf ("Put this in Source/LicensePublicKey.h:\n#define BASSPHEMY_PUBLIC_KEY \"%s\"\n",
                     pub.toString().toRawUTF8());
        return 0;
    }

    if (argc >= 4 && juce::String (argv[1]) == "sign")
    {
        juce::RSAKey priv (juce::File (argv[2]).loadFileAsString().trim());
        if (! priv.isValid()) { std::fprintf (stderr, "Bad private key file.\n"); return 1; }
        std::printf ("%s\n", LicenseManager::sign (argv[3], priv).toRawUTF8());
        return 0;
    }

    std::fprintf (stderr, "usage: VoidclankKeygen gen | sign <private.key> <email>\n");
    return 1;
}
