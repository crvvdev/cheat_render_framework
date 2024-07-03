#pragma once

#include <memory>
#include <vector>
#include <cmath>
#include <unordered_map>
#include <array>
#include <map>
#include <algorithm>
#include <locale>
#include <codecvt>

#include <d3d9.h>
#include <DirectXMath.h>

namespace CheatRenderFramework
{
namespace detail
{
__forceinline void ThrowIfFailed(const HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception("HRESULT not successfull!");
    }
}

template <class T> __forceinline void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}

__forceinline std::wstring ConvertToWString(const std::string &str)
{
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), NULL, 0);
    std::wstring wstrTo(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.length()), &wstrTo[0], sizeNeeded);
    return wstrTo;
}
} // namespace detail

namespace util
{
__forceinline bool IsTopologyList(const D3DPRIMITIVETYPE topology)
{
    return topology == D3DPT_POINTLIST || topology == D3DPT_LINELIST || topology == D3DPT_TRIANGLELIST;
}

__forceinline int GetTopologyOrder(const D3DPRIMITIVETYPE topology)
{
    switch (topology)
    {
    case D3DPT_POINTLIST:
        return 1;
    case D3DPT_LINELIST:
    case D3DPT_LINESTRIP:
        return 2;
    case D3DPT_TRIANGLELIST:
    case D3DPT_TRIANGLESTRIP:
    case D3DPT_TRIANGLEFAN:
        return 3;
    default:
        return 0;
    }
}
} // namespace util

class Renderer;
class RenderList;
class Font;

using RenderListPtr = std::shared_ptr<RenderList>;
using RendererPtr = std::shared_ptr<Renderer>;
using FontPtr = std::shared_ptr<Font>;
using FontHandle = size_t;

using TopologyType = D3DPRIMITIVETYPE;

using Vec2 = DirectX::XMFLOAT2;
using Vec3 = DirectX::XMFLOAT3;
using Vec4 = DirectX::XMFLOAT4;

static constexpr wchar_t g_charRangeMin = 0x20;
static constexpr wchar_t g_charRangeMax = 0x250;
static constexpr ULONG g_vertexDefinition = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

enum FontFlags : int32_t
{
    FONT_FLAG_NONE = 0,
    FONT_FLAG_BOLD = 1 << 0,
    FONT_FLAG_ITALIC = 1 << 1,
    FONT_FLAG_CLEAR_TYPE = 1 << 2,
    FONT_FLAG_MAX
};

enum TextFlags : int32_t
{
    TEXT_FLAG_NONE = 0,
    TEXT_FLAG_LEFT = 0 << 0,
    TEXT_FLAG_RIGHT = 1 << 1,
    TEXT_FLAG_CENTERED_X = 1 << 2,
    TEXT_FLAG_CENTERED_Y = 1 << 3,
    TEXT_FLAG_CENTERED = TEXT_FLAG_CENTERED_X | TEXT_FLAG_CENTERED_Y,
    TEXT_FLAG_DROPSHADOW = 1 << 4,
    TEXT_FLAG_OUTLINE = 1 << 5,
    TEXT_FLAG_COLORTAGS = 1 << 6,
    TEXT_FLAG_MAX
};

enum class GradientDirection : int32_t
{
    Horizontal = 0,
    Vertical
};

class Color
{
  public:
    Color() = default;

    Color(const uint32_t color) : _color(color)
    {
    }

    Color(const float r, const float g, const float b, const float a = 1.f)
    {
        this->_color = ToHexColor(r, g, b, a);
    }

    Color(const int r, const int g, const int b, const int a = 255)
    {
        this->_color = ToHexColor(static_cast<float>(r) / 255.f, static_cast<float>(g) / 255.f,
                                  static_cast<float>(b) / 255.f, static_cast<float>(a) / 255.f);
    }

    explicit operator uint32_t() const
    {
        return _color;
    }

    inline uint32_t ToHexColor() const
    {
        return _color;
    }

    inline void FromHexColor(uint32_t color)
    {
        this->_color = color;
    }

  private:
    inline uint32_t ToHexColor(float r, float g, float b, float a) const
    {
        uint8_t aByte = static_cast<uint8_t>(a * 255.0f);
        uint8_t rByte = static_cast<uint8_t>(r * 255.0f);
        uint8_t gByte = static_cast<uint8_t>(g * 255.0f);
        uint8_t bByte = static_cast<uint8_t>(b * 255.0f);

        return (aByte << 24) | (rByte << 16) | (gByte << 8) | bByte;
    }

    uint32_t _color = 0xFF000000; // Default to opaque black
};

struct Vertex
{
    Vertex() = default;

    Vertex(Vec4 position, Color color) : position(position), color(color)
    {
    }

    Vertex(Vec4 position, Color color, Vec2 tex) : position(position), color(color), tex(tex)
    {
    }

    Vertex(Vec3 position, Color color) : position(position.x, position.y, position.z, 1.f), color(color)
    {
    }

    Vertex(Vec2 position, Color color) : position(position.x, position.y, 1.f, 1.f), color(color)
    {
    }

