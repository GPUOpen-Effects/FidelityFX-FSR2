//--------------------------------------------------------------------------------------
// File: DDSTextureLoader.cpp
//
// Functions for loading a DDS texture and creating a Direct3D runtime resource for it
//
// Note these functions are useful as a light-weight runtime loader for DDS files. For
// a full-featured DDS file reader, writer, and texture processing pipeline see
// the 'Texconv' sample and the 'DirectXTex' library.
//
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248926
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "dxut.h"
#include "DDSTextureLoader.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <new>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

using namespace DirectX;

//--------------------------------------------------------------------------------------
// Macros
//--------------------------------------------------------------------------------------
#ifndef MAKEFOURCC
    #define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
                ((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) |       \
                ((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24 ))
#endif /* defined(MAKEFOURCC) */

//--------------------------------------------------------------------------------------
// DDS file structure definitions
//
// See DDS.h in the 'Texconv' sample and the 'DirectXTex' library
//--------------------------------------------------------------------------------------
#pragma pack(push,1)

const uint32_t DDS_MAGIC = 0x20534444; // "DDS "

struct DDS_PIXELFORMAT
{
    uint32_t    size;
    uint32_t    flags;
    uint32_t    fourCC;
    uint32_t    RGBBitCount;
    uint32_t    RBitMask;
    uint32_t    GBitMask;
    uint32_t    BBitMask;
    uint32_t    ABitMask;
};

#define DDS_FOURCC      0x00000004  // DDPF_FOURCC
#define DDS_RGB         0x00000040  // DDPF_RGB
#define DDS_LUMINANCE   0x00020000  // DDPF_LUMINANCE
#define DDS_ALPHA       0x00000002  // DDPF_ALPHA
#define DDS_BUMPDUDV    0x00080000  // DDPF_BUMPDUDV

#define DDS_HEADER_FLAGS_VOLUME         0x00800000  // DDSD_DEPTH

#define DDS_HEIGHT 0x00000002 // DDSD_HEIGHT

#define DDS_CUBEMAP_POSITIVEX 0x00000600 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEX
#define DDS_CUBEMAP_NEGATIVEX 0x00000a00 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEX
#define DDS_CUBEMAP_POSITIVEY 0x00001200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEY
#define DDS_CUBEMAP_NEGATIVEY 0x00002200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEY
#define DDS_CUBEMAP_POSITIVEZ 0x00004200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_POSITIVEZ
#define DDS_CUBEMAP_NEGATIVEZ 0x00008200 // DDSCAPS2_CUBEMAP | DDSCAPS2_CUBEMAP_NEGATIVEZ

#define DDS_CUBEMAP_ALLFACES ( DDS_CUBEMAP_POSITIVEX | DDS_CUBEMAP_NEGATIVEX |\
                               DDS_CUBEMAP_POSITIVEY | DDS_CUBEMAP_NEGATIVEY |\
                               DDS_CUBEMAP_POSITIVEZ | DDS_CUBEMAP_NEGATIVEZ )

#define DDS_CUBEMAP 0x00000200 // DDSCAPS2_CUBEMAP

enum DDS_MISC_FLAGS2
{
    DDS_MISC_FLAGS2_ALPHA_MODE_MASK = 0x7L,
};

struct DDS_HEADER
{
    uint32_t        size;
    uint32_t        flags;
    uint32_t        height;
    uint32_t        width;
    uint32_t        pitchOrLinearSize;
    uint32_t        depth; // only if DDS_HEADER_FLAGS_VOLUME is set in flags
    uint32_t        mipMapCount;
    uint32_t        reserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t        caps;
    uint32_t        caps2;
    uint32_t        caps3;
    uint32_t        caps4;
    uint32_t        reserved2;
};

struct DDS_HEADER_DXT10
{
    DXGI_FORMAT     dxgiFormat;
    uint32_t        resourceDimension;
    uint32_t        miscFlag; // see D3D11_RESOURCE_MISC_FLAG
    uint32_t        arraySize;
    uint32_t        miscFlags2;
};

#pragma pack(pop)

//--------------------------------------------------------------------------------------
namespace
{
    struct handle_closer { void operator()(HANDLE h) noexcept { if (h) CloseHandle(h); } };

    using ScopedHandle = std::unique_ptr<void, handle_closer>;

    inline HANDLE safe_handle(HANDLE h) noexcept { return (h == INVALID_HANDLE_VALUE) ? nullptr : h; }

    template<UINT TNameLength>
    inline void SetDebugObjectName(_In_ ID3D11DeviceChild* resource, _In_ const char (&name)[TNameLength]) noexcept
    {
    #if defined(_DEBUG) || defined(PROFILE)
        resource->SetPrivateData(WKPDID_D3DDebugObjectName, TNameLength - 1, name);
    #else
        UNREFERENCED_PARAMETER(resource);
        UNREFERENCED_PARAMETER(name);
    #endif
    }

    //--------------------------------------------------------------------------------------
    HRESULT LoadTextureDataFromMemory(
        _In_reads_(ddsDataSize) const uint8_t* ddsData,
        size_t ddsDataSize,
        const DDS_HEADER** header,
        const uint8_t** bitData,
        size_t* bitSize) noexcept
    {
        if (!header || !bitData || !bitSize)
        {
            return E_POINTER;
        }

        *bitSize = 0;

        if (ddsDataSize > UINT32_MAX)
        {
            return E_FAIL;
        }

        if (ddsDataSize < (sizeof(uint32_t) + sizeof(DDS_HEADER)))
        {
            return E_FAIL;
        }

        // DDS files always start with the same magic number ("DDS ")
        auto dwMagicNumber = *reinterpret_cast<const uint32_t*>(ddsData);
        if (dwMagicNumber != DDS_MAGIC)
        {
            return E_FAIL;
        }

        auto hdr = reinterpret_cast<const DDS_HEADER*>(ddsData + sizeof(uint32_t));

        // Verify header to validate DDS file
        if (hdr->size != sizeof(DDS_HEADER) ||
            hdr->ddspf.size != sizeof(DDS_PIXELFORMAT))
        {
            return E_FAIL;
        }

        // Check for DX10 extension
        bool bDXT10Header = false;
        if ((hdr->ddspf.flags & DDS_FOURCC) &&
            (MAKEFOURCC('D', 'X', '1', '0') == hdr->ddspf.fourCC))
        {
            // Must be long enough for both headers and magic value
            if (ddsDataSize < (sizeof(uint32_t) + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10)))
            {
                return E_FAIL;
            }

            bDXT10Header = true;
        }

