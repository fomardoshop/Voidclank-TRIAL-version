#include "License.h"
#include "LicensePublicKey.h"

namespace
{
    constexpr juce::int64 dayMs = 24LL * 60 * 60 * 1000;
    const juce::String fileSalt = "vc1-9f2b7e41-d03a";

    juce::String checksum (const juce::String& body)
    {
        return juce::SHA256 ((body + fileSalt).toUTF8()).toHexString();
    }
}

//==============================================================================
juce::Array<juce::File> LicenseManager::defaultStorage()
{
    juce::Array<juce::File> f;
    for (auto loc : { juce::File::userApplicationDataDirectory, juce::File::commonApplicationDataDirectory })
        f.add (juce::File::getSpecialLocation (loc)
                   .getChildFile ("Fomardo").getChildFile ("Voidclank").getChildFile ("vc.dat"));
    return f;
}

LicenseManager::LicenseManager (Clock c, juce::Array<juce::File> storage, bool autoRefresh)
    : clock (c ? std::move (c) : Clock ([] { return juce::Time::currentTimeMillis(); })),
      files (storage.isEmpty() ? defaultStorage() : std::move (storage))
{
    load();
    refresh();
    if (autoRefresh)
        startTimer (60 * 1000);
}

//==============================================================================
LicenseManager::Record LicenseManager::readFile (const juce::File& f)
{
    Record r;
    if (! f.existsAsFile())
        return r;

    juce::StringArray lines;
    lines.addLines (f.loadFileAsString());

    juce::String body, sum;
    for (auto& l : lines)
    {
        const auto eq = l.indexOfChar ('=');
        if (eq < 0) continue;
        const auto k = l.substring (0, eq), v = l.substring (eq + 1);

        if (k == "sum") { sum = v; continue; }
        body << k << "=" << v << "\n";

        if (k == "first") r.first = v.getLargeIntValue();
        else if (k == "last") r.last = v.getLargeIntValue();
        else if (k == "email") r.email = v;
        else if (k == "code") r.code = v;
    }

    r.valid = sum.isNotEmpty() && sum == checksum (body) && r.first > 0;
    return r;
}

void LicenseManager::writeFile (const juce::File& f, const Record& r)
{
    juce::String body;
    body << "first=" << juce::String (r.first) << "\n"
         << "last="  << juce::String (r.last)  << "\n"
         << "email=" << r.email << "\n"
         << "code="  << r.code  << "\n";

    if (f.getParentDirectory().createDirectory().wasOk())
        f.replaceWithText (body + "sum=" + checksum (body) + "\n");
}

//==============================================================================
void LicenseManager::load()
{
    const juce::ScopedLock sl (lock);
    const auto now = clock();

    bool anyFile = false, anyValid = false;
    firstRun = 0; lastSeen = 0;

    for (auto& f : files)
    {
        if (! f.existsAsFile()) continue;
        anyFile = true;

        auto r = readFile (f);
        if (! r.valid) continue;
        anyValid = true;

        firstRun = firstRun == 0 ? r.first : juce::jmin (firstRun, r.first);
        lastSeen = juce::jmax (lastSeen, r.last);
        if (email.isEmpty() && r.email.isNotEmpty()) { email = r.email; code = r.code; }
    }

    if (! anyFile)            { firstRun = now; lastSeen = now; }   // fresh install: trial starts now
    else if (! anyValid)      { firstRun = 1; lastSeen = now; }     // files were edited: trial is spent
}

void LicenseManager::save()
{
    Record r;
    r.first = firstRun; r.last = lastSeen; r.email = email; r.code = code; r.valid = true;
    for (auto& f : files)
        writeFile (f, r);
    lastSaved = lastSeen;
}

//==============================================================================
void LicenseManager::updateStateLocked (juce::int64 now)
{
    clockTampered = (now + dayMs < lastSeen);   // system clock moved back

#if VC_FRIENDS_BUILD
    // Tester build: always fully unlocked, no trial, never muted.
    state = Licensed;
    daysLeft = 0;
    return;
#endif

    const juce::RSAKey pub (BASSPHEMY_PUBLIC_KEY);
    if (email.isNotEmpty() && pub.isValid() && verify (email, code, pub))
    {
        state = Licensed;
        daysLeft = 0;
        return;
    }

    const auto elapsed = now - firstRun;
    if (clockTampered || elapsed >= trialDays * dayMs)
    {
        state = Expired;
        daysLeft = 0;
    }
    else
    {
        state = Trial;
        daysLeft = (int) ((trialDays * dayMs - elapsed + dayMs - 1) / dayMs);
    }
}

void LicenseManager::refresh()
{
    const juce::ScopedLock sl (lock);
    const auto now = clock();

    const bool firstWrite = (lastSaved == 0);
    lastSeen = juce::jmax (lastSeen, now);
    updateStateLocked (now);

    if (firstWrite || lastSeen - lastSaved > 10 * 60 * 1000)
        save();
}

bool LicenseManager::activate (const juce::String& e, const juce::String& c, juce::String& error)
{
    const juce::RSAKey pub (BASSPHEMY_PUBLIC_KEY);
    if (! pub.isValid())
    {
        error = "Licensing is not configured in this build.";
        return false;
    }

    const auto mail = e.trim().toLowerCase();
    if (! mail.contains ("@"))
    {
        error = "Please enter the e-mail address used for the purchase.";
        return false;
    }

    if (! verify (mail, c, pub))
    {
        error = "This licence code does not match the e-mail address.";
        return false;
    }

    const juce::ScopedLock sl (lock);
    email = mail;
    code = c.removeCharacters (" \r\n\t");
    updateStateLocked (clock());
    save();
    return true;
}

juce::String LicenseManager::getLicensee() const
{
#if VC_FRIENDS_BUILD
    return "Tester";
#else
    const juce::ScopedLock sl (lock);
    return email;
#endif
}

//==============================================================================
juce::BigInteger LicenseManager::payloadValue (const juce::String& e)
{
    const auto payload = e.trim().toLowerCase() + "|BASSPHEMY|1";
    // JUCE's RSAKey uses the small public exponent 3, so the signed value must be large
    // (a short hash could be forged by taking an integer cube root).  Expand to 768 bits.
    juce::MemoryBlock mb;
    for (int i = 0; i < 3; ++i)
        mb.append (juce::SHA256 ((payload + "|" + juce::String (i)).toUTF8()).getRawData().getData(), 32);

    juce::BigInteger v;
    v.loadFromMemoryBlock (mb);
    v.setBit (768);   // fixed top bit -> constant length, always smaller than the 1024-bit modulus
    return v;
}

juce::String LicenseManager::sign (const juce::String& e, const juce::RSAKey& privateKey)
{
    auto v = payloadValue (e);
    privateKey.applyToValue (v);
    return v.toMemoryBlock().toBase64Encoding();
}

bool LicenseManager::verify (const juce::String& e, const juce::String& codeText,
                             const juce::RSAKey& publicKey)
{
    if (! publicKey.isValid())
        return false;

    juce::MemoryBlock mb;
    if (! mb.fromBase64Encoding (codeText.removeCharacters (" \r\n\t")) || mb.getSize() == 0)
        return false;

    juce::BigInteger s;
    s.loadFromMemoryBlock (mb);
    if (s.isZero() || ! publicKey.applyToValue (s))
        return false;

    return s == payloadValue (e);
}