    Vertex(float x, float y, float z, Color color) : position(x, y, z, 1.f), color(color)
    {
    }

    Vertex(float x, float y, Color color) : position(x, y, 1.f, 1.f), color(color)
    {
    }

    Vec4 position{};
    Color color{};
    Vec2 tex{};
};

struct Batch
{
    Batch(size_t count, const TopologyType topology, IDirect3DTexture9 *d3dTexture = nullptr)
        : count(count), topology(topology), d3dTexture(d3dTexture)
    {
    }

    std::size_t count = 0;
    TopologyType topology = static_cast<TopologyType>(0);
    IDirect3DTexture9 *d3dTexture = nullptr;
};

class RenderList : public std::enable_shared_from_this<RenderList>
{
  public:
    RenderList() = delete;
    RenderList(size_t maxVertices)
    {
        this->_vertices.reserve(maxVertices);
    }

    template <size_t N>
    inline void AddVertices(const Vertex (&vertexArray)[N], const TopologyType topology,
                            IDirect3DTexture9 *d3dTexture = nullptr)
    {
        const size_t numVertices = this->_vertices.size();

        if (this->_batches.empty() || this->_batches.back().topology != topology ||
            this->_batches.back().d3dTexture != d3dTexture)
        {
            this->_batches.emplace_back(0, topology, d3dTexture);
        }

        this->_batches.back().count += N;
        this->_vertices.resize(numVertices + N);

        memcpy(&this->_vertices[this->_vertices.size() - N], &vertexArray[0], N * sizeof(Vertex));

        switch (topology)
        {
        default:
            break;

        case D3DPT_LINESTRIP:
        case D3DPT_TRIANGLESTRIP:
            // add a new empty batch to force the end of the strip
            this->_batches.emplace_back(0, D3DPT_FORCE_DWORD, nullptr);
            break;
        }
    }

    inline void AddVertices(Vertex *vertexArray, size_t vertexArrayCount, const TopologyType topology,
                            IDirect3DTexture9 *d3dTexture = nullptr)
    {
        const size_t numVertices = this->_vertices.size();

        if (this->_batches.empty() || this->_batches.back().topology != topology ||
            this->_batches.back().d3dTexture != d3dTexture)
        {
            this->_batches.emplace_back(0, topology, d3dTexture);
        }

        this->_batches.back().count += vertexArrayCount;
        this->_vertices.resize(numVertices + vertexArrayCount);

        memcpy(&this->_vertices[this->_vertices.size() - vertexArrayCount], vertexArray,
               vertexArrayCount * sizeof(Vertex));

        switch (topology)
        {
        default:
            break;

        case D3DPT_LINESTRIP:
        case D3DPT_TRIANGLESTRIP:
            // add a new empty batch to force the end of the strip
            this->_batches.emplace_back(0, D3DPT_FORCE_DWORD, nullptr);
            break;
        }
    }

    void Clear()
    {
        this->_vertices.clear();
        this->_batches.clear();
    }

  protected:
    friend class Renderer;

    std::vector<Vertex> _vertices{};
    std::vector<Batch> _batches{};
};

class Font : public std::enable_shared_from_this<Font>
{
  public:
    using TextSegment = std::pair<std::wstring, Color>;

    Font(IDirect3DDevice9 *d3dDevice, const std::wstring &fontFamily, long fontHeigth,
         uint32_t fontFlags = FONT_FLAG_NONE)
        : _d3dDevice(d3dDevice), _fontFamily(fontFamily), _fontHeigth(fontHeigth), _fontFlags(fontFlags),
          _charSpacing(0), _fontTexture(nullptr), _textScale(1.f), _textureWidth(1024), _textureHeight(1024),
          _initialized(false)
    {
        this->Initialize();
    }

    ~Font()
    {
        this->Release();
    }

    inline void Release()
    {
        detail::SafeRelease(&this->_fontTexture);
    }

    inline void OnLostDevice()
    {
        this->Release();
    }

    inline void OnResetDevice()
    {
        this->Initialize();
    }

