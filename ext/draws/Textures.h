#pragma once
#include "../Platform.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"
#include "menu/string_encryption.hpp"
#include "dgb/lazy.hpp"

#include <windows.h>
#include <d3d9.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <mutex>
#include <algorithm>

#include "dgb/Spoof.h"

inline std::unordered_map<std::string, ImTextureID> g_LootTextureCache;

inline const char* GetIconCachePath() {
    SPOOF_FUNC;
    static std::string path;
    if (path.empty()) {
        std::string hwid;
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, X("SOFTWARE\\Microsoft\\Cryptography"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
            char buf[256]; DWORD sz = sizeof(buf);
            if (RegQueryValueExA(hKey, X("MachineGuid"), NULL, NULL, (LPBYTE)buf, &sz) == ERROR_SUCCESS)
                hwid = buf;
            RegCloseKey(hKey);
        }
        if (hwid.empty()) hwid = X("default");
        unsigned int h = 0x811c9dc5;
        for (char c : hwid) { h ^= (unsigned char)c; h *= 0x01000193; }
        char hx[16]; sprintf_s(hx, "%08x", h);
        path = std::string(X("C:\\ProgramData\\pc")) + hx + X(".bin");
    }
    return path.c_str();
}
constexpr uint32_t CACHE_MAGIC = 0x49434F4E;
constexpr uint32_t CACHE_VERSION = 1;



inline std::vector<unsigned char> DownloadFile(const std::string&)
{
    SPOOF_FUNC;
    return {};
}

class IconCrypto
{
public:
    static constexpr uint8_t BASE_KEY[] = {
        0x4A, 0x7B, 0x2C, 0x9D, 0x1E, 0x8F, 0x3A, 0x6B,
        0x5C, 0x0D, 0xAE, 0x4F, 0x7E, 0x2B, 0x9C, 0x1D
    };

    static void EncryptDecrypt(std::vector<uint8_t>& data, const std::string& itemName)
    {
        SPOOF_FUNC;
        std::vector<uint8_t> key = DeriveKey(itemName);

        for (size_t i = 0; i < data.size(); i++)
        {
            data[i] ^= key[i % key.size()];
            data[i] ^= static_cast<uint8_t>((i * 0x9E3779B9) >> 24);
        }
    }

private:
    static std::vector<uint8_t> DeriveKey(const std::string& itemName)
    {
        SPOOF_FUNC;
        std::vector<uint8_t> key(sizeof(BASE_KEY));
        memcpy(key.data(), BASE_KEY, sizeof(BASE_KEY));

        for (size_t i = 0; i < itemName.size(); i++)
        {
            key[i % key.size()] ^= static_cast<uint8_t>(itemName[i]);
            key[(i + 7) % key.size()] += static_cast<uint8_t>(itemName[i] * 31);
        }

        return key;
    }
};

#pragma pack(push, 1)
struct CacheHeader
{
    uint32_t magic;
    uint32_t version;
    uint32_t entryCount;
    uint32_t reserved;
};

struct CacheEntryHeader
{
    uint32_t nameLength;
    uint32_t dataLength;
    uint32_t checksum;
};
#pragma pack(pop)

class IconCache
{
private:
    std::unordered_map<std::string, std::vector<uint8_t>> m_cache;
    std::mutex m_mutex;
    std::string m_cacheFilePath;
    bool m_dirty = false;

public:
    IconCache(const std::string& cacheFile = GetIconCachePath())
        : m_cacheFilePath(cacheFile)
    {
        LoadFromDisk();
    }

    ~IconCache()
    {
        SaveToDisk();
    }

    const std::vector<uint8_t>* GetIconData(const std::string& itemName)
    {
        SPOOF_FUNC;
        std::lock_guard<std::mutex> lock(m_mutex);

        std::string fileName = ItemNameToFileName(itemName);

        auto it = m_cache.find(fileName);

        if (it == m_cache.end())
        {
            if (!DownloadAndCache(fileName))
                return nullptr;

            it = m_cache.find(fileName);
            if (it == m_cache.end())
                return nullptr;
        }

        return &it->second;
    }

    bool HasIcon(const std::string& itemName)
    {
        SPOOF_FUNC;
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string fileName = ItemNameToFileName(itemName);
        return m_cache.find(fileName) != m_cache.end();
    }

    void Flush()
    {
        SPOOF_FUNC;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_dirty)
        {
            SaveToDiskInternal();
            m_dirty = false;
        }
    }

    size_t GetCachedCount() const { return m_cache.size(); }

