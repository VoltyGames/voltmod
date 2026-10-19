#include "Ui/WriteCache.hpp"

namespace VoltMod
{

/** Separates the parts of a key; not legal in an element id or a variable name. */
static constexpr char KeySeparator = '\x1f';

/** The value a class write is cached under. */
static std::string_view ClassValue(bool on)
{
    return on ? "1" : "0";
}

WriteCache::Sent* WriteCache::For(int slot)
{
    if (slot == EveryoneSlot)
    {
        return &_everyone;
    }
    if (!IsValidSlot(slot))
    {
        return nullptr;
    }

    std::optional<Sent>& sent = _slots[slot];
    if (!sent)
    {
        sent.emplace();
    }
    return &*sent;
}

bool WriteCache::Changed(int slot, WriteKind kind, std::string_view elementId, std::string_view name,
                         std::string_view value)
{
    Sent* sent = For(slot);
    if (!sent)
    {
        return false;
    }

    const std::string& key = KeyFor(kind, elementId, name);
    if (auto it = sent->Values.find(key); it != sent->Values.end())
    {
        if (it->second == value)
        {
            return false;
        }

        it->second = value;
        return true;
    }

    sent->Values.emplace(key, value);
    return true;
}

bool WriteCache::ClassChanged(int slot, std::string_view elementId, std::string_view className, bool on)
{
    return Changed(slot, WriteKind::Class, elementId, className, ClassValue(on));
}

bool WriteCache::IsClassOn(int slot, std::string_view elementId, std::string_view className)
{
    Sent* sent = For(slot);
    if (!sent)
    {
        return false;
    }

    const auto it = sent->Values.find(KeyFor(WriteKind::Class, elementId, className));
    return it != sent->Values.end() && it->second == ClassValue(true);
}

bool WriteCache::CursorChanged(int slot, bool shown)
{
    Sent* sent = For(slot);
    if (!sent || sent->Cursor == shown)
    {
        return false;
    }

    sent->Cursor = shown;
    return true;
}

bool WriteCache::IsFirstFailure(int slot)
{
    Sent* sent = For(slot);
    if (!sent || sent->Failed)
    {
        return false;
    }

    sent->Failed = true;
    return true;
}

void WriteCache::Invalidate(int slot)
{
    Sent* sent = For(slot);
    if (!sent)
    {
        return;
    }

    sent->Values.clear();
    sent->Cursor.reset();
}

void WriteCache::RemoveSlot(int slot)
{
    _slots.Reset(slot);
}

void WriteCache::ResetCursor(int slot)
{
    if (Sent* sent = For(slot))
    {
        sent->Cursor.reset();
    }
}

void WriteCache::Clear()
{
    _slots.ResetAll();
    _everyone = {};
}

const std::string& WriteCache::KeyFor(WriteKind kind, std::string_view elementId, std::string_view name)
{
    _key.assign(1, static_cast<char>(kind));
    _key += KeySeparator;
    _key += elementId;
    _key += KeySeparator;
    _key += name;
    return _key;
}

}  // namespace VoltMod