    inline void Initialize()
    {
        this->_initialized = false;

        HGDIOBJ gdiFont = nullptr;
        HGDIOBJ prevGdiFont = nullptr;
        HBITMAP bitmap = nullptr;
        HGDIOBJ prevBitmap = nullptr;

        HDC hdc = CreateCompatibleDC(nullptr);
        SetMapMode(hdc, MM_TEXT);

        this->CreateGdiFont(hdc, &gdiFont);
        if (!gdiFont)
        {
            throw std::runtime_error("Font::ctor(): CreateGdiFont failed!");
        }

        prevGdiFont = SelectObject(hdc, gdiFont);

        this->EstimateTextureSize(hdc);

        HRESULT hr = this->_d3dDevice->CreateTexture(this->_textureWidth, this->_textureHeight, 1, D3DUSAGE_DYNAMIC,
                                                     D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &this->_fontTexture, nullptr);
        if (FAILED(hr))
        {
            throw std::runtime_error("Font::ctor(): CreateTexture failed!");
        }

        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmapInfo.bmiHeader.biWidth = this->_textureWidth;
        bitmapInfo.bmiHeader.biHeight = -this->_textureHeight;
        bitmapInfo.bmiHeader.biPlanes = 1;
        bitmapInfo.bmiHeader.biCompression = BI_RGB;
        bitmapInfo.bmiHeader.biBitCount = 32;

        uint32_t *bitmapBips;

        bitmap = CreateDIBSection(hdc, &bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void **>(&bitmapBips), nullptr, 0);
        if (!bitmap)
        {
            throw std::runtime_error("Font::ctor(): CreateDIBSection failed!");
        }

        prevBitmap = SelectObject(hdc, bitmap);

        SetTextColor(hdc, RGB(255, 255, 255));
        SetBkColor(hdc, 0x00000000);
        SetTextAlign(hdc, TA_TOP);

        hr = this->RenderAlphabet(hdc, false);
        if (FAILED(hr))
        {
            throw std::runtime_error("Font::ctor(): RenderAlphabet failed!");
        }

        D3DLOCKED_RECT lockedRect;
        this->_fontTexture->LockRect(0, &lockedRect, nullptr, 0);

        uint8_t *dstRow = static_cast<uint8_t *>(lockedRect.pBits);

        for (long y = 0; y < this->_textureHeight; y++)
        {
            uint32_t *dst = reinterpret_cast<uint32_t *>(dstRow);

            for (long x = 0; x < this->_textureWidth; x++)
            {
                uint8_t alpha = (bitmapBips[this->_textureWidth * y + x] & 0xff);

                if (alpha > 0)
                {
                    *dst++ = (alpha << 24) | 0x00FFFFFF;
                }
                else
                {
                    *dst++ = 0x00000000;
                }
            }

            dstRow += lockedRect.Pitch;
        }

        this->_fontTexture->UnlockRect(0);

        SelectObject(hdc, prevBitmap);
        SelectObject(hdc, prevGdiFont);
        DeleteObject(bitmap);
        DeleteObject(gdiFont);
        DeleteDC(hdc);

        this->_initialized = true;
    }

    inline void RenderText(const RenderListPtr &renderList, Vec2 pos, const std::wstring &text, const Color color,
                           uint32_t flags, const Color outlineColor, float outlineThickness)
    {
        size_t numToSkip = 0;

        std::vector<TextSegment> segments = PreprocessText(text, color);
        std::wstring cleanedText;

        for (const auto &[textSegment, _] : segments)
        {
            cleanedText += textSegment;
        }

        if (flags & (TEXT_FLAG_RIGHT | TEXT_FLAG_CENTERED))
        {
            Vec2 size = this->CalculateTextExtent(cleanedText);

            if (flags & TEXT_FLAG_RIGHT)
            {
                pos.x -= size.x;
            }
            else if (flags & TEXT_FLAG_CENTERED_X)
            {
                pos.x -= 0.5f * size.x;
            }

            if (flags & TEXT_FLAG_CENTERED_Y)
            {
                pos.y -= 0.5f * size.y;
            }
        }

        pos.x -= this->_charSpacing;

        float startX = pos.x;

        for (const auto &[textSegment, currentColor] : segments)
        {
            for (const auto &c : textSegment)
            {
                if (numToSkip > 0 && numToSkip-- > 0)
                {
                    continue;
                }

                // jump to next line if asked
                if (c == '\n')
                {
                    pos.x = startX;
                    pos.y += (this->_charCoords[L' '][3] - this->_charCoords[L' '][1]) * this->_textureHeight;
                }

                // ignore invalid chars
                if (c < L' ')
                {
                    continue;
                }

                // try to find char in coordinates map
                auto it = this->_charCoords.find(c);
                if (it == this->_charCoords.end())
                {
                    continue;
                }

                float tx1 = it->second[0];
                float ty1 = it->second[1];
                float tx2 = it->second[2];
                float ty2 = it->second[3];

                float w = (tx2 - tx1) * this->_textureWidth / this->_textScale;
                float h = (ty2 - ty1) * this->_textureHeight / this->_textScale;

                // do not render space char
                if (c != L' ')
                {
                    Vertex v[] = {{Vec4{pos.x - 0.5f, pos.y - 0.5f + h, 0.9f, 1.f}, currentColor, Vec2{tx1, ty2}},
                                  {Vec4{pos.x - 0.5f, pos.y - 0.5f, 0.9f, 1.f}, currentColor, Vec2{tx1, ty1}},
                                  {Vec4{pos.x - 0.5f + w, pos.y - 0.5f + h, 0.9f, 1.f}, currentColor, Vec2{tx2, ty2}},

                                  {Vec4{pos.x - 0.5f + w, pos.y - 0.5f, 0.9f, 1.f}, currentColor, Vec2{tx2, ty1}},
                                  {Vec4{pos.x - 0.5f + w, pos.y - 0.5f + h, 0.9f, 1.f}, currentColor, Vec2{tx2, ty2}},
                                  {Vec4{pos.x - 0.5f, pos.y - 0.5f, 0.9f, 1.f}, currentColor, Vec2{tx1, ty1}}};

                    // Outline vertices
                    Vertex outlineV[] = {{Vec4{pos.x - outlineThickness, pos.y - outlineThickness + h, 0.89f, 1.f},
                                          outlineColor, Vec2{tx1, ty2}},
                                         {Vec4{pos.x - outlineThickness, pos.y - outlineThickness, 0.89f, 1.f},
                                          outlineColor, Vec2{tx1, ty1}},
                                         {Vec4{pos.x - outlineThickness + w, pos.y - outlineThickness + h, 0.89f, 1.f},
                                          outlineColor, Vec2{tx2, ty2}},

                                         {Vec4{pos.x - outlineThickness + w, pos.y - outlineThickness, 0.89f, 1.f},
                                          outlineColor, Vec2{tx2, ty1}},
                                         {Vec4{pos.x - outlineThickness + w, pos.y - outlineThickness + h, 0.89f, 1.f},
                                          outlineColor, Vec2{tx2, ty2}},
                                         {Vec4{pos.x - outlineThickness, pos.y - outlineThickness, 0.89f, 1.f},
                                          outlineColor, Vec2{tx1, ty1}}};

                    // Drop shadow vertices (slightly offset and darker)
                    Color shadowColor = Color(0x00, 0x00, 0x00, (currentColor.ToHexColor() >> 24) & 0xff);

                    Vertex shadowV[] = {
                        {Vec4{pos.x + 1.0f, pos.y + 1.0f + h, 0.89f, 1.f}, shadowColor, Vec2{tx1, ty2}},
                        {Vec4{pos.x + 1.0f, pos.y + 1.0f, 0.89f, 1.f}, shadowColor, Vec2{tx1, ty1}},
                        {Vec4{pos.x + 1.0f + w, pos.y + 1.0f + h, 0.89f, 1.f}, shadowColor, Vec2{tx2, ty2}},

                        {Vec4{pos.x + 1.0f + w, pos.y + 1.0f, 0.89f, 1.f}, shadowColor, Vec2{tx2, ty1}},
                        {Vec4{pos.x + 1.0f + w, pos.y + 1.0f + h, 0.89f, 1.f}, shadowColor, Vec2{tx2, ty2}},
                        {Vec4{pos.x + 1.0f, pos.y + 1.0f, 0.89f, 1.f}, shadowColor, Vec2{tx1, ty1}}};

                    if (flags & TEXT_FLAG_OUTLINE)
                    {
                        renderList->AddVertices(outlineV, D3DPT_TRIANGLELIST, this->_fontTexture);
                    }
                    else if (flags & TEXT_FLAG_DROPSHADOW)
                    {
                        renderList->AddVertices(shadowV, D3DPT_TRIANGLELIST, this->_fontTexture);
                    }

                    renderList->AddVertices(v, D3DPT_TRIANGLELIST, this->_fontTexture);
                }

                pos.x += w - (2.f * this->_charSpacing);
            }
        }
    }