        // setup the pointers in the process request
        *header = hdr;
        auto offset = sizeof(uint32_t)
            + sizeof(DDS_HEADER)
            + (bDXT10Header ? sizeof(DDS_HEADER_DXT10) : 0);
        *bitData = ddsData + offset;
        *bitSize = ddsDataSize - offset;

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    HRESULT LoadTextureDataFromFile(
        _In_z_ const wchar_t* fileName,
        std::unique_ptr<uint8_t[]>& ddsData,
        const DDS_HEADER** header,
        const uint8_t** bitData,
        size_t* bitSize) noexcept
    {
        if (!header || !bitData || !bitSize)
        {
            return E_POINTER;
        }

        *bitSize = 0;

        // open the file
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
        ScopedHandle hFile(safe_handle(CreateFile2(fileName,
            GENERIC_READ,
            FILE_SHARE_READ,
            OPEN_EXISTING,
            nullptr)));
#else
        ScopedHandle hFile(safe_handle(CreateFileW(fileName,
            GENERIC_READ,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr)));
#endif

        if (!hFile)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        // Get the file size
        FILE_STANDARD_INFO fileInfo;
        if (!GetFileInformationByHandleEx(hFile.get(), FileStandardInfo, &fileInfo, sizeof(fileInfo)))
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        // File is too big for 32-bit allocation, so reject read
        if (fileInfo.EndOfFile.HighPart > 0)
        {
            return E_FAIL;
        }

        // Need at least enough data to fill the header and magic number to be a valid DDS
        if (fileInfo.EndOfFile.LowPart < (sizeof(uint32_t) + sizeof(DDS_HEADER)))
        {
            return E_FAIL;
        }

        // create enough space for the file data
        ddsData.reset(new (std::nothrow) uint8_t[fileInfo.EndOfFile.LowPart]);
        if (!ddsData)
        {
            return E_OUTOFMEMORY;
        }

        // read the data in
        DWORD bytesRead = 0;
        if (!ReadFile(hFile.get(),
            ddsData.get(),
            fileInfo.EndOfFile.LowPart,
            &bytesRead,
            nullptr
        ))
        {
            ddsData.reset();
            return HRESULT_FROM_WIN32(GetLastError());
        }

        if (bytesRead < fileInfo.EndOfFile.LowPart)
        {
            ddsData.reset();
            return E_FAIL;
        }

        // DDS files always start with the same magic number ("DDS ")
        auto dwMagicNumber = *reinterpret_cast<const uint32_t*>(ddsData.get());
        if (dwMagicNumber != DDS_MAGIC)
        {
            ddsData.reset();
            return E_FAIL;
        }

        auto hdr = reinterpret_cast<const DDS_HEADER*>(ddsData.get() + sizeof(uint32_t));

        // Verify header to validate DDS file
        if (hdr->size != sizeof(DDS_HEADER) ||
            hdr->ddspf.size != sizeof(DDS_PIXELFORMAT))
        {
            ddsData.reset();
            return E_FAIL;
        }

        // Check for DX10 extension
        bool bDXT10Header = false;
        if ((hdr->ddspf.flags & DDS_FOURCC) &&
            (MAKEFOURCC('D', 'X', '1', '0') == hdr->ddspf.fourCC))
        {
            // Must be long enough for both headers and magic value
            if (fileInfo.EndOfFile.LowPart < (sizeof(uint32_t) + sizeof(DDS_HEADER) + sizeof(DDS_HEADER_DXT10)))
            {
                ddsData.reset();
                return E_FAIL;
            }

            bDXT10Header = true;
        }

        // setup the pointers in the process request
        *header = hdr;
        auto offset = sizeof(uint32_t) + sizeof(DDS_HEADER)
            + (bDXT10Header ? sizeof(DDS_HEADER_DXT10) : 0);
        *bitData = ddsData.get() + offset;
        *bitSize = fileInfo.EndOfFile.LowPart - offset;

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    // Return the BPP for a particular format
    //--------------------------------------------------------------------------------------
    size_t BitsPerPixel(_In_ DXGI_FORMAT fmt) noexcept
    {
        switch (fmt)
        {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return 128;

        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
            return 96;

        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        case DXGI_FORMAT_Y416:
        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y216:
            return 64;

        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
        case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
        case DXGI_FORMAT_B8G8R8A8_TYPELESS:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8X8_TYPELESS:
        case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
        case DXGI_FORMAT_AYUV:
        case DXGI_FORMAT_Y410:
        case DXGI_FORMAT_YUY2:
            return 32;

        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
            return 24;

        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
        case DXGI_FORMAT_A8P8:
        case DXGI_FORMAT_B4G4R4A4_UNORM:
            return 16;

        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_420_OPAQUE:
        case DXGI_FORMAT_NV11:
            return 12;

        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
        case DXGI_FORMAT_AI44:
        case DXGI_FORMAT_IA44:
        case DXGI_FORMAT_P8:
            return 8;

        case DXGI_FORMAT_R1_UNORM:
            return 1;

        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            return 4;

        default:
            return 0;
        }
    }


    //--------------------------------------------------------------------------------------
    // Get surface information for a particular format
    //--------------------------------------------------------------------------------------
    HRESULT GetSurfaceInfo(
        _In_ size_t width,
        _In_ size_t height,
        _In_ DXGI_FORMAT fmt,
        size_t* outNumBytes,
        _Out_opt_ size_t* outRowBytes,
        _Out_opt_ size_t* outNumRows) noexcept
    {
        uint64_t numBytes = 0;
        uint64_t rowBytes = 0;
        uint64_t numRows = 0;

        bool bc = false;
        bool packed = false;
        bool planar = false;
        size_t bpe = 0;
        switch (fmt)
        {
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            bc = true;
            bpe = 8;
            break;

        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
        case DXGI_FORMAT_BC6H_TYPELESS:
        case DXGI_FORMAT_BC6H_UF16:
        case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_TYPELESS:
        case DXGI_FORMAT_BC7_UNORM:
        case DXGI_FORMAT_BC7_UNORM_SRGB:
            bc = true;
            bpe = 16;
            break;

        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
        case DXGI_FORMAT_YUY2:
            packed = true;
            bpe = 4;
            break;

        case DXGI_FORMAT_Y210:
        case DXGI_FORMAT_Y216:
            packed = true;
            bpe = 8;
            break;

        case DXGI_FORMAT_NV12:
        case DXGI_FORMAT_420_OPAQUE:
            planar = true;
            bpe = 2;
            break;

        case DXGI_FORMAT_P010:
        case DXGI_FORMAT_P016:
            planar = true;
            bpe = 4;
            break;

        default:
            break;
        }

        if (bc)
        {
            uint64_t numBlocksWide = 0;
            if (width > 0)
            {
                numBlocksWide = std::max<uint64_t>(1u, (uint64_t(width) + 3u) / 4u);
            }
            uint64_t numBlocksHigh = 0;
            if (height > 0)
            {
                numBlocksHigh = std::max<uint64_t>(1u, (uint64_t(height) + 3u) / 4u);
            }
            rowBytes = numBlocksWide * bpe;
            numRows = numBlocksHigh;
            numBytes = rowBytes * numBlocksHigh;
        }
        else if (packed)
        {
            rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
            numRows = uint64_t(height);
            numBytes = rowBytes * height;
        }
        else if (fmt == DXGI_FORMAT_NV11)
        {
            rowBytes = ((uint64_t(width) + 3u) >> 2) * 4u;
            numRows = uint64_t(height) * 2u; // Direct3D makes this simplifying assumption, although it is larger than the 4:1:1 data
            numBytes = rowBytes * numRows;
        }
        else if (planar)
        {
            rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
            numBytes = (rowBytes * uint64_t(height)) + ((rowBytes * uint64_t(height) + 1u) >> 1);
            numRows = height + ((uint64_t(height) + 1u) >> 1);
        }
        else
        {
            size_t bpp = BitsPerPixel(fmt);
            if (!bpp)
                return E_INVALIDARG;

            rowBytes = (uint64_t(width) * bpp + 7u) / 8u; // round up to nearest byte
            numRows = uint64_t(height);
            numBytes = rowBytes * height;
        }

#if defined(_M_IX86) || defined(_M_ARM) || defined(_M_HYBRID_X86_ARM64)
        static_assert(sizeof(size_t) == 4, "Not a 32-bit platform!");
        if (numBytes > UINT32_MAX || rowBytes > UINT32_MAX || numRows > UINT32_MAX)
            return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
#else
        static_assert(sizeof(size_t) == 8, "Not a 64-bit platform!");
#endif

        if (outNumBytes)
        {
            *outNumBytes = static_cast<size_t>(numBytes);
        }
        if (outRowBytes)
        {
            *outRowBytes = static_cast<size_t>(rowBytes);
        }
        if (outNumRows)
        {
            *outNumRows = static_cast<size_t>(numRows);
        }

        return S_OK;
    }


    //--------------------------------------------------------------------------------------
    #define ISBITMASK( r,g,b,a ) ( ddpf.RBitMask == r && ddpf.GBitMask == g && ddpf.BBitMask == b && ddpf.ABitMask == a )

    DXGI_FORMAT GetDXGIFormat(const DDS_PIXELFORMAT& ddpf) noexcept
    {
        if (ddpf.flags & DDS_RGB)
        {
            // Note that sRGB formats are written using the "DX10" extended header

            switch (ddpf.RGBBitCount)
            {
            case 32:
                if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
                {
                    return DXGI_FORMAT_R8G8B8A8_UNORM;
                }

                if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000))
                {
                    return DXGI_FORMAT_B8G8R8A8_UNORM;
                }

                if (ISBITMASK(0x00ff0000, 0x0000ff00, 0x000000ff, 0))
                {
                    return DXGI_FORMAT_B8G8R8X8_UNORM;
                }

                // No DXGI format maps to ISBITMASK(0x000000ff,0x0000ff00,0x00ff0000,0) aka D3DFMT_X8B8G8R8

                // Note that many common DDS reader/writers (including D3DX) swap the
                // the RED/BLUE masks for 10:10:10:2 formats. We assume
                // below that the 'backwards' header mask is being used since it is most
                // likely written by D3DX. The more robust solution is to use the 'DX10'
                // header extension and specify the DXGI_FORMAT_R10G10B10A2_UNORM format directly

                // For 'correct' writers, this should be 0x000003ff,0x000ffc00,0x3ff00000 for RGB data
                if (ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000))
                {
                    return DXGI_FORMAT_R10G10B10A2_UNORM;
                }

                // No DXGI format maps to ISBITMASK(0x000003ff,0x000ffc00,0x3ff00000,0xc0000000) aka D3DFMT_A2R10G10B10

                if (ISBITMASK(0x0000ffff, 0xffff0000, 0, 0))
                {
                    return DXGI_FORMAT_R16G16_UNORM;
                }

                if (ISBITMASK(0xffffffff, 0, 0, 0))
                {
                    // Only 32-bit color channel format in D3D9 was R32F
                    return DXGI_FORMAT_R32_FLOAT; // D3DX writes this out as a FourCC of 114
                }
                break;

            case 24:
                // No 24bpp DXGI formats aka D3DFMT_R8G8B8
                break;

            case 16:
                if (ISBITMASK(0x7c00, 0x03e0, 0x001f, 0x8000))
                {
                    return DXGI_FORMAT_B5G5R5A1_UNORM;
                }
                if (ISBITMASK(0xf800, 0x07e0, 0x001f, 0))
                {
                    return DXGI_FORMAT_B5G6R5_UNORM;
                }

                // No DXGI format maps to ISBITMASK(0x7c00,0x03e0,0x001f,0) aka D3DFMT_X1R5G5B5

                if (ISBITMASK(0x0f00, 0x00f0, 0x000f, 0xf000))
                {
                    return DXGI_FORMAT_B4G4R4A4_UNORM;
                }

                // NVTT versions 1.x wrote this as RGB instead of LUMINANCE
                if (ISBITMASK(0x00ff, 0, 0, 0xff00))
                {
                    return DXGI_FORMAT_R8G8_UNORM;
                }
                if (ISBITMASK(0xffff, 0, 0, 0))
                {
                    return DXGI_FORMAT_R16_UNORM;
                }

                // No DXGI format maps to ISBITMASK(0x0f00,0x00f0,0x000f,0) aka D3DFMT_X4R4G4B4

                // No 3:3:2:8 or paletted DXGI formats aka D3DFMT_A8R3G3B2, D3DFMT_A8P8, etc.
                break;

            case 8:
                // NVTT versions 1.x wrote this as RGB instead of LUMINANCE
                if (ISBITMASK(0xff, 0, 0, 0))
                {
                    return DXGI_FORMAT_R8_UNORM;
                }

                // No 3:3:2 or paletted DXGI formats aka D3DFMT_R3G3B2, D3DFMT_P8
                break;
            }
        }
        else if (ddpf.flags & DDS_LUMINANCE)
        {
            switch (ddpf.RGBBitCount)
            {
            case 16:
                if (ISBITMASK(0xffff, 0, 0, 0))
                {
                    return DXGI_FORMAT_R16_UNORM; // D3DX10/11 writes this out as DX10 extension
                }
                if (ISBITMASK(0x00ff, 0, 0, 0xff00))
                {
                    return DXGI_FORMAT_R8G8_UNORM; // D3DX10/11 writes this out as DX10 extension
                }
                break;

            case 8:
                if (ISBITMASK(0xff, 0, 0, 0))
                {
                    return DXGI_FORMAT_R8_UNORM; // D3DX10/11 writes this out as DX10 extension
                }

                // No DXGI format maps to ISBITMASK(0x0f,0,0,0xf0) aka D3DFMT_A4L4

                if (ISBITMASK(0x00ff, 0, 0, 0xff00))
                {
                    return DXGI_FORMAT_R8G8_UNORM; // Some DDS writers assume the bitcount should be 8 instead of 16
                }
                break;
            }
        }
        else if (ddpf.flags & DDS_ALPHA)
        {
            if (8 == ddpf.RGBBitCount)
            {
                return DXGI_FORMAT_A8_UNORM;
            }
        }
        else if (ddpf.flags & DDS_BUMPDUDV)
        {
            switch (ddpf.RGBBitCount)
            {
            case 32:
                if (ISBITMASK(0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000))
                {
                    return DXGI_FORMAT_R8G8B8A8_SNORM; // D3DX10/11 writes this out as DX10 extension
                }
                if (ISBITMASK(0x0000ffff, 0xffff0000, 0, 0))
                {
                    return DXGI_FORMAT_R16G16_SNORM; // D3DX10/11 writes this out as DX10 extension
                }

                // No DXGI format maps to ISBITMASK(0x3ff00000, 0x000ffc00, 0x000003ff, 0xc0000000) aka D3DFMT_A2W10V10U10
                break;

            case 16:
                if (ISBITMASK(0x00ff, 0xff00, 0, 0))
                {
                    return DXGI_FORMAT_R8G8_SNORM; // D3DX10/11 writes this out as DX10 extension
                }
                break;
            }

            // No DXGI format maps to DDPF_BUMPLUMINANCE aka D3DFMT_L6V5U5, D3DFMT_X8L8V8U8
        }
        else if (ddpf.flags & DDS_FOURCC)
        {
            if (MAKEFOURCC('D', 'X', 'T', '1') == ddpf.fourCC)
            {
                return DXGI_FORMAT_BC1_UNORM;
            }
            if (MAKEFOURCC('D', 'X', 'T', '3') == ddpf.fourCC)
            {
                return DXGI_FORMAT_BC2_UNORM;
            }
            if (MAKEFOURCC('D', 'X', 'T', '5') == ddpf.fourCC)
            {
                return DXGI_FORMAT_BC3_UNORM;
            }

            // While pre-multiplied alpha isn't directly supported by the DXGI formats,
            // they are basically the same as these BC formats so they can be mapped
            if (MAKEFOURCC('D', 'X', 'T', '2') == ddpf.fourCC)
            {
                return DXGI_FORMAT_BC2_UNORM;
            }
            if (MAKEFOURCC('D', 'X', 'T', '4') == ddpf.fourCC)
            {
                return DXGI_FORMAT_BC3_UNORM;
            }

            if (MAKEFOURCC('A', 'T', 'I', '1') == ddpf.fourCC)
            {
                return DXGI_FORMAT_BC4_UNORM;
            }
            if (MAKEFOURCC('B', 'C', '4', 'U') == ddpf.fourCC)
            {
                return DXGI_FORMAT_BC4_UNORM;
            }
            if (MAKEFOURCC('B', 'C', '4', 'S') == ddpf.fourCC)
            {
                return DXGI_FORMAT_BC4_SNORM;
            }

            if (MAKEFOURCC('A', 'T', 'I', '2') == ddpf.fourCC)
            {
                return DXGI_FORMAT_BC5_UNORM;
            }
            if (MAKEFOURCC('B', 'C', '5', 'U') == ddpf.fourCC)
            {
                return DXGI_FORMAT_BC5_UNORM;
            }
            if (MAKEFOURCC('B', 'C', '5', 'S') == ddpf.fourCC)
            {
                return DXGI_FORMAT_BC5_SNORM;
            }

            // BC6H and BC7 are written using the "DX10" extended header

            if (MAKEFOURCC('R', 'G', 'B', 'G') == ddpf.fourCC)
            {
                return DXGI_FORMAT_R8G8_B8G8_UNORM;
            }
            if (MAKEFOURCC('G', 'R', 'G', 'B') == ddpf.fourCC)
            {
                return DXGI_FORMAT_G8R8_G8B8_UNORM;
            }

            if (MAKEFOURCC('Y', 'U', 'Y', '2') == ddpf.fourCC)
            {
                return DXGI_FORMAT_YUY2;
            }

            // Check for D3DFORMAT enums being set here
            switch (ddpf.fourCC)
            {
            case 36: // D3DFMT_A16B16G16R16
                return DXGI_FORMAT_R16G16B16A16_UNORM;

            case 110: // D3DFMT_Q16W16V16U16
                return DXGI_FORMAT_R16G16B16A16_SNORM;

            case 111: // D3DFMT_R16F
                return DXGI_FORMAT_R16_FLOAT;

            case 112: // D3DFMT_G16R16F
                return DXGI_FORMAT_R16G16_FLOAT;

            case 113: // D3DFMT_A16B16G16R16F
                return DXGI_FORMAT_R16G16B16A16_FLOAT;

            case 114: // D3DFMT_R32F
                return DXGI_FORMAT_R32_FLOAT;

            case 115: // D3DFMT_G32R32F
                return DXGI_FORMAT_R32G32_FLOAT;

            case 116: // D3DFMT_A32B32G32R32F
                return DXGI_FORMAT_R32G32B32A32_FLOAT;

            // No DXGI format maps to D3DFMT_CxV8U8
            }
        }

        return DXGI_FORMAT_UNKNOWN;
    }

    #undef ISBITMASK


    //--------------------------------------------------------------------------------------
    DXGI_FORMAT MakeSRGB(_In_ DXGI_FORMAT format) noexcept
    {
        switch (format)
        {
        case DXGI_FORMAT_R8G8B8A8_UNORM:
            return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

        case DXGI_FORMAT_BC1_UNORM:
            return DXGI_FORMAT_BC1_UNORM_SRGB;

        case DXGI_FORMAT_BC2_UNORM:
            return DXGI_FORMAT_BC2_UNORM_SRGB;

        case DXGI_FORMAT_BC3_UNORM:
            return DXGI_FORMAT_BC3_UNORM_SRGB;

        case DXGI_FORMAT_B8G8R8A8_UNORM:
            return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;

        case DXGI_FORMAT_B8G8R8X8_UNORM:
            return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;

        case DXGI_FORMAT_BC7_UNORM:
            return DXGI_FORMAT_BC7_UNORM_SRGB;

        default:
            return format;
        }
    }


    //--------------------------------------------------------------------------------------
    HRESULT FillInitData(
        _In_ size_t width,
        _In_ size_t height,
        _In_ size_t depth,
        _In_ size_t mipCount,
        _In_ size_t arraySize,
        _In_ DXGI_FORMAT format,
        _In_ size_t maxsize,
        _In_ size_t bitSize,
        _In_reads_bytes_(bitSize) const uint8_t* bitData,
        _Out_ size_t& twidth,
        _Out_ size_t& theight,
        _Out_ size_t& tdepth,
        _Out_ size_t& skipMip,
        _Out_writes_(mipCount*arraySize) D3D11_SUBRESOURCE_DATA* initData) noexcept
    {
        if (!bitData || !initData)
        {
            return E_POINTER;
        }

        skipMip = 0;
        twidth = 0;
        theight = 0;
        tdepth = 0;

        size_t NumBytes = 0;
        size_t RowBytes = 0;
        const uint8_t* pSrcBits = bitData;
        const uint8_t* pEndBits = bitData + bitSize;

        size_t index = 0;
        for (size_t j = 0; j < arraySize; j++)
        {
            size_t w = width;
            size_t h = height;
            size_t d = depth;
            for (size_t i = 0; i < mipCount; i++)
            {
                HRESULT hr = GetSurfaceInfo(w, h, format, &NumBytes, &RowBytes, nullptr);
                if (FAILED(hr))
                    return hr;

                if (NumBytes > UINT32_MAX || RowBytes > UINT32_MAX)
                    return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

                if ((mipCount <= 1) || !maxsize || (w <= maxsize && h <= maxsize && d <= maxsize))
                {
                    if (!twidth)
                    {
                        twidth = w;
                        theight = h;
                        tdepth = d;
                    }

                    assert(index < mipCount * arraySize);
                    _Analysis_assume_(index < mipCount * arraySize);
                    initData[index].pSysMem = pSrcBits;
                    initData[index].SysMemPitch = static_cast<UINT>(RowBytes);
                    initData[index].SysMemSlicePitch = static_cast<UINT>(NumBytes);
                    ++index;
                }
                else if (!j)
                {
                    // Count number of skipped mipmaps (first item only)
                    ++skipMip;
                }

                if (pSrcBits + (NumBytes*d) > pEndBits)
                {
                    return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
                }

                pSrcBits += NumBytes * d;

                w = w >> 1;
                h = h >> 1;
                d = d >> 1;
                if (w == 0)
                {
                    w = 1;
                }
                if (h == 0)
                {
                    h = 1;
                }
                if (d == 0)
                {
                    d = 1;
                }
            }
        }

        return (index > 0) ? S_OK : E_FAIL;
    }


    //--------------------------------------------------------------------------------------
    HRESULT CreateD3DResources(
        _In_ ID3D11Device* d3dDevice,
        _In_ uint32_t resDim,
        _In_ size_t width,
        _In_ size_t height,
        _In_ size_t depth,
        _In_ size_t mipCount,
        _In_ size_t arraySize,
        _In_ DXGI_FORMAT format,
        _In_ D3D11_USAGE usage,
        _In_ unsigned int bindFlags,
        _In_ unsigned int cpuAccessFlags,
        _In_ unsigned int miscFlags,
        _In_ bool forceSRGB,
        _In_ bool isCubeMap,
        _In_reads_opt_(mipCount*arraySize) D3D11_SUBRESOURCE_DATA* initData,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView) noexcept
    {
        if (!d3dDevice)
            return E_POINTER;

        HRESULT hr = E_FAIL;

        if (forceSRGB)
        {
            format = MakeSRGB(format);
        }

        switch (resDim)
        {
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
        {
            D3D11_TEXTURE1D_DESC desc;
            desc.Width = static_cast<UINT>(width);
            desc.MipLevels = static_cast<UINT>(mipCount);
            desc.ArraySize = static_cast<UINT>(arraySize);
            desc.Format = format;
            desc.Usage = usage;
            desc.BindFlags = bindFlags;
            desc.CPUAccessFlags = cpuAccessFlags;
            desc.MiscFlags = miscFlags & ~static_cast<unsigned int>(D3D11_RESOURCE_MISC_TEXTURECUBE);

            ID3D11Texture1D* tex = nullptr;
            hr = d3dDevice->CreateTexture1D(&desc,
                initData,
                &tex
            );
            if (SUCCEEDED(hr) && tex)
            {
                if (textureView)
                {
                    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                    SRVDesc.Format = format;

                    if (arraySize > 1)
                    {
                        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
                        SRVDesc.Texture1DArray.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                        SRVDesc.Texture1DArray.ArraySize = static_cast<UINT>(arraySize);
                    }
                    else
                    {
                        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
                        SRVDesc.Texture1D.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                    }

                    hr = d3dDevice->CreateShaderResourceView(tex,
                        &SRVDesc,
                        textureView
                    );
                    if (FAILED(hr))
                    {
                        tex->Release();
                        return hr;
                    }
                }

                if (texture)
                {
                    *texture = tex;
                }
                else
                {
                    SetDebugObjectName(tex, "DDSTextureLoader");
                    tex->Release();
                }
            }
        }
        break;

        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
        {
            D3D11_TEXTURE2D_DESC desc;
            desc.Width = static_cast<UINT>(width);
            desc.Height = static_cast<UINT>(height);
            desc.MipLevels = static_cast<UINT>(mipCount);
            desc.ArraySize = static_cast<UINT>(arraySize);
            desc.Format = format;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = usage;
            desc.BindFlags = bindFlags;
            desc.CPUAccessFlags = cpuAccessFlags;
            if (isCubeMap)
            {
                desc.MiscFlags = miscFlags | D3D11_RESOURCE_MISC_TEXTURECUBE;
            }
            else
            {
                desc.MiscFlags = miscFlags & ~static_cast<unsigned int>(D3D11_RESOURCE_MISC_TEXTURECUBE);
            }

            ID3D11Texture2D* tex = nullptr;
            hr = d3dDevice->CreateTexture2D(&desc,
                initData,
                &tex
            );
            if (SUCCEEDED(hr) && tex)
            {
                if (textureView)
                {
                    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                    SRVDesc.Format = format;

                    if (isCubeMap)
                    {
                        if (arraySize > 6)
                        {
                            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
                            SRVDesc.TextureCubeArray.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;

                            // Earlier we set arraySize to (NumCubes * 6)
                            SRVDesc.TextureCubeArray.NumCubes = static_cast<UINT>(arraySize / 6);
                        }
                        else
                        {
                            SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                            SRVDesc.TextureCube.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                        }
                    }
                    else if (arraySize > 1)
                    {
                        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                        SRVDesc.Texture2DArray.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                        SRVDesc.Texture2DArray.ArraySize = static_cast<UINT>(arraySize);
                    }
                    else
                    {
                        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                        SRVDesc.Texture2D.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;
                    }

                    hr = d3dDevice->CreateShaderResourceView(tex,
                        &SRVDesc,
                        textureView
                    );
                    if (FAILED(hr))
                    {
                        tex->Release();
                        return hr;
                    }
                }

                if (texture)
                {
                    *texture = tex;
                }
                else
                {
                    SetDebugObjectName(tex, "DDSTextureLoader");
                    tex->Release();
                }
            }
        }
        break;

        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        {
            D3D11_TEXTURE3D_DESC desc;
            desc.Width = static_cast<UINT>(width);
            desc.Height = static_cast<UINT>(height);
            desc.Depth = static_cast<UINT>(depth);
            desc.MipLevels = static_cast<UINT>(mipCount);
            desc.Format = format;
            desc.Usage = usage;
            desc.BindFlags = bindFlags;
            desc.CPUAccessFlags = cpuAccessFlags;
            desc.MiscFlags = miscFlags & ~static_cast<unsigned int>(D3D11_RESOURCE_MISC_TEXTURECUBE);

            ID3D11Texture3D* tex = nullptr;
            hr = d3dDevice->CreateTexture3D(&desc,
                initData,
                &tex
            );
            if (SUCCEEDED(hr) && tex)
            {
                if (textureView)
                {
                    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
                    SRVDesc.Format = format;

                    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                    SRVDesc.Texture3D.MipLevels = (!mipCount) ? UINT(-1) : desc.MipLevels;

                    hr = d3dDevice->CreateShaderResourceView(tex,
                        &SRVDesc,
                        textureView
                    );
                    if (FAILED(hr))
                    {
                        tex->Release();
                        return hr;
                    }
                }

                if (texture)
                {
                    *texture = tex;
                }
                else
                {
                    SetDebugObjectName(tex, "DDSTextureLoader");
                    tex->Release();
                }
            }
        }
        break;
        }

        return hr;
    }

    //--------------------------------------------------------------------------------------
    HRESULT CreateTextureFromDDS(
        _In_ ID3D11Device* d3dDevice,
        _In_opt_ ID3D11DeviceContext* d3dContext,
        _In_ const DDS_HEADER* header,
        _In_reads_bytes_(bitSize) const uint8_t* bitData,
        _In_ size_t bitSize,
        _In_ size_t maxsize,
        _In_ D3D11_USAGE usage,
        _In_ unsigned int bindFlags,
        _In_ unsigned int cpuAccessFlags,
        _In_ unsigned int miscFlags,
        _In_ bool forceSRGB,
        _Outptr_opt_ ID3D11Resource** texture,
        _Outptr_opt_ ID3D11ShaderResourceView** textureView) noexcept
    {
        HRESULT hr = S_OK;

        UINT width = header->width;
        UINT height = header->height;
        UINT depth = header->depth;

        uint32_t resDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
        UINT arraySize = 1;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        bool isCubeMap = false;

        size_t mipCount = header->mipMapCount;
        if (0 == mipCount)
        {
            mipCount = 1;
        }

        if ((header->ddspf.flags & DDS_FOURCC) &&
            (MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC))
        {
            auto d3d10ext = reinterpret_cast<const DDS_HEADER_DXT10*>(reinterpret_cast<const uint8_t*>(header) + sizeof(DDS_HEADER));

            arraySize = d3d10ext->arraySize;
            if (arraySize == 0)
            {
                return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
            }

            switch (d3d10ext->dxgiFormat)
            {
            case DXGI_FORMAT_AI44:
            case DXGI_FORMAT_IA44:
            case DXGI_FORMAT_P8:
            case DXGI_FORMAT_A8P8:
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);

            default:
                if (BitsPerPixel(d3d10ext->dxgiFormat) == 0)
                {
                    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                }
            }

            format = d3d10ext->dxgiFormat;

            switch (d3d10ext->resourceDimension)
            {
            case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
                // D3DX writes 1D textures with a fixed Height of 1
                if ((header->flags & DDS_HEIGHT) && height != 1)
                {
                    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                }
                height = depth = 1;
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
                if (d3d10ext->miscFlag & D3D11_RESOURCE_MISC_TEXTURECUBE)
                {
                    arraySize *= 6;
                    isCubeMap = true;
                }
                depth = 1;
                break;

            case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
                if (!(header->flags & DDS_HEADER_FLAGS_VOLUME))
                {
                    return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                }

                if (arraySize > 1)
                {
                    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                }
                break;

            default:
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }

            resDim = d3d10ext->resourceDimension;
        }
        else
        {
            format = GetDXGIFormat(header->ddspf);

            if (format == DXGI_FORMAT_UNKNOWN)
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }

            if (header->flags & DDS_HEADER_FLAGS_VOLUME)
            {
                resDim = D3D11_RESOURCE_DIMENSION_TEXTURE3D;
            }
            else
            {
                if (header->caps2 & DDS_CUBEMAP)
                {
                    // We require all six faces to be defined
                    if ((header->caps2 & DDS_CUBEMAP_ALLFACES) != DDS_CUBEMAP_ALLFACES)
                    {
                        return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                    }

                    arraySize = 6;
                    isCubeMap = true;
                }

                depth = 1;
                resDim = D3D11_RESOURCE_DIMENSION_TEXTURE2D;

                // Note there's no way for a legacy Direct3D 9 DDS to express a '1D' texture
            }

            assert(BitsPerPixel(format) != 0);
        }

        // Bound sizes (for security purposes we don't trust DDS file metadata larger than the D3D 11.x hardware requirements)
        if (mipCount > D3D11_REQ_MIP_LEVELS)
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        switch (resDim)
        {
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
            if ((arraySize > D3D11_REQ_TEXTURE1D_ARRAY_AXIS_DIMENSION) ||
                (width > D3D11_REQ_TEXTURE1D_U_DIMENSION))
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
            break;

        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
            if (isCubeMap)
            {
                // This is the right bound because we set arraySize to (NumCubes*6) above
                if ((arraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) ||
                    (width > D3D11_REQ_TEXTURECUBE_DIMENSION) ||
                    (height > D3D11_REQ_TEXTURECUBE_DIMENSION))
                {
                    return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
                }
            }
            else if ((arraySize > D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION) ||
                     (width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION) ||
                     (height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION))
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
            break;

        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
            if ((arraySize > 1) ||
                (width > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
                (height > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION) ||
                (depth > D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION))
            {
                return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
            }
            break;

        default:
            return HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        }

        bool autogen = false;
        if (mipCount == 1 && d3dContext && textureView) // Must have context and shader-view to auto generate mipmaps
        {
            // See if format is supported for auto-gen mipmaps (varies by feature level)
            UINT fmtSupport = 0;
            hr = d3dDevice->CheckFormatSupport(format, &fmtSupport);
            if (SUCCEEDED(hr) && (fmtSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN))
            {
                // 10level9 feature levels do not support auto-gen mipgen for volume textures
                if ((resDim != D3D11_RESOURCE_DIMENSION_TEXTURE3D)
                    || (d3dDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_10_0))
                {
                    autogen = true;
                }
            }
        }

        if (autogen)
        {
            // Create texture with auto-generated mipmaps
            ID3D11Resource* tex = nullptr;
            hr = CreateD3DResources(d3dDevice,
                resDim, width, height, depth, 0, arraySize,
                format,
                usage,
                bindFlags | D3D11_BIND_RENDER_TARGET,
                cpuAccessFlags,
                miscFlags | D3D11_RESOURCE_MISC_GENERATE_MIPS,
                forceSRGB,
                isCubeMap,
                nullptr,
                &tex, textureView);
            if (SUCCEEDED(hr))
            {
                size_t numBytes = 0;
                size_t rowBytes = 0;
                hr = GetSurfaceInfo(width, height, format, &numBytes, &rowBytes, nullptr);
                if (FAILED(hr))
                    return hr;

                if (numBytes > bitSize)
                {
                    (*textureView)->Release();
                    *textureView = nullptr;
                    tex->Release();
                    return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
                }

                if (numBytes > UINT32_MAX || rowBytes > UINT32_MAX)
                    return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

                D3D11_SHADER_RESOURCE_VIEW_DESC desc;
                (*textureView)->GetDesc(&desc);

                UINT mipLevels = 1;

                switch (desc.ViewDimension)
                {
                case D3D_SRV_DIMENSION_TEXTURE1D:       mipLevels = desc.Texture1D.MipLevels; break;
                case D3D_SRV_DIMENSION_TEXTURE1DARRAY:  mipLevels = desc.Texture1DArray.MipLevels; break;
                case D3D_SRV_DIMENSION_TEXTURE2D:       mipLevels = desc.Texture2D.MipLevels; break;
                case D3D_SRV_DIMENSION_TEXTURE2DARRAY:  mipLevels = desc.Texture2DArray.MipLevels; break;
                case D3D_SRV_DIMENSION_TEXTURECUBE:     mipLevels = desc.TextureCube.MipLevels; break;
                case D3D_SRV_DIMENSION_TEXTURECUBEARRAY:mipLevels = desc.TextureCubeArray.MipLevels; break;
                case D3D_SRV_DIMENSION_TEXTURE3D:       mipLevels = desc.Texture3D.MipLevels; break;
                default:
                    (*textureView)->Release();
                    *textureView = nullptr;
                    tex->Release();
                    return E_UNEXPECTED;
                }

                if (arraySize > 1)
                {
                    const uint8_t* pSrcBits = bitData;
                    const uint8_t* pEndBits = bitData + bitSize;
                    for (UINT item = 0; item < arraySize; ++item)
                    {
                        if ((pSrcBits + numBytes) > pEndBits)
                        {
                            (*textureView)->Release();
                            *textureView = nullptr;
                            tex->Release();
                            return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
                        }

                        UINT res = D3D11CalcSubresource(0, item, mipLevels);
                        d3dContext->UpdateSubresource(tex, res, nullptr, pSrcBits, static_cast<UINT>(rowBytes), static_cast<UINT>(numBytes));
                        pSrcBits += numBytes;
                    }
                }
                else
                {
                    d3dContext->UpdateSubresource(tex, 0, nullptr, bitData, static_cast<UINT>(rowBytes), static_cast<UINT>(numBytes));
                }

                d3dContext->GenerateMips(*textureView);

                if (texture)
                {
                    *texture = tex;
                }
                else
                {
                    tex->Release();
                }
            }
        }
        else
        {
            // Create the texture
            std::unique_ptr<D3D11_SUBRESOURCE_DATA[]> initData(new (std::nothrow) D3D11_SUBRESOURCE_DATA[mipCount * arraySize]);
            if (!initData)
            {
                return E_OUTOFMEMORY;
            }

            size_t skipMip = 0;
            size_t twidth = 0;
            size_t theight = 0;
            size_t tdepth = 0;
            hr = FillInitData(width, height, depth, mipCount, arraySize,
                format, maxsize, bitSize, bitData,
                twidth, theight, tdepth, skipMip, initData.get());

            if (SUCCEEDED(hr))
            {
                hr = CreateD3DResources(d3dDevice,
                    resDim, twidth, theight, tdepth, mipCount - skipMip, arraySize,
                    format,
                    usage, bindFlags, cpuAccessFlags, miscFlags,
                    forceSRGB,
                    isCubeMap,
                    initData.get(),
                    texture, textureView);

                if (FAILED(hr) && !maxsize && (mipCount > 1))
                {
                    // Retry with a maxsize determined by feature level
                    switch (d3dDevice->GetFeatureLevel())
                    {
                    case D3D_FEATURE_LEVEL_9_1:
                    case D3D_FEATURE_LEVEL_9_2:
                        if (isCubeMap)
                        {
                            maxsize = 512u /*D3D_FL9_1_REQ_TEXTURECUBE_DIMENSION*/;
                        }
                        else
                        {
                            maxsize = (resDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
                                ? 256u /*D3D_FL9_1_REQ_TEXTURE3D_U_V_OR_W_DIMENSION*/
                                : 2048u /*D3D_FL9_1_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                        }
                        break;

                    case D3D_FEATURE_LEVEL_9_3:
                        maxsize = (resDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
                            ? 256u /*D3D_FL9_1_REQ_TEXTURE3D_U_V_OR_W_DIMENSION*/
                            : 4096u /*D3D_FL9_3_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                        break;

                    default: // D3D_FEATURE_LEVEL_10_0 & D3D_FEATURE_LEVEL_10_1
                        maxsize = (resDim == D3D11_RESOURCE_DIMENSION_TEXTURE3D)
                            ? 2048u /*D3D10_REQ_TEXTURE3D_U_V_OR_W_DIMENSION*/
                            : 8192u /*D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION*/;
                        break;
                    }

                    hr = FillInitData(width, height, depth, mipCount, arraySize, format, maxsize, bitSize, bitData,
                        twidth, theight, tdepth, skipMip, initData.get());
                    if (SUCCEEDED(hr))
                    {
                        hr = CreateD3DResources(d3dDevice,
                            resDim, twidth, theight, tdepth, mipCount - skipMip, arraySize,
                            format,
                            usage, bindFlags, cpuAccessFlags, miscFlags,
                            forceSRGB,
                            isCubeMap,
                            initData.get(),
                            texture, textureView);
                    }
                }
            }
        }

        return hr;
    }


    //--------------------------------------------------------------------------------------
    DDS_ALPHA_MODE GetAlphaMode(_In_ const DDS_HEADER* header) noexcept
    {
        if (header->ddspf.flags & DDS_FOURCC)
        {
            if (MAKEFOURCC('D', 'X', '1', '0') == header->ddspf.fourCC)
            {
                auto d3d10ext = reinterpret_cast<const DDS_HEADER_DXT10*>(reinterpret_cast<const uint8_t*>(header) + sizeof(DDS_HEADER));
                auto mode = static_cast<DDS_ALPHA_MODE>(d3d10ext->miscFlags2 & DDS_MISC_FLAGS2_ALPHA_MODE_MASK);
                switch (mode)
                {
                case DDS_ALPHA_MODE_STRAIGHT:
                case DDS_ALPHA_MODE_PREMULTIPLIED:
                case DDS_ALPHA_MODE_OPAQUE:
                case DDS_ALPHA_MODE_CUSTOM:
                    return mode;

                case DDS_ALPHA_MODE_UNKNOWN:
                default:
                    break;
                }
            }
            else if ((MAKEFOURCC('D', 'X', 'T', '2') == header->ddspf.fourCC)
                || (MAKEFOURCC('D', 'X', 'T', '4') == header->ddspf.fourCC))
            {
                return DDS_ALPHA_MODE_PREMULTIPLIED;
            }
        }

        return DDS_ALPHA_MODE_UNKNOWN;
    }

    //--------------------------------------------------------------------------------------
    void SetDebugTextureInfo(
        _In_z_ const wchar_t* fileName,
        _In_opt_ ID3D11Resource** texture,
        _In_opt_ ID3D11ShaderResourceView** textureView) noexcept
    {
#if !defined(NO_D3D11_DEBUG_NAME) && ( defined(_DEBUG) || defined(PROFILE) )
        if (texture || textureView)
        {
            CHAR strFileA[MAX_PATH];
            int result = WideCharToMultiByte(CP_UTF8,
                WC_NO_BEST_FIT_CHARS,
                fileName,
                -1,
                strFileA,
                MAX_PATH,
                nullptr,
                nullptr
            );
            if (result > 0)
            {
                const char* pstrName = strrchr(strFileA, '\\');
                if (!pstrName)
                {
                    pstrName = strFileA;
                }
                else
                {
                    pstrName++;
                }

                if (texture && *texture)
                {
                    (*texture)->SetPrivateData(WKPDID_D3DDebugObjectName,
                        static_cast<UINT>(strnlen_s(pstrName, MAX_PATH)),
                        pstrName
                    );
                }

                if (textureView && *textureView)
                {
                    (*textureView)->SetPrivateData(WKPDID_D3DDebugObjectName,
                        static_cast<UINT>(strnlen_s(pstrName, MAX_PATH)),
                        pstrName
                    );
                }
            }
        }
#else
        UNREFERENCED_PARAMETER(fileName);
        UNREFERENCED_PARAMETER(texture);
        UNREFERENCED_PARAMETER(textureView);
#endif
    }
} // anonymous namespace

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemory(
    ID3D11Device* d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    size_t maxsize,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    return CreateDDSTextureFromMemoryEx(d3dDevice, nullptr,
        ddsData, ddsDataSize,
        maxsize,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        false,
        texture, textureView, alphaMode);
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemory(
    ID3D11Device* d3dDevice,
    ID3D11DeviceContext* d3dContext,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    size_t maxsize,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    return CreateDDSTextureFromMemoryEx(d3dDevice, d3dContext,
        ddsData, ddsDataSize,
        maxsize,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        false,
        texture, textureView, alphaMode);
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemoryEx(
    ID3D11Device* d3dDevice,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    size_t maxsize,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    bool forceSRGB,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    return CreateDDSTextureFromMemoryEx(d3dDevice, nullptr,
        ddsData, ddsDataSize,
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        forceSRGB,
        texture, textureView, alphaMode);
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromMemoryEx(
    ID3D11Device* d3dDevice,
    ID3D11DeviceContext* d3dContext,
    const uint8_t* ddsData,
    size_t ddsDataSize,
    size_t maxsize,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    bool forceSRGB,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }
    if (alphaMode)
    {
        *alphaMode = DDS_ALPHA_MODE_UNKNOWN;
    }

    if (!d3dDevice || !ddsData || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    // Validate DDS file in memory
    const DDS_HEADER* header = nullptr;
    const uint8_t* bitData = nullptr;
    size_t bitSize = 0;

    HRESULT hr = LoadTextureDataFromMemory(ddsData, ddsDataSize,
        &header,
        &bitData,
        &bitSize
    );
    if (FAILED(hr))
    {
        return hr;
    }

    hr = CreateTextureFromDDS(d3dDevice, d3dContext,
        header, bitData, bitSize,
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        forceSRGB,
        texture, textureView);
    if (SUCCEEDED(hr))
    {
        if (texture && *texture)
        {
            SetDebugObjectName(*texture, "DDSTextureLoader");
        }

        if (textureView && *textureView)
        {
            SetDebugObjectName(*textureView, "DDSTextureLoader");
        }

        if (alphaMode)
            *alphaMode = GetAlphaMode(header);
    }

    return hr;
}

//--------------------------------------------------------------------------------------
_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFile(
    ID3D11Device* d3dDevice,
    const wchar_t* fileName,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    size_t maxsize,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    return CreateDDSTextureFromFileEx(d3dDevice, nullptr,
        fileName, maxsize,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        false,
        texture, textureView, alphaMode);
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFile(
    ID3D11Device* d3dDevice,
    ID3D11DeviceContext* d3dContext,
    const wchar_t* fileName,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    size_t maxsize,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    return CreateDDSTextureFromFileEx(d3dDevice, d3dContext,
        fileName,
        maxsize,
        D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0,
        false,
        texture, textureView, alphaMode);
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFileEx(
    ID3D11Device* d3dDevice,
    const wchar_t* fileName,
    size_t maxsize,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    bool forceSRGB,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    return CreateDDSTextureFromFileEx(d3dDevice, nullptr,
        fileName,
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        forceSRGB,
        texture, textureView, alphaMode);
}

_Use_decl_annotations_
HRESULT DirectX::CreateDDSTextureFromFileEx(
    ID3D11Device* d3dDevice,
    ID3D11DeviceContext* d3dContext,
    const wchar_t* fileName,
    size_t maxsize,
    D3D11_USAGE usage,
    unsigned int bindFlags,
    unsigned int cpuAccessFlags,
    unsigned int miscFlags,
    bool forceSRGB,
    ID3D11Resource** texture,
    ID3D11ShaderResourceView** textureView,
    DDS_ALPHA_MODE* alphaMode) noexcept
{
    if (texture)
    {
        *texture = nullptr;
    }
    if (textureView)
    {
        *textureView = nullptr;
    }
    if (alphaMode)
    {
        *alphaMode = DDS_ALPHA_MODE_UNKNOWN;
    }

    if (!d3dDevice || !fileName || (!texture && !textureView))
    {
        return E_INVALIDARG;
    }

    if (textureView && !(bindFlags & D3D11_BIND_SHADER_RESOURCE))
    {
        return E_INVALIDARG;
    }

    const DDS_HEADER* header = nullptr;
    const uint8_t* bitData = nullptr;
    size_t bitSize = 0;

    std::unique_ptr<uint8_t[]> ddsData;
    HRESULT hr = LoadTextureDataFromFile(fileName,
        ddsData,
        &header,
        &bitData,
        &bitSize
    );
    if (FAILED(hr))
    {
        return hr;
    }

    hr = CreateTextureFromDDS(d3dDevice, d3dContext,
        header, bitData, bitSize,
        maxsize,
        usage, bindFlags, cpuAccessFlags, miscFlags,
        forceSRGB,
        texture, textureView);

    if (SUCCEEDED(hr))
    {
        SetDebugTextureInfo(fileName, texture, textureView);

        if (alphaMode)
            *alphaMode = GetAlphaMode(header);
    }

    return hr;
}
