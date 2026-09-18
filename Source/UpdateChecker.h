#pragma once

#include <JuceHeader.h>

// Checks GitHub Releases for a newer version than this build, entirely on
// a background thread (network calls must never touch the message
// thread). Deliberately does NOT auto-download-and-run an installer --
// see checkForUpdates()'s call site in PluginEditor.cpp for why -- it
// just reports whether a newer version exists and hands back that
// release's page URL so the caller can open it in the system browser,
// the same way every release has always been distributed.
class UpdateChecker : private juce::Thread
{
public:
    struct Result
    {
        bool checkedOk = false;       // false if the network request/parse itself failed
        bool updateAvailable = false; // only meaningful when checkedOk
        juce::String latestVersion;   // e.g. "v1.4.0"
        juce::String releaseUrl;      // the GitHub release page, for opening in a browser
        juce::String errorMessage;    // set when checkedOk is false
    };

    UpdateChecker();
    ~UpdateChecker() override;

    // Starts a background check; onComplete is invoked on the message
    // thread once it finishes. If a check is already in flight, this call
    // is silently ignored (rather than queuing a second one) -- the
    // in-flight check's own callback will still fire.
    void checkAsync(std::function<void(Result)> onComplete);

private:
    void run() override;

    std::function<void(Result)> callback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UpdateChecker)
};