    inline Vec2 CalculateTextExtent(const std::wstring &text)
    {
        float rowWidth = 0.f;
        float rowHeight = (this->_charCoords[L' '][3] - this->_charCoords[L' '][1]) * this->_textureHeight;
        float width = 0.f;
        float height = rowHeight;

        for (const auto &c : text)
        {
            if (c == L'\n')
            {
                height += rowHeight;
                width = std::max(width, rowWidth);
                rowWidth = 0.f;
            }
            else if (c >= L' ')
            {
                auto it = this->_charCoords.find(c);
                if (it != this->_charCoords.end())
                {
                    float charWidth = (it->second[2] - it->second[0]) * this->_textureWidth / this->_textScale;
                    rowWidth += charWidth - (2.f * this->_charSpacing);
                }
            }
        }

        width = std::max(width, rowWidth);
        return Vec2(width, height);
    }

    inline bool IsInitialized() const
    {
        return this->_initialized;
    }

  private:
    inline void EstimateTextureSize(HDC hdc)
    {
        SIZE size;
        wchar_t chr[] = L" ";

        if (!GetTextExtentPoint32W(hdc, chr, static_cast<int>(wcslen(chr)), &size))
        {
            throw std::runtime_error("EstimateTextureSize(): Failed to get text extent!");
        }

        long x = this->_charSpacing;
        long y = 0;

        for (wchar_t c = g_charRangeMin; c < g_charRangeMax; c++)
        {
            chr[0] = c;

            if (!GetTextExtentPoint32W(hdc, chr, static_cast<int>(wcslen(chr)), &size))
            {
                continue;
            }

            if (x + size.cx + this->_charSpacing > this->_textureWidth)
            {
                x = this->_charSpacing;
                y += size.cy + 1;
            }

            if (y + size.cy > this->_textureHeight)
            {
                this->_textureWidth = this->_textureWidth * 2;
                this->_textureHeight = this->_textureHeight * 2;
                x = this->_charSpacing;
                y = 0;
            }

            x += size.cx + (2 * this->_charSpacing);
        }
    }