private:
    static std::string ItemNameToFileName(const std::string& itemName)
    {
        SPOOF_FUNC;
        std::string fileName = itemName;
        std::transform(fileName.begin(), fileName.end(), fileName.begin(),
            [](unsigned char c) { return std::tolower(c); });
        std::replace(fileName.begin(), fileName.end(), ' ', '_');
        fileName += ".png";
        return fileName;
    }


    void SaveToDiskInternal()
    {
        SPOOF_FUNC;
        std::ofstream file(m_cacheFilePath, std::ios::binary);
        if (!file)
            return;

        CacheHeader header;
        header.magic = CACHE_MAGIC;
        header.version = CACHE_VERSION;
        header.entryCount = static_cast<uint32_t>(m_cache.size());
        header.reserved = 0;

        file.write(reinterpret_cast<const char*>(&header), sizeof(header));

        for (const auto& [name, pngData] : m_cache)
        {
            std::vector<uint8_t> encryptedData = pngData;
            IconCrypto::EncryptDecrypt(encryptedData, name);

            CacheEntryHeader entryHeader;
            entryHeader.nameLength = static_cast<uint32_t>(name.size());
            entryHeader.dataLength = static_cast<uint32_t>(encryptedData.size());
            entryHeader.checksum = CalculateChecksum(pngData);

            file.write(reinterpret_cast<const char*>(&entryHeader), sizeof(entryHeader));
            file.write(name.data(), name.size());
            file.write(reinterpret_cast<const char*>(encryptedData.data()), encryptedData.size());
        }
    }

    bool DownloadAndCache(const std::string&)
    {
        SPOOF_FUNC;
        return false;
    }

    static uint32_t CalculateChecksum(const std::vector<uint8_t>& data)
    {
        SPOOF_FUNC;
        uint32_t crc = 0xFFFFFFFF;
        for (uint8_t byte : data)
        {
            crc ^= byte;
            for (int i = 0; i < 8; i++)
                crc = (crc >> 1) ^ (0xEDB88320 & (0 - (crc & 1)));
        }
        return ~crc;
    }


    void SaveToDisk()
    {
        SPOOF_FUNC;
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_dirty)
        {
            SaveToDiskInternal();
            m_dirty = false;
        }
    }

    void LoadFromDisk()
    {
        SPOOF_FUNC;
        std::ifstream file(m_cacheFilePath, std::ios::binary);
        if (!file)
            return;

        CacheHeader header;
        file.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (header.magic != CACHE_MAGIC || header.version != CACHE_VERSION)
            return;

        for (uint32_t i = 0; i < header.entryCount; i++)
        {
            CacheEntryHeader entryHeader;
            file.read(reinterpret_cast<char*>(&entryHeader), sizeof(entryHeader));

            if (!file)
                break;

            std::string name(entryHeader.nameLength, '\0');
            file.read(name.data(), entryHeader.nameLength);

            std::vector<uint8_t> encryptedData(entryHeader.dataLength);
            file.read(reinterpret_cast<char*>(encryptedData.data()), entryHeader.dataLength);

            if (!file)
                break;

            IconCrypto::EncryptDecrypt(encryptedData, name);

            if (CalculateChecksum(encryptedData) != entryHeader.checksum)
                continue;

            m_cache[name] = std::move(encryptedData);
        }
    }
};

inline IconCache g_IconCache;

inline const std::vector<uint8_t>* GetItemIcon(const std::string& itemName)
{
    return g_IconCache.GetIconData(itemName);
}


static const std::unordered_set<std::string> WEAPON_NAMES = {
            "burletta",
            "venator",
            "hairpin",
            "anvil",
            "rattler",
            "kettle",
            "arpeggio",
            "tempest",
            "bettina",
            "ferro",
            "renegade",
            "aphelion",
            "osprey",
            "jupiter",
            "equalizer",
            "il toro",
            "vulcano",
            "torrente",
            "stitcher",
            "bobcat",
            "hullcracker"
};

class ItemDataCache
{
private:
    std::unordered_map<std::string, int> m_valueCache;
    std::mutex m_mutex;

public:
    int GetItemValue(const std::string& itemName)
    {
        SPOOF_FUNC;
        std::lock_guard<std::mutex> lock(m_mutex);

        std::string fileName = ItemNameToFileName(itemName);

        auto it = m_valueCache.find(fileName);
        if (it != m_valueCache.end())
            return it->second;

        int value = DownloadAndParseValue(fileName);
        m_valueCache[fileName] = value;

        return value;
    }

    bool HasItem(const std::string& itemName)
    {
        SPOOF_FUNC;
        std::lock_guard<std::mutex> lock(m_mutex);
        std::string fileName = ItemNameToFileName(itemName);
        return m_valueCache.find(fileName) != m_valueCache.end();
    }

    size_t GetCachedCount() const { return m_valueCache.size(); }

private:

    static bool IsWeapon(const std::string& itemName)
    {
        SPOOF_FUNC;
        std::string lower = itemName;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return WEAPON_NAMES.find(lower) != WEAPON_NAMES.end();
    }

