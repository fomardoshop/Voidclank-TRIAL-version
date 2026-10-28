#pragma once

#include <JuceHeader.h>
#include <functional>

#ifndef VC_FRIENDS_BUILD
 #define VC_FRIENDS_BUILD 0
#endif

//==============================================================================
// 14-day trial + offline licence keys.
//
//  * Trial start / last-seen time are stored in two small checksummed files
//    (per-user and, where writable, machine-wide).  Deleting one copy does not
//    restart the trial; moving the system clock back expires it.
//  * A licence code is an RSA signature (made with YOUR private key, kept
//    secret, see Tools/Keygen.cpp) over the buyer's e-mail address.  The plugin
//    only contains the public key, so it cannot be used to make new codes.
//
//  NOTE: any purely offline scheme can eventually be cracked by a determined
//  person.  This stops casual copying; for stronger protection use an online
//  activation service or iLok on top of this.
//==============================================================================
class LicenseManager : private juce::Timer
{
public:
    enum State { Trial = 0, Expired = 1, Licensed = 2 };

    static constexpr int trialDays = 14;
    static constexpr const char* purchaseUrl = "https://fomardo.pl/voidclank";

    using Clock = std::function<juce::int64()>;

    // clock / storage can be injected for testing; defaults are the real ones.
    explicit LicenseManager (Clock clock = {}, juce::Array<juce::File> storage = {},
                             bool autoRefresh = true);
    ~LicenseManager() override { stopTimer(); }

    State getState() const noexcept        { return (State) state.load(); }
    int getDaysLeft() const noexcept       { return daysLeft.load(); }
    juce::String getLicensee() const;

    // Message thread only (file IO).
    void refresh();
    bool activate (const juce::String& email, const juce::String& licenseCode,
                   juce::String& errorMessage);

    // --- crypto helpers (also used by the key generator tool)
    static juce::BigInteger payloadValue (const juce::String& email);
    static juce::String sign (const juce::String& email, const juce::RSAKey& privateKey);
    static bool verify (const juce::String& email, const juce::String& code,
                        const juce::RSAKey& publicKey);

private:
    struct Record
    {
        juce::int64 first = 0, last = 0;
        juce::String email, code;
        bool valid = false;
    };

    static juce::Array<juce::File> defaultStorage();
    static Record readFile (const juce::File&);
    static void writeFile (const juce::File&, const Record&);

    void timerCallback() override { refresh(); }
    void load();
    void save();
    void updateStateLocked (juce::int64 now);

    Clock clock;
    juce::Array<juce::File> files;

    mutable juce::CriticalSection lock;
    juce::int64 firstRun = 0, lastSeen = 0, lastSaved = 0;
    juce::String email, code;
    bool clockTampered = false;

    std::atomic<int> state { Trial };
    std::atomic<int> daysLeft { trialDays };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LicenseManager)
};