    inline void CreateGdiFont(HDC hdc, HGDIOBJ *gdiFont)
    {
        static const int pointsPerInch = 72;
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        int pixelsHeight = -MulDiv(this->_fontHeigth, dpi, pointsPerInch);

        DWORD bold = (this->_fontFlags & FONT_FLAG_BOLD) ? FW_BOLD : FW_NORMAL;
        DWORD italic = (this->_fontFlags & FONT_FLAG_ITALIC) ? TRUE : FALSE;

        HFONT font = CreateFontW(pixelsHeight, 0, 0, 0, bold, italic, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS,
                                 CLIP_DEFAULT_PRECIS,
                                 (this->_fontFlags & FONT_FLAG_CLEAR_TYPE) ? CLEARTYPE_QUALITY : ANTIALIASED_QUALITY,
                                 VARIABLE_PITCH, this->_fontFamily.c_str());

        if (font == NULL)
        {
            throw std::runtime_error("CreateGdiFont(): CreateFontW failed!");
        }

        *gdiFont = font;
    }

    inline HRESULT RenderAlphabet(HDC hdc, bool onlyMeasure)
    {
        SIZE size;
        wchar_t chr[2] = L" ";

        if (!GetTextExtentPoint32W(hdc, chr, static_cast<int>(wcslen(chr)), &size))
        {
            return E_FAIL;
        }

        // the result of the font width is used for spacing
        this->_charSpacing = static_cast<long>(ceil(size.cy * 0.3f));

        long x = this->_charSpacing;
        long y = 0;

        // cover basic latin till latin extended B
        for (wchar_t c = g_charRangeMin; c < g_charRangeMax; c++)
        {
            chr[0] = c;

            if (!GetTextExtentPoint32W(hdc, chr, static_cast<int>(wcslen(chr)), &size))
            {
                continue;
            }

            if (x + size.cx + this->_charSpacing > this->_textureWidth)
            {
                x = this->_charSpacing;
                y += size.cy + 1;
            }

            if (y + size.cy > this->_textureHeight)
            {
                return E_NOT_SUFFICIENT_BUFFER;
            }

            if (!onlyMeasure)
            {
                if (!ExtTextOutW(hdc, x, y, ETO_OPAQUE, nullptr, chr, static_cast<int>(wcslen(chr)), nullptr))
                {
                    return E_FAIL;
                }

                this->_charCoords[c] = {(static_cast<float>(x - this->_charSpacing)) / this->_textureWidth,
                                        (static_cast<float>(y)) / this->_textureHeight,
                                        (static_cast<float>(x + size.cx + this->_charSpacing)) / this->_textureWidth,
                                        (static_cast<float>(y + size.cy)) / this->_textureHeight};
            }

            x += size.cx + (2 * this->_charSpacing);
        }

        return S_OK;
    }

    inline std::vector<TextSegment> PreprocessText(const std::wstring &text, Color defaultColor)
    {
        std::vector<TextSegment> segments;
        std::wstring cleanText;
        Color currentColor = defaultColor;

        for (size_t i = 0; i < text.size(); ++i)
        {
            if (text[i] == L'{' && text.size() > i + 11 && text[i + 1] == L'#')
            {
                bool hasAlpha = (text[i + 10] == L'}');
                bool noAlpha = (text[i + 8] == L'}');
                if (hasAlpha || noAlpha)
                {
                    if (!cleanText.empty())
                    {
                        segments.push_back({cleanText, currentColor});
                        cleanText.clear();
                    }

                    std::wstring colorStr = text.substr(i + 2, hasAlpha ? 8 : 6);
                    colorStr.erase(
                        std::remove_if(colorStr.begin(), colorStr.end(), [](wchar_t c) { return !iswxdigit(c); }),
                        colorStr.end());

                    if (!hasAlpha)
                    {
                        colorStr = L"ff" + colorStr;
                    }

                    currentColor = std::wcstoul(colorStr.c_str(), nullptr, 16);
                    i += hasAlpha ? 10 : 8;
                    continue;
                }
            }
            cleanText += text[i];
        }

        if (!cleanText.empty())
        {
            segments.push_back({cleanText, currentColor});
        }

        return segments;
    }

    IDirect3DDevice9 *_d3dDevice;
    IDirect3DTexture9 *_fontTexture;
    std::map<wchar_t, std::array<float, 4>> _charCoords;
    long _textureWidth;
    long _textureHeight;
    float _textScale;
    long _charSpacing;

    std::wstring _fontFamily;
    long _fontHeigth;
    uint32_t _fontFlags;
    bool _initialized;
};

class Renderer : public std::enable_shared_from_this<Renderer>
{
  public:
    Renderer(IDirect3DDevice9 *d3dDevice, uint32_t maxVertices)
        : _d3dDevice(d3dDevice), _d3dVertexBuffer(nullptr), _maxVertices(maxVertices),
          _renderList(std::make_shared<RenderList>(maxVertices)), _d3dPreviousStateBlock(nullptr),
          _d3dRenderStateBlock(nullptr), _nextFontId(1)
    {
        if (!d3dDevice)
        {
            throw std::exception("Renderer::ctor() d3dDevice is null!");
        }

        this->AcquireStateBlock();
    }

    ~Renderer()
    {
        this->Release();
    }

