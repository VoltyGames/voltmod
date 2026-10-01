#include "Host/Workshop/DownloadQueue.hpp"

#include <VoltMod/Core/Text/Strings.hpp>
#include <algorithm>
#include <string>
#include <utility>

namespace VoltMod
{

bool DownloadQueue::Add(uint64_t id)
{
    if (id == 0)
    {
        return false;
    }

    const auto found = std::ranges::find(_required, id, &Requirement::Id);
    if (found != _required.end())
    {
        ++found->Holders;
    }
    else
    {
        _required.push_back({.Id = id, .Holders = 1});
    }
    return true;
}

void DownloadQueue::Remove(uint64_t id)
{
    const auto found = std::ranges::find(_required, id, &Requirement::Id);
    if (found != _required.end() && --found->Holders <= 0)
    {
        _required.erase(found);
    }
}

std::vector<uint64_t> DownloadQueue::Required() const
{
    std::vector<uint64_t> ids;
    ids.reserve(_required.size());
    for (const Requirement& requirement : _required)
    {
        ids.push_back(requirement.Id);
    }
    return ids;
}

const DownloadQueue::Client* DownloadQueue::FindClient(int64_t steamId) const
{
    const auto found = _clients.find(steamId);
    return found != _clients.end() ? &found->second : nullptr;
}

std::vector<uint64_t> DownloadQueue::MissingFor(int64_t steamId) const
{
    const Client* client = FindClient(steamId);
    std::vector<uint64_t> missing = Required();
    if (client)
    {
        std::erase_if(missing, [client](uint64_t id) { return std::ranges::contains(client->Downloaded, id); });
    }
    return missing;
}

std::vector<uint64_t> DownloadQueue::ClientMountList(int64_t steamId) const
{
    const Client* client = FindClient(steamId);
    if (!client)
    {
        return {};
    }

    std::vector<uint64_t> ids = Required();
    std::erase_if(
        ids, [client](uint64_t id) { return id != client->Sending && !std::ranges::contains(client->Downloaded, id); });
    return ids;
}

bool DownloadQueue::HasMissing(int64_t steamId) const
{
    const Client* client = FindClient(steamId);
    if (!client)
    {
        return !_required.empty();
    }

    const auto missing = [client](const Requirement& requirement) {
        return !std::ranges::contains(client->Downloaded, requirement.Id);
    };
    return std::ranges::any_of(_required, missing);
}

AddonDecision DownloadQueue::NextToSend(int64_t steamId, double now, int maxAttempts)
{
    const std::vector<uint64_t> missing = MissingFor(steamId);
    if (missing.empty())
    {
        return {};
    }

    Client& client = _clients[steamId];
    const uint64_t next = missing.front();

    // The same addon coming round again means the last offer was not taken.
    client.Attempts = (client.Sending == next) ? client.Attempts + 1 : 1;
    if (client.Attempts > maxAttempts)
    {
        return {.Action = AddonAction::Kick, .Id = next};
    }

    client.Sending = next;
    client.SentAt = now;
    return {.Action = AddonAction::Send, .Id = next, .Remaining = missing.size() - 1};
}

AddonDecision DownloadQueue::DecideJoinMessage(int64_t steamId, bool reconnect, std::string_view addons, double now,
                                               int maxAttempts)
{
    if (!reconnect)
    {
        return NextToSend(steamId, now, maxAttempts);
    }

    // The client handles only the first addon; the rest wait for a later reconnect.
    const std::vector<uint64_t> listed = ParseAddonList(addons);
    if (listed.empty())
    {
        // A client unmounts whatever the map change message does not name.
        const std::vector<uint64_t> downloaded = ClientMountList(steamId);
        if (downloaded.empty())
        {
            return {};
        }
        return {.Action = AddonAction::KeepMounted, .Id = downloaded.front()};
    }

    MarkSending(steamId, listed.front(), now);
    if (listed.size() == 1)
    {
        return {};
    }
    return {.Action = AddonAction::TrimToFirst, .Id = listed.front(), .Remaining = listed.size() - 1};
}

void DownloadQueue::MarkSending(int64_t steamId, uint64_t id, double now)
{
    if (id == 0)
    {
        return;
    }

    Client& client = _clients[steamId];
    client.Sending = id;
    client.SentAt = now;
    client.Attempts = 0;
}

void DownloadQueue::RecordReconnect(int64_t steamId, double now, double timeoutSec)
{
    const auto found = _clients.find(steamId);
    if (found == _clients.end())
    {
        return;
    }

    Client& client = found->second;
    client.LeftAt = 0.0;
    if (client.Sending == 0)
    {
        return;
    }
    if (now - client.SentAt <= timeoutSec)
    {
        if (!std::ranges::contains(client.Downloaded, client.Sending))
        {
            client.Downloaded.push_back(client.Sending);
        }
        client.Attempts = 0;
    }
    client.Sending = 0;
}

void DownloadQueue::RecordDisconnect(int64_t steamId, double now, double forgetAfterSec)
{
    if (const auto found = _clients.find(steamId); found != _clients.end())
    {
        found->second.LeftAt = now;
    }
    std::erase_if(_clients, [now, forgetAfterSec](const auto& entry) {
        const double leftAt = entry.second.LeftAt;
        return leftAt > 0.0 && now - leftAt > forgetAfterSec;
    });
}

void DownloadQueue::ClearProgress()
{
    _clients.clear();
}

/** The comma-separated entries of @p field, verbatim; none for an empty field. */
static std::vector<std::string_view> SplitAddonList(std::string_view field)
{
    std::vector<std::string_view> entries;

    while (!field.empty())
    {
        const size_t comma = field.find(',');
        entries.push_back(field.substr(0, comma));

        if (comma == std::string_view::npos)
        {
            break;
        }
        field.remove_prefix(comma + 1);
        if (field.empty())
        {
            entries.emplace_back();
        }
    }

    return entries;
}

std::vector<uint64_t> ParseAddonList(std::string_view field)
{
    std::vector<uint64_t> ids;
    for (std::string_view entry : SplitAddonList(field))
    {
        if (auto id = ParseUInt64(entry); id && *id != 0)
        {
            ids.push_back(*id);
        }
    }
    return ids;
}

std::vector<uint64_t> AppendToAddonList(std::string& field, const std::vector<uint64_t>& ids)
{
    const std::vector<uint64_t> named = ParseAddonList(field);
    std::vector<uint64_t> appended;

    for (uint64_t id : ids)
    {
        if (id == 0 || std::ranges::contains(named, id) || std::ranges::contains(appended, id))
        {
            continue;
        }

        if (!field.empty())
        {
            field += ',';
        }
        field += std::to_string(id);
        appended.push_back(id);
    }

    return appended;
}

void RemoveFromAddonList(std::string& field, const std::vector<uint64_t>& ids)
{
    std::vector<std::string_view> entries = SplitAddonList(field);
    for (uint64_t id : ids)
    {
        const auto at =
            std::ranges::find_if(entries, [id](std::string_view entry) { return ParseUInt64(entry) == id; });
        if (at != entries.end())
        {
            entries.erase(at);
        }
    }

    std::string kept;
    kept.reserve(field.size());
    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (i > 0)
        {
            kept += ',';
        }
        kept += entries[i];
    }
    field = std::move(kept);
}

}  // namespace VoltMod
