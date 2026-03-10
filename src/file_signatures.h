#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rt {

// ── File signature for carving ──────────────────────────────────────────────
struct FileSignature {
    int                   id;
    std::wstring          category;     // "Image", "Document", "Video", "Audio", "Archive"
    std::wstring          extension;    // ".jpg", ".png", etc.
    std::wstring          description;
    std::vector<uint8_t>  header;       // magic bytes at file start
    std::vector<uint8_t>  footer;       // optional end-of-file marker
    int                   headerOffset; // offset from start where header appears
    uint64_t              maxFileSize;  // maximum expected file size (for carving limit)
};

// Get the built-in signature database
const std::vector<FileSignature>& GetSignatureDatabase();

// Match buffer against all signatures; returns matching signature IDs
std::vector<int> MatchSignatures(const uint8_t* data, size_t dataLen);

// Get a signature by ID
const FileSignature* GetSignatureById(int id);

// Get category icon index for the GUI
int GetCategoryIconIndex(const std::wstring& category);

} // namespace rt