    inline void Release()
    {
        detail::SafeRelease(&this->_d3dVertexBuffer);
        detail::SafeRelease(&this->_d3dPreviousStateBlock);
        detail::SafeRelease(&this->_d3dRenderStateBlock);
    }

    inline void OnLostDevice()
    {
        this->Release();

        for (auto &[_, font] : this->_fonts)
        {
            font->OnLostDevice();
        }
    }

    inline void OnResetDevice()
    {
        this->AcquireStateBlock();

        for (auto &[_, font] : this->_fonts)
        {
            font->OnResetDevice();
        }
    }

    inline void BeginFrame()
    {
        this->_d3dPreviousStateBlock->Capture();
        this->_d3dRenderStateBlock->Apply();
    }

    inline void EndFrame()
    {
        this->_d3dPreviousStateBlock->Apply();
    }

    inline FontHandle AddFont(const std::wstring &fontFamily, long fontHeigth, uint32_t fontFlags = FONT_FLAG_NONE)
    {
        std::shared_ptr<Font> fontPtr = std::make_shared<Font>(this->_d3dDevice, fontFamily, fontHeigth, fontFlags);

        const size_t fontHandle = this->_nextFontId++;
        this->_fonts[fontHandle] = fontPtr;
        return fontHandle;
    }

    inline void AddText(const RenderListPtr &renderList, const FontHandle fontId, const std::wstring &text, Vec2 pos,
                        const Color color, uint32_t flags = FONT_FLAG_NONE, const Color outlineColor = Color(0, 0, 0),
                        float outlineThickness = 2.0f)
    {
        auto font = this->_fonts.find(fontId);
        if (font == this->_fonts.end())
        {
            throw std::exception("AddText(): Font not found!");
        }

        return font->second->RenderText(renderList, pos, text, color, flags, outlineColor, outlineThickness);
    }

    inline void AddText(const FontHandle fontId, const std::wstring &text, Vec2 pos, const Color color,
                        uint32_t flags = FONT_FLAG_NONE, const Color outlineColor = Color(0, 0, 0),
                        float outlineThickness = 2.0f)
    {
        return this->AddText(this->_renderList, fontId, text, pos, color, flags, outlineColor, outlineThickness);
    }

    inline void AddText(const RenderListPtr &renderList, const FontHandle fontId, const std::string &text, Vec2 pos,
                        const Color color, uint32_t flags = FONT_FLAG_NONE, const Color outlineColor = Color(0, 0, 0),
                        float outlineThickness = 2.0f)
    {
        auto font = this->_fonts.find(fontId);
        if (font == this->_fonts.end())
        {
            throw std::exception("AddText(): Font not found!");
        }

        return font->second->RenderText(renderList, pos, detail::ConvertToWString(text), color, flags, outlineColor,
                                        outlineThickness);
    }

    inline void AddText(const FontHandle fontId, const std::string &text, Vec2 pos, const Color color,
                        uint32_t flags = FONT_FLAG_NONE, const Color outlineColor = Color(0, 0, 0),
                        float outlineThickness = 2.0f)
    {
        return this->AddText(this->_renderList, fontId, text, pos, color, flags, outlineColor, outlineThickness);
    }

    inline Vec2 CalculateTextExtent(const FontHandle fontId, const std::wstring &text)
    {
        auto font = this->_fonts.find(fontId);
        if (font == this->_fonts.end())
        {
            throw std::exception("AddText(): Font not found!");
        }

        return font->second->CalculateTextExtent(text);
    }

    inline Vec2 CalculateTextExtent(const FontHandle fontId, const std::string &text)
    {
        return this->CalculateTextExtent(fontId, detail::ConvertToWString(text));
    }

    inline void AddGradientRect(const RenderListPtr &renderList, const Vec2 &min, const Vec2 &max, const Color &color1,
                                const Color &color2, const GradientDirection direction)
    {
        float x1 = min.x;
        float y1 = min.y;
        float x2 = max.x;
        float y2 = max.y;

        Vertex v[6];

        if (direction == GradientDirection::Horizontal)
        {
            v[0] = {x1, y1, 0.5f, color1}; // Top-left
            v[1] = {x2, y1, 0.5f, color1}; // Top-right
            v[2] = {x1, y2, 0.5f, color2}; // Bottom-left

            v[3] = {x2, y1, 0.5f, color1}; // Top-right
            v[4] = {x2, y2, 0.5f, color2}; // Bottom-right
            v[5] = {x1, y2, 0.5f, color2}; // Bottom-left
        }
        else
        {
            v[0] = {x1, y1, 0.5f, color1}; // Top-left
            v[1] = {x2, y1, 0.5f, color2}; // Top-right
            v[2] = {x1, y2, 0.5f, color1}; // Bottom-left

            v[3] = {x2, y1, 0.5f, color2}; // Top-right
            v[4] = {x2, y2, 0.5f, color2}; // Bottom-right
            v[5] = {x1, y2, 0.5f, color1}; // Bottom-left
        }

        renderList->AddVertices(v, 6, D3DPT_TRIANGLELIST);
    }

    inline void AddGradientRect(const Vec2 &min, const Vec2 &max, const Color &color1, const Color &color2,
                                const GradientDirection direction)
    {
        return this->AddGradientRect(this->_renderList, min, max, color1, color2, direction);
    }