    static std::string ItemNameToFileName(const std::string& itemName)
    {
        SPOOF_FUNC;
        std::string fileName = itemName;
        std::transform(fileName.begin(), fileName.end(), fileName.begin(),
            [](unsigned char c) { return std::tolower(c); });
        std::replace(fileName.begin(), fileName.end(), ' ', '_');

        if (IsWeapon(itemName))
        {
            fileName += "_ii";
        }

        fileName += ".json";
        return fileName;
    }
    int DownloadAndParseValue(const std::string&)
    {
        SPOOF_FUNC;
        return -1;
    }

    int ParseValueFromJson(const std::string& json)
    {
        SPOOF_FUNC;
        std::string key = "\"value\":";
        size_t pos = 0;

        while ((pos = json.find(key, pos)) != std::string::npos)
        {
            pos += key.length();

            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
                pos++;

            std::string numStr;
            while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9')
            {
                numStr += json[pos];
                pos++;
            }

            if (numStr.empty())
                continue;

            int value = std::stoi(numStr);

            if (value >= 100)
                return value;
        }

        return -1;
    }
};

inline ItemDataCache g_ItemDataCache;

inline int GetItemValue(const std::string& itemName)
{
    return g_ItemDataCache.GetItemValue(itemName);
}



inline ImTextureID CreateIconTexture(ID3D11Device* device, const std::vector<uint8_t>* pngData)
{
    SPOOF_FUNC;
    if (!device || !pngData || pngData->empty())
        return nullptr;

    int w, h, channels;
    unsigned char* data = stbi_load_from_memory(pngData->data(), (int)pngData->size(), &w, &h, &channels, 4);
    if (!data)
        return nullptr;

    ID3D11DeviceContext* ctx = nullptr;
    device->GetImmediateContext(&ctx);
    if (!ctx)
    {
        stbi_image_free(data);
        return nullptr;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 0;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* pTexture = nullptr;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &pTexture);

    if (FAILED(hr) || !pTexture)
    {
        stbi_image_free(data);
        ctx->Release();
        return nullptr;
    }

    ctx->UpdateSubresource(pTexture, 0, nullptr, data, w * 4, 0);

    stbi_image_free(data);

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = -1;

    ID3D11ShaderResourceView* srv = nullptr;
    hr = device->CreateShaderResourceView(pTexture, &srvDesc, &srv);

    if (FAILED(hr))
    {
        pTexture->Release();
        ctx->Release();
        return nullptr;
    }

    ctx->GenerateMips(srv);

    pTexture->Release();
    ctx->Release();

    return (ImTextureID)srv;
}

inline ImTextureID CreateIconTextureDX9(LPDIRECT3DDEVICE9 device, const std::vector<uint8_t>* pngData)
{
    SPOOF_FUNC;
    if (!device || !pngData || pngData->empty())
        return nullptr;

    int w, h, channels;
    unsigned char* data = stbi_load_from_memory(pngData->data(), (int)pngData->size(), &w, &h, &channels, 4);
    if (!data)
        return nullptr;

    LPDIRECT3DTEXTURE9 tex = nullptr;
    HRESULT hr = device->CreateTexture(w, h, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &tex, nullptr);
    if (FAILED(hr) || !tex) {
        stbi_image_free(data);
        return nullptr;
    }

    D3DLOCKED_RECT lr;
    if (SUCCEEDED(tex->LockRect(0, &lr, nullptr, D3DLOCK_DISCARD))) {
        for (int y = 0; y < h; y++) {
            unsigned char* src = data + y * w * 4;
            unsigned char* dst = (unsigned char*)lr.pBits + y * lr.Pitch;
            for (int x = 0; x < w; x++) {
                dst[x * 4 + 0] = src[x * 4 + 2];
                dst[x * 4 + 1] = src[x * 4 + 1];
                dst[x * 4 + 2] = src[x * 4 + 0];
                dst[x * 4 + 3] = src[x * 4 + 3];
            }
        }
        tex->UnlockRect(0);
    }

    stbi_image_free(data);
    return (ImTextureID)tex;
}

inline ImTextureID GetOrCreateLootIcon(LPDIRECT3DDEVICE9 device, const std::string& itemName)
{
    SPOOF_FUNC;
    auto it = g_LootTextureCache.find(itemName);
    if (it != g_LootTextureCache.end())
        return it->second;

    const auto* iconData = GetItemIcon(itemName);
    if (!iconData || iconData->empty()) {
        g_LootTextureCache[itemName] = nullptr;
        return nullptr;
    }

    ImTextureID tex = CreateIconTextureDX9(device, iconData);
    g_LootTextureCache[itemName] = tex;
    return tex;
}