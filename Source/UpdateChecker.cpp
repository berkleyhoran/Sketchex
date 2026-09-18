#include "UpdateChecker.h"

namespace
{
    // Splits "v1.4.0" (or "1.4.0") into {1,4,0} for a proper numeric
    // compare -- a plain string compare would wrongly decide "v1.10.0" is
    // older than "v1.9.0".
    std::vector<int> parseVersion(const juce::String& v)
    {
        auto s = (v.startsWithChar('v') || v.startsWithChar('V')) ? v.substring(1) : v;
        std::vector<int> parts;
        for (const auto& token : juce::StringArray::fromTokens(s, ".", ""))
            parts.push_back(token.getIntValue());
        while (parts.size() < 3)
            parts.push_back(0);
        return parts;
    }

    bool isNewer(const juce::String& latest, const juce::String& current)
    {
        const auto a = parseVersion(latest);
        const auto b = parseVersion(current);
        const auto n = juce::jmax(a.size(), b.size());
        for (size_t i = 0; i < n; ++i)
        {
            const int av = i < a.size() ? a[i] : 0;
            const int bv = i < b.size() ? b[i] : 0;
            if (av != bv)
                return av > bv;
        }
        return false;
    }
}

UpdateChecker::UpdateChecker() : juce::Thread("Sketchex Update Check") {}

UpdateChecker::~UpdateChecker()
{
    stopThread(4000);
}

void UpdateChecker::checkAsync(std::function<void(Result)> onComplete)
{
    if (isThreadRunning())
        return; // a check is already in flight -- its own callback will still fire

    callback = std::move(onComplete);
    startThread();
}

void UpdateChecker::run()
{
    Result result;

    const juce::URL url("https://api.github.com/repos/berkleyhoran/Sketchex/releases/latest");
    // GitHub's API rejects requests with no User-Agent header (403), which
    // is easy to miss since curl/browsers always set one implicitly.
    const auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                              .withConnectionTimeoutMs(8000)
                              .withExtraHeaders("User-Agent: Sketchex-UpdateChecker\r\n"
                                                "Accept: application/vnd.github+json");

    if (auto stream = url.createInputStream(options))
    {
        const auto json = stream->readEntireStreamAsString();
        const auto parsed = juce::JSON::parse(json);

        if (auto* obj = parsed.getDynamicObject())
        {
            const auto tag = obj->getProperty("tag_name").toString();
            const auto htmlUrl = obj->getProperty("html_url").toString();

            if (tag.isNotEmpty())
            {
                result.checkedOk = true;
                result.latestVersion = tag;
                result.releaseUrl = htmlUrl;
                result.updateAvailable = isNewer(tag, ProjectInfo::versionString);
            }
            else
            {
                result.errorMessage = "GitHub's response didn't include a version tag.";
            }
        }
        else
        {
            result.errorMessage = "Couldn't parse the response from GitHub.";
        }
    }
    else
    {
        result.errorMessage = "Couldn't reach GitHub -- check your internet connection.";
    }

    if (callback)
    {
        auto cb = callback;
        juce::MessageManager::callAsync([cb, result] { cb(result); });
    }
}