    inline void AddGradientRect(const RenderListPtr &renderList, const Vec4 &rect, const Color &color1,
                                const Color &color2, const GradientDirection direction)
    {
        return this->AddGradientRect(renderList, Vec2(rect.x, rect.y), Vec2(rect.x + rect.z, rect.y + rect.w), color1,
                                     color2, direction);
    }

    inline void AddGradientRect(const Vec4 &rect, const Color &color1, const Color &color2,
                                const GradientDirection direction)
    {
        return this->AddGradientRect(this->_renderList, Vec2(rect.x, rect.y), Vec2(rect.x + rect.z, rect.y + rect.w),
                                     color1, color2, direction);
    }

    inline void AddRectFilled(const RenderListPtr &renderList, const Vec2 &min, const Vec2 &max, const Color color)
    {
        float x1 = min.x;
        float y1 = min.y;
        float x2 = max.x;
        float y2 = max.y;

        Vertex v[] = {
            {x1, y1, color}, // Top-left vertex
            {x2, y1, color}, // Top-right vertex
            {x1, y2, color}, // Bottom-left vertex

            {x2, y1, color}, // Top-right vertex
            {x2, y2, color}, // Bottom-right vertex
            {x1, y2, color}  // Bottom-left vertex
        };

        renderList->AddVertices(v, D3DPT_TRIANGLELIST);
    }

    inline void AddRectFilled(const Vec2 &min, const Vec2 &max, const Color color)
    {
        return this->AddRectFilled(this->_renderList, min, max, color);
    }

    inline void AddRectFilled(const RenderListPtr &renderList, const Vec4 &rect, const Color color)
    {
        return this->AddRectFilled(renderList, Vec2(rect.x, rect.y), Vec2(rect.x + rect.z, rect.y + rect.w), color);
    }

    inline void AddRectFilled(const Vec4 &rect, const Color color)
    {
        return this->AddRectFilled(this->_renderList, Vec2(rect.x, rect.y), Vec2(rect.x + rect.z, rect.y + rect.w),
                                   color);
    }

    inline void AddRect(const RenderListPtr &renderList, const Vec2 &min, const Vec2 &max, const Color color,
                        float strokeWidth = 1.f)
    {
        Vec2 topLineMin = {min.x, min.y};
        Vec2 topLineMax = {max.x, min.y + strokeWidth};
        Vec2 bottomLineMin = {min.x, max.y - strokeWidth};
        Vec2 bottomLineMax = {max.x, max.y};
        Vec2 leftLineMin = {min.x, min.y};
        Vec2 leftLineMax = {min.x + strokeWidth, max.y};
        Vec2 rightLineMin = {max.x - strokeWidth, min.y};
        Vec2 rightLineMax = {max.x, max.y};

        this->AddRectFilled(renderList, topLineMin, topLineMax, color);
        this->AddRectFilled(renderList, bottomLineMin, bottomLineMax, color);
        this->AddRectFilled(renderList, leftLineMin, leftLineMax, color);
        this->AddRectFilled(renderList, rightLineMin, rightLineMax, color);
    }

    inline void AddRect(const Vec2 &min, const Vec2 &max, const Color color, float strokeWidth = 1.f)
    {
        return this->AddRect(this->_renderList, min, max, color, strokeWidth);
    }

    inline void AddRect(const RenderListPtr &renderList, const Vec4 &rect, const Color color, float strokeWidth = 1.f)
    {
        return this->AddRect(renderList, Vec2(rect.x, rect.y), Vec2(rect.x + rect.z, rect.y + rect.w), color,
                             strokeWidth);
    }

    inline void AddRect(const Vec4 &rect, const Color color, float strokeWidth = 1.f)
    {
        return this->AddRect(this->_renderList, Vec2(rect.x, rect.y), Vec2(rect.x + rect.z, rect.y + rect.w), color,
                             strokeWidth);
    }

    inline void AddLine(const RenderListPtr &renderList, const Vec2 &v1, const Vec2 &v2, const Color color,
                        const float thickness = 1.f)
    {
        float dx = v2.x - v1.x;
        float dy = v2.y - v1.y;
        float length = std::sqrtf(dx * dx + dy * dy);

        dx /= length;
        dy /= length;

        float px = -dy * thickness * 0.5f;
        float py = dx * thickness * 0.5f;

        Vertex v[] = {
            {{v1.x + px, v1.y + py, 0.0f, 1.0f}, color}, // Bottom-left
            {{v1.x - px, v1.y - py, 0.0f, 1.0f}, color}, // Top-left
            {{v2.x + px, v2.y + py, 0.0f, 1.0f}, color}, // Bottom-right
            {{v2.x - px, v2.y - py, 0.0f, 1.0f}, color}  // Top-right
        };

        renderList->AddVertices(v, D3DPT_TRIANGLESTRIP);
    }

    inline void AddLine(const Vec2 &v1, const Vec2 &v2, const Color color, const float thickness = 1.f)
    {
        return this->AddLine(this->_renderList, v1, v2, color, thickness);
    }

