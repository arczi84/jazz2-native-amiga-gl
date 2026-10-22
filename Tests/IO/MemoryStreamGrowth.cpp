#include <IO/MemoryStream.h>
#if defined(DEATH_TARGET_AMIGAOS)
#include <IO/FileSystem.h>
#endif
#include <cstdio>
#include <cstdlib>
#include <cstdint>

static unsigned reallocations;
extern "C" void* __real_realloc(void*, std::size_t);
extern "C" void* __wrap_realloc(void* data, std::size_t size)
{
    ++reallocations;
    return __real_realloc(data, size);
}

int main()
{
    using namespace Death::IO;
#if defined(DEATH_TARGET_AMIGAOS)
    using namespace Death::Containers::Literals;
    if (FileSystem::CombinePath("Work:"_s, "Source"_s) != "Work:Source"_s ||
        FileSystem::CombinePath({"Work:"_s, "Source"_s, "Anims.j2a"_s}) != "Work:Source/Anims.j2a"_s)
        return 7;
#endif
    MemoryStream stream(16384);
    const unsigned before = reallocations;
    for (std::uint32_t i = 0; i < 131072; ++i) {
        const std::uint8_t byte = std::uint8_t(i);
        if (stream.Write(&byte, 1) != 1) return 1;
    }
    const unsigned growths = reallocations - before;
    if (growths > 16 || stream.GetSize() != 131072) {
        std::printf("FAIL: %u reallocations for 128 KiB\n", growths);
        return 2;
    }
    stream.Seek(0, SeekOrigin::Begin);
    for (std::uint32_t i = 0; i < 131072; ++i) {
        std::uint8_t byte = 0;
        if (stream.Read(&byte, 1) != 1 || byte != std::uint8_t(i)) return 3;
    }
    stream.Seek(100, SeekOrigin::Begin);
    const std::uint8_t replacement = 42;
    if (stream.Write(&replacement, 1) != 1 || stream.GetSize() != 131072) return 4;
    stream.SetSize(10);
    stream.Seek(0, SeekOrigin::End);
    if (stream.Write(&replacement, 1) != 1 || stream.GetSize() != 11) return 5;
    std::uint8_t fixed[2] = {};
    MemoryStream bounded(fixed, sizeof(fixed));
    const std::uint8_t bytes[] = {7,8,9};
    if (bounded.Write(bytes, sizeof(bytes)) != 2 || fixed[0] != 7 || fixed[1] != 8) return 6;
    std::printf("PASS: %u reallocations, contents/overwrite/truncate/bounded write correct\n", growths);
    return 0;
}
