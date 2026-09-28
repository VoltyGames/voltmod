#pragma once

#include <VoltMod/Core/Result.hpp>
#include <filesystem>

namespace VoltMod
{

/** A shared library the host opened, closed at scope end. Move-only: two owners would close it twice. */
class SharedLibrary
{
public:
    SharedLibrary() = default;
    ~SharedLibrary();

    SharedLibrary(SharedLibrary&& other) noexcept;
    SharedLibrary& operator=(SharedLibrary&& other) noexcept;

    SharedLibrary(const SharedLibrary&) = delete;
    SharedLibrary& operator=(const SharedLibrary&) = delete;

    /** Load @p path. The error carries the operating system's own message. */
    static Result<SharedLibrary> Open(const std::filesystem::path& path);

    /** The address @p name exports, or an error naming it. */
    Result<void*> Symbol(const char* name) const;

    void Close();

private:
    explicit SharedLibrary(void* handle) : _handle(handle) {}

    void* _handle = nullptr;
};

}  // namespace VoltMod