    inline void AddCircle(const RenderListPtr &renderList, const Vec2 &pos, float radius, const Color color,
                          int segments = 64)
    {
        std::vector<Vertex> v(segments + 1);

        for (int i = 0; i <= segments; i++)
        {
            const float theta = 2.f * DirectX::XM_PI * static_cast<float>(i) / static_cast<float>(segments);

            v[i] = Vertex{pos.x + radius * std::cos(theta), pos.y + radius * std::sin(theta), color};
        }

        renderList->AddVertices(v.data(), v.size(), D3DPT_LINESTRIP);
    }

    inline void AddCircle(const Vec2 &pos, float radius, const Color color, int segments = 24)
    {
        return this->AddCircle(this->_renderList, pos, radius, color, segments);
    }

    inline void Render(const RenderListPtr &renderList)
    {
        size_t numVertices = renderList->_vertices.size();
        if (numVertices > 0)
        {
            void *data;

            if (numVertices > this->_maxVertices)
            {
                this->_maxVertices = static_cast<uint32_t>(numVertices);
                this->Release();
                this->AcquireStateBlock();
            }

            detail::ThrowIfFailed(_d3dVertexBuffer->Lock(0, 0, &data, D3DLOCK_DISCARD));
            {
                memcpy(data, renderList->_vertices.data(), sizeof(Vertex) * numVertices);
            }
            _d3dVertexBuffer->Unlock();
        }

        size_t pos = 0;

        for (const auto &batch : renderList->_batches)
        {
            int order = util::GetTopologyOrder(batch.topology);
            if (batch.count && order > 0)
            {
                uint32_t primitiveCount = static_cast<uint32_t>(batch.count);

                if (util::IsTopologyList(batch.topology))
                {
                    primitiveCount /= order;
                }
                else
                {
                    primitiveCount -= (order - 1);
                }

                _d3dDevice->SetTexture(0, batch.d3dTexture);
                _d3dDevice->DrawPrimitive(batch.topology, static_cast<uint32_t>(pos), primitiveCount);

                pos += batch.count;
            }
        }
    }

    inline void Render()
    {
        this->Render(_renderList);
        _renderList->Clear();
    }

    inline RenderListPtr CreateRenderList()
    {
        return std::make_shared<RenderList>(this->_maxVertices);
    }

  private:
    inline void AcquireStateBlock()
    {
        D3DVIEWPORT9 vp = {};
        detail::ThrowIfFailed(this->_d3dDevice->GetViewport(&vp));

        this->_displaySize = {static_cast<float>(vp.Width), static_cast<float>(vp.Height)};

        detail::ThrowIfFailed(this->_d3dDevice->CreateVertexBuffer(
            this->_maxVertices * sizeof(Vertex), D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, g_vertexDefinition,
            D3DPOOL_DEFAULT, &this->_d3dVertexBuffer, nullptr));

        for (int i = 0; i < 2; ++i)
        {
            this->_d3dDevice->BeginStateBlock();

            this->_d3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);

            this->_d3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            this->_d3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            this->_d3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

            this->_d3dDevice->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
            this->_d3dDevice->SetRenderState(D3DRS_ALPHAREF, 0x08);
            this->_d3dDevice->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);

            this->_d3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

            this->_d3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
            this->_d3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
            this->_d3dDevice->SetRenderState(D3DRS_STENCILENABLE, FALSE);
            this->_d3dDevice->SetRenderState(D3DRS_CLIPPING, TRUE);
            this->_d3dDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, FALSE);
            this->_d3dDevice->SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
            this->_d3dDevice->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
            this->_d3dDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);
            this->_d3dDevice->SetRenderState(D3DRS_COLORWRITEENABLE,
                                             D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN |
                                                 D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);

            this->_d3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
            this->_d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
            this->_d3dDevice->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
            this->_d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
            this->_d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
            this->_d3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
            this->_d3dDevice->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
            this->_d3dDevice->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
            this->_d3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
            this->_d3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
            this->_d3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            this->_d3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            this->_d3dDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

            this->_d3dDevice->SetFVF(g_vertexDefinition);
            this->_d3dDevice->SetTexture(0, nullptr);
            this->_d3dDevice->SetStreamSource(0, this->_d3dVertexBuffer, 0, sizeof(Vertex));
            this->_d3dDevice->SetPixelShader(nullptr);

            if (i != 0)
            {
                this->_d3dDevice->EndStateBlock(&this->_d3dPreviousStateBlock);
            }
            else
            {
                this->_d3dDevice->EndStateBlock(&this->_d3dRenderStateBlock);
            }
        }
    }

    Vec2 _displaySize;
    IDirect3DDevice9 *_d3dDevice;
    IDirect3DVertexBuffer9 *_d3dVertexBuffer;

    uint32_t _maxVertices;
    RenderListPtr _renderList;

    IDirect3DStateBlock9 *_d3dPreviousStateBlock;
    IDirect3DStateBlock9 *_d3dRenderStateBlock;

    std::unordered_map<FontHandle, FontPtr> _fonts;
    FontHandle _nextFontId;
};
} // namespace CheatRenderFramework