#include "file_signatures.h"
#include <algorithm>

namespace rt {

static std::vector<FileSignature> BuildDatabase() {
    std::vector<FileSignature> db;
    int id = 0;

    auto add = [&](const std::wstring& cat, const std::wstring& ext,
                    const std::wstring& desc, std::vector<uint8_t> hdr,
                    std::vector<uint8_t> ftr, int off, uint64_t maxSz) {
        db.push_back({ id++, cat, ext, desc, std::move(hdr), std::move(ftr), off, maxSz });
    };

    // ── Images ──────────────────────────────────────────────────────────
    add(L"Image", L".jpg", L"JPEG Image",
        {0xFF, 0xD8, 0xFF}, {0xFF, 0xD9}, 0, 50ULL * 1024 * 1024);

    add(L"Image", L".png", L"PNG Image",
        {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A}, {0x49, 0x45, 0x4E, 0x44}, 0, 100ULL * 1024 * 1024);

    add(L"Image", L".gif", L"GIF Image",
        {0x47, 0x49, 0x46, 0x38}, {0x00, 0x3B}, 0, 50ULL * 1024 * 1024);

    add(L"Image", L".bmp", L"Bitmap Image",
        {0x42, 0x4D}, {}, 0, 200ULL * 1024 * 1024);

    add(L"Image", L".tif", L"TIFF Image",
        {0x49, 0x49, 0x2A, 0x00}, {}, 0, 500ULL * 1024 * 1024);

    add(L"Image", L".webp", L"WebP Image",
        {0x52, 0x49, 0x46, 0x46}, {}, 0, 100ULL * 1024 * 1024);

    // ── Documents ───────────────────────────────────────────────────────
    add(L"Document", L".pdf", L"PDF Document",
        {0x25, 0x50, 0x44, 0x46}, {0x25, 0x25, 0x45, 0x4F, 0x46}, 0, 500ULL * 1024 * 1024);

    add(L"Document", L".docx", L"Word Document (OOXML)",
        {0x50, 0x4B, 0x03, 0x04}, {}, 0, 200ULL * 1024 * 1024);

    add(L"Document", L".doc", L"Word Document (Legacy)",
        {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1}, {}, 0, 200ULL * 1024 * 1024);

    add(L"Document", L".xlsx", L"Excel Spreadsheet",
        {0x50, 0x4B, 0x03, 0x04}, {}, 0, 200ULL * 1024 * 1024);

    add(L"Document", L".pptx", L"PowerPoint Presentation",
        {0x50, 0x4B, 0x03, 0x04}, {}, 0, 500ULL * 1024 * 1024);

    // ── Video ───────────────────────────────────────────────────────────
    add(L"Video", L".mp4", L"MP4 Video",
        {0x00, 0x00, 0x00}, {}, 0, 4ULL * 1024 * 1024 * 1024);

    add(L"Video", L".avi", L"AVI Video",
        {0x52, 0x49, 0x46, 0x46}, {}, 0, 4ULL * 1024 * 1024 * 1024);

    add(L"Video", L".mkv", L"Matroska Video",
        {0x1A, 0x45, 0xDF, 0xA3}, {}, 0, 4ULL * 1024 * 1024 * 1024);

    add(L"Video", L".mov", L"QuickTime Video",
        {0x00, 0x00, 0x00, 0x14, 0x66, 0x74, 0x79, 0x70}, {}, 0, 4ULL * 1024 * 1024 * 1024);

    add(L"Video", L".wmv", L"Windows Media Video",
        {0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11}, {}, 0, 2ULL * 1024 * 1024 * 1024);

    // ── Audio ───────────────────────────────────────────────────────────
    add(L"Audio", L".mp3", L"MP3 Audio",
        {0x49, 0x44, 0x33}, {}, 0, 100ULL * 1024 * 1024);

    add(L"Audio", L".wav", L"WAV Audio",
        {0x52, 0x49, 0x46, 0x46}, {}, 0, 1ULL * 1024 * 1024 * 1024);

    add(L"Audio", L".flac", L"FLAC Audio",
        {0x66, 0x4C, 0x61, 0x43}, {}, 0, 500ULL * 1024 * 1024);

    add(L"Audio", L".ogg", L"OGG Audio",
        {0x4F, 0x67, 0x67, 0x53}, {}, 0, 200ULL * 1024 * 1024);

    add(L"Audio", L".wma", L"Windows Media Audio",
        {0x30, 0x26, 0xB2, 0x75, 0x8E, 0x66, 0xCF, 0x11}, {}, 0, 200ULL * 1024 * 1024);

    // ── Archives ────────────────────────────────────────────────────────
    add(L"Archive", L".zip", L"ZIP Archive",
        {0x50, 0x4B, 0x03, 0x04}, {0x50, 0x4B, 0x05, 0x06}, 0, 4ULL * 1024 * 1024 * 1024);

    add(L"Archive", L".rar", L"RAR Archive",
        {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07}, {}, 0, 4ULL * 1024 * 1024 * 1024);

    add(L"Archive", L".7z", L"7-Zip Archive",
        {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C}, {}, 0, 4ULL * 1024 * 1024 * 1024);

    add(L"Archive", L".gz", L"GZip Archive",
        {0x1F, 0x8B}, {}, 0, 4ULL * 1024 * 1024 * 1024);

    // ── Executables ─────────────────────────────────────────────────────
    add(L"Program", L".exe", L"Windows Executable",
        {0x4D, 0x5A}, {}, 0, 500ULL * 1024 * 1024);

    add(L"Program", L".dll", L"Dynamic Link Library",
        {0x4D, 0x5A}, {}, 0, 200ULL * 1024 * 1024);

    return db;
}

const std::vector<FileSignature>& GetSignatureDatabase() {
    static auto db = BuildDatabase();
    return db;
}

std::vector<int> MatchSignatures(const uint8_t* data, size_t dataLen) {
    std::vector<int> matches;
    for (auto& sig : GetSignatureDatabase()) {
        if (sig.header.empty()) continue;
        size_t off = static_cast<size_t>(sig.headerOffset);
        if (off + sig.header.size() > dataLen) continue;

        if (std::equal(sig.header.begin(), sig.header.end(), data + off)) {
            matches.push_back(sig.id);
        }
    }
    return matches;
}

const FileSignature* GetSignatureById(int id) {
    auto& db = GetSignatureDatabase();
    if (id >= 0 && id < (int)db.size()) return &db[id];
    return nullptr;
}

int GetCategoryIconIndex(const std::wstring& category) {
    if (category == L"Image")    return 0;
    if (category == L"Document") return 1;
    if (category == L"Video")    return 2;
    if (category == L"Audio")    return 3;
    if (category == L"Archive")  return 4;
    if (category == L"Program")  return 5;
    return 6;
}

} // namespace rt
