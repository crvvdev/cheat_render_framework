#pragma once

#include <memory>
#include <vector>
#include <cmath>
#include <unordered_map>
#include <array>
#include <map>
#include <algorithm>

#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#pragma comment(lib, "d3dcompiler")

namespace CheatRenderFramework
{
namespace detail
{
void ThrowIfFailed(const HRESULT hr)
{
    if (FAILED(hr))
    {
        throw std::exception("HRESULT not successfull!");
    }
}

template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}
} // namespace detail

class Renderer;
class RenderList;
class Font;

using RenderListPtr = std::shared_ptr<RenderList>;
using RendererPtr = std::shared_ptr<Renderer>;
using FontPtr = std::shared_ptr<Font>;
using FontHandle = size_t;

using TopologyType = D3D11_PRIMITIVE_TOPOLOGY;

using Vec2 = DirectX::XMFLOAT2;
using Vec3 = DirectX::XMFLOAT3;
using Vec4 = DirectX::XMFLOAT4;

static constexpr const char g_vertexShader[] = "cbuffer vertexBuffer : register(b0) \
            {\
              float4x4 ProjectionMatrix; \
            };\
            struct VS_INPUT\
            {\
              float2 pos : POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            struct PS_INPUT\
            {\
              float4 pos : SV_POSITION;\
              float4 col : COLOR0;\
              float2 uv  : TEXCOORD0;\
            };\
            \
            PS_INPUT main(VS_INPUT input)\
            {\
              PS_INPUT output;\
              output.pos = mul( ProjectionMatrix, float4(input.pos.xy, 0.f, 1.f));\
              output.col = input.col;\
              output.uv  = input.uv;\
              return output;\
            }";

static constexpr const char g_pixelShader[] = "struct PS_INPUT\
            {\
            float4 pos : SV_POSITION;\
            float4 col : COLOR0;\
            float2 uv  : TEXCOORD0;\
            };\
            sampler sampler0;\
            Texture2D texture0;\
            \
            float4 main(PS_INPUT input) : SV_Target\
            {\
            float4 out_col = input.col * texture0.Sample(sampler0, input.uv); \
            return out_col; \
            }";

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

    Vertex(const Vec2 &pos, Color color, const Vec2 &uv) : pos(pos), color(color), uv(uv)
    {
    }

    Vertex(const Vec2 &pos, Color color) : pos(pos), color(color)
    {
    }

    Vertex(const float x, const float y, Color color) : pos(x, y), color(color)
    {
    }

    Vec2 pos{};
    Color color{};
    Vec2 uv{};
};

struct Batch
{
    Batch(size_t count, const TopologyType topology, ID3D11ShaderResourceView *texture = nullptr)
        : count(count), topology(topology), texture(texture)
    {
    }

    std::size_t count = 0;
    TopologyType topology = static_cast<TopologyType>(0);
    ID3D11ShaderResourceView *texture = nullptr;
};

class RenderList : public std::enable_shared_from_this<RenderList>
{
  public:
    RenderList() = delete;
    RenderList(size_t maxVertices)
    {
        this->_vertices.reserve(maxVertices);
    }

    inline void AddVertex(Vertex &vertex, D3D11_PRIMITIVE_TOPOLOGY topology)
    {
        assert(topology != D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP &&
               "addVertex >Use addVertices to draw line/triangle strips!");
        assert(topology != D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ &&
               "addVertex >Use addVertices to draw line/triangle strips!");
        assert(topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP &&
               "addVertex >Use addVertices to draw line/triangle strips!");
        assert(topology != D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ &&
               "addVertex >Use addVertices to draw line/triangle strips!");

        if (this->_batches.empty() || this->_batches.back().topology != topology)
        {
            this->_batches.emplace_back(0, topology);
        }

        this->_batches.back().count += 1;
        this->_vertices.push_back(vertex);
    }

    template <size_t N>
    inline void AddVertices(const Vertex (&vertexArray)[N], const TopologyType topology,
                            ID3D11ShaderResourceView *texture = nullptr)
    {
        const size_t numVertices = this->_vertices.size();

        if (this->_batches.empty() || this->_batches.back().topology != topology ||
            this->_batches.back().texture != texture)
        {
            this->_batches.emplace_back(0, topology, texture);
        }

        this->_batches.back().count += N;
        this->_vertices.resize(numVertices + N);

        memcpy(&this->_vertices[this->_vertices.size() - N], &vertexArray[0], N * sizeof(Vertex));

        /*switch (topology)
        {
        default:
            break;

        case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:
        case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
        case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
        case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
            Vertex seperator{};
            this->AddVertex(seperator, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
            break;
        }*/
    }

    inline void AddVertices(Vertex *vertexArray, size_t vertexArrayCount, const TopologyType topology,
                            ID3D11ShaderResourceView *texture = nullptr)
    {
        const size_t numVertices = this->_vertices.size();

        if (this->_batches.empty() || this->_batches.back().topology != topology ||
            this->_batches.back().texture != texture)
        {
            this->_batches.emplace_back(0, topology, texture);
        }

        this->_batches.back().count += vertexArrayCount;
        this->_vertices.resize(numVertices + vertexArrayCount);

        memcpy(&this->_vertices[this->_vertices.size() - vertexArrayCount], vertexArray,
               vertexArrayCount * sizeof(Vertex));

        /*switch (topology)
        {
        default:
            break;

        case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:
        case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
        case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
        case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
            Vertex seperator{};
            this->AddVertex(seperator, D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);
            break;
        }*/
    }

    RenderListPtr MakePtr()
    {
        return shared_from_this();
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

    Font(const RenderListPtr &renderList, ID3D11Device *d3dDevice, const std::wstring &fontFamily, long fontHeigth,
         uint32_t fontFlags = FONT_FLAG_NONE)
        : _renderList(renderList), _d3dDevice(d3dDevice), _fontFamily(fontFamily), _fontHeigth(fontHeigth),
          _fontFlags(fontFlags), _charSpacing(0), _d3dTexture(nullptr), _d3dTextureView(nullptr), _textScale(1.f),
          _textureWidth(1024), _textureHeight(1024), _initialized(false)
    {
        this->_d3dDevice->GetImmediateContext(&this->_d3dDeviceContext);

        this->Initialize();
    }

    ~Font()
    {
        this->Release();
    }

    inline void Release()
    {
        detail::SafeRelease(&this->_d3dTexture);
        detail::SafeRelease(&this->_d3dTextureView);
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

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = this->_textureWidth;
        texDesc.Height = this->_textureHeight;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_B4G4R4A4_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DYNAMIC;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = this->_d3dDevice->CreateTexture2D(&texDesc, nullptr, &this->_d3dTexture);
        if (FAILED(hr))
        {
            throw std::runtime_error("Font::ctor(): CreateTexture failed!");
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;

        hr = this->_d3dDevice->CreateShaderResourceView(this->_d3dTexture, &srvDesc, &this->_d3dTextureView);
        if (FAILED(hr))
        {
            throw std::runtime_error("Font::ctor(): CreateShaderResourceView failed!");
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

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        this->_d3dDeviceContext->Map(this->_d3dTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

        uint8_t *dstRow = static_cast<uint8_t *>(mappedResource.pData);

        for (long y = 0; y < this->_textureHeight; y++)
        {
            uint16_t *dst = reinterpret_cast<uint16_t *>(dstRow);

            for (long x = 0; x < this->_textureWidth; x++)
            {
                uint8_t alpha = ((bitmapBips[this->_textureWidth * y + x] & 0xff) >> 4);
                if (alpha > 0)
                {
                    *dst++ = ((alpha << 12) | 0x0fff);
                }
                else
                {
                    *dst++ = 0x0000;
                }
            }

            dstRow += mappedResource.RowPitch;
        }

        this->_d3dDeviceContext->Unmap(this->_d3dTexture, 0);

        SelectObject(hdc, prevBitmap);
        SelectObject(hdc, prevGdiFont);
        DeleteObject(bitmap);
        DeleteObject(gdiFont);
        DeleteDC(hdc);

        this->_initialized = true;
    }

    inline void RenderText(Vec2 pos, const std::wstring &text, const Color color, uint32_t flags,
                           const Color outlineColor, float outlineThickness)
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
                    Vertex v[] = {{Vec2{pos.x - 0.5f, pos.y - 0.5f + h}, currentColor, Vec2{tx1, ty2}},
                                  {Vec2{pos.x - 0.5f, pos.y - 0.5f}, currentColor, Vec2{tx1, ty1}},
                                  {Vec2{pos.x - 0.5f + w, pos.y - 0.5f + h}, currentColor, Vec2{tx2, ty2}},

                                  {Vec2{pos.x - 0.5f + w, pos.y - 0.5f}, currentColor, Vec2{tx2, ty1}},
                                  {Vec2{pos.x - 0.5f + w, pos.y - 0.5f + h}, currentColor, Vec2{tx2, ty2}},
                                  {Vec2{pos.x - 0.5f, pos.y - 0.5f}, currentColor, Vec2{tx1, ty1}}};

                    // Outline vertices
                    Vertex outlineV[] = {
                        {Vec2{pos.x - outlineThickness, pos.y - outlineThickness + h}, outlineColor, Vec2{tx1, ty2}},
                        {Vec2{pos.x - outlineThickness, pos.y - outlineThickness}, outlineColor, Vec2{tx1, ty1}},
                        {Vec2{pos.x - outlineThickness + w, pos.y - outlineThickness + h}, outlineColor,
                         Vec2{tx2, ty2}},
                        {Vec2{pos.x - outlineThickness + w, pos.y - outlineThickness}, outlineColor, Vec2{tx2, ty1}},
                        {Vec2{pos.x - outlineThickness + w, pos.y - outlineThickness + h}, outlineColor,
                         Vec2{tx2, ty2}},
                        {Vec2{pos.x - outlineThickness, pos.y - outlineThickness}, outlineColor, Vec2{tx1, ty1}}};

                    // Drop shadow vertices (slightly offset and darker)
                    /* Color shadowColor = Color((currentColor.ToHexColor() >> 24) & 0xff, 0x00, 0x00, 0x00);

                     Vertex shadowV[] = {
                         {Vec4{pos.x + 1.0f, pos.y + 1.0f + h, 0.89f, 1.f}, shadowColor, Vec2{tx1, ty2}},
                         {Vec4{pos.x + 1.0f, pos.y + 1.0f, 0.89f, 1.f}, shadowColor, Vec2{tx1, ty1}},
                         {Vec4{pos.x + 1.0f + w, pos.y + 1.0f + h, 0.89f, 1.f}, shadowColor, Vec2{tx2, ty2}},

                         {Vec4{pos.x + 1.0f + w, pos.y + 1.0f, 0.89f, 1.f}, shadowColor, Vec2{tx2, ty1}},
                         {Vec4{pos.x + 1.0f + w, pos.y + 1.0f + h, 0.89f, 1.f}, shadowColor, Vec2{tx2, ty2}},
                         {Vec4{pos.x + 1.0f, pos.y + 1.0f, 0.89f, 1.f}, shadowColor, Vec2{tx1, ty1}}};*/

                    if (flags & TEXT_FLAG_OUTLINE)
                    {
                        this->_renderList->AddVertices(outlineV, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                                                       this->_d3dTextureView);
                    }
                    else if (flags & TEXT_FLAG_DROPSHADOW)
                    {
                        /* this->_renderList->AddVertices(shadowV, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
                                                        this->_d3dTextureView);*/
                    }

                    this->_renderList->AddVertices(v, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST, this->_d3dTextureView);
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
                width = max(width, rowWidth);
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

        width = max(width, rowWidth);
        return Vec2(width, height);
    }

    inline std::shared_ptr<Font> MakePtr()
    {
        return shared_from_this();
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

        for (wchar_t c = 32; c < 0xFFFF; c++)
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
        int pixelsPerInch = GetDeviceCaps(hdc, LOGPIXELSY);
        int pixelsHeight = -(this->_fontHeigth * pixelsPerInch / pointsPerInch);

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
        for (wchar_t c = 0x20; c < 0x250; c++)
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

    RenderListPtr _renderList;
    ID3D11Device *_d3dDevice;
    ID3D11DeviceContext *_d3dDeviceContext;
    ID3D11Texture2D *_d3dTexture;
    ID3D11ShaderResourceView *_d3dTextureView;
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
    Renderer(ID3D11Device *d3dDevice, uint32_t maxVertices)
        : _d3dDevice(d3dDevice), _d3dDeviceContext(nullptr), _inputLayout(nullptr), _blendState(nullptr),
          _vertexShader(nullptr), _pixelShader(nullptr), _vertexBuffer(nullptr), _screenProjectionBuffer(nullptr),
          _maxVertices(maxVertices), _renderList(std::make_shared<RenderList>(maxVertices)), _nextFontId(1)
    {
        if (!d3dDevice)
        {
            throw std::exception("Renderer::ctor() d3dDevice is null!");
        }

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof(Vertex, pos), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, (UINT)offsetof(Vertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, (UINT)offsetof(Vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        ID3DBlob *vsBlob = nullptr;
        ID3DBlob *psBlob = nullptr;
        ID3DBlob *errorBlob = nullptr;

        this->_d3dDevice->GetImmediateContext(&this->_d3dDeviceContext);

        detail::ThrowIfFailed(D3DCompile(g_vertexShader, strlen(g_vertexShader), nullptr, nullptr, nullptr, "main",
                                         "vs_4_0", 0, 0, &vsBlob, &errorBlob));

        detail::ThrowIfFailed(D3DCompile(g_pixelShader, strlen(g_pixelShader), nullptr, nullptr, nullptr, "main",
                                         "ps_4_0", 0, 0, &psBlob, &errorBlob));

        detail::ThrowIfFailed(this->_d3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                                                   nullptr, &this->_vertexShader));

        detail::ThrowIfFailed(this->_d3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                                                  nullptr, &this->_pixelShader));

        detail::ThrowIfFailed(this->_d3dDevice->CreateInputLayout(layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(),
                                                                  vsBlob->GetBufferSize(), &this->_inputLayout));

        detail::SafeRelease(&vsBlob);
        detail::SafeRelease(&psBlob);

        // Create the blender state
        {
            D3D11_BLEND_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.AlphaToCoverageEnable = false;
            desc.RenderTarget[0].BlendEnable = true;
            desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
            desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            detail::ThrowIfFailed(this->_d3dDevice->CreateBlendState(&desc, &this->_blendState));
        }

        // Create the rasterizer state
        {
            D3D11_RASTERIZER_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.AntialiasedLineEnable = false;
            desc.CullMode = D3D11_CULL_BACK;
            desc.DepthBias = 0;
            desc.DepthBiasClamp = 0.0f;
            desc.DepthClipEnable = true;
            desc.FillMode = D3D11_FILL_SOLID;
            desc.FrontCounterClockwise = false;
            desc.MultisampleEnable = false;
            desc.ScissorEnable = false;
            desc.SlopeScaledDepthBias = 0.0f;

            detail::ThrowIfFailed(this->_d3dDevice->CreateRasterizerState(&desc, &this->_rasterizerState));
        }

        // Create depth-stencil State
        {
            D3D11_DEPTH_STENCIL_DESC desc;
            ZeroMemory(&desc, sizeof(desc));
            desc.DepthEnable = false;
            desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
            desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
            desc.StencilEnable = false;
            desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp =
                D3D11_STENCIL_OP_KEEP;
            desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
            desc.BackFace = desc.FrontFace;

            detail::ThrowIfFailed(this->_d3dDevice->CreateDepthStencilState(&desc, &this->_depthStencilState));
        }

        // Create vertex buffer
        {
            D3D11_BUFFER_DESC desc{};
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.ByteWidth = sizeof(Vertex) * static_cast<UINT>(maxVertices);
            desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags = 0;

            detail::ThrowIfFailed(this->_d3dDevice->CreateBuffer(&desc, nullptr, &this->_vertexBuffer));
        }

        // Create projection buffer
        {
            D3D11_BUFFER_DESC desc{};
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.ByteWidth = sizeof(DirectX::XMMATRIX);
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            desc.MiscFlags = 0;

            detail::ThrowIfFailed(this->_d3dDevice->CreateBuffer(&desc, nullptr, &this->_screenProjectionBuffer));
        }

        // Create font sampler
        {
            D3D11_SAMPLER_DESC desc{};
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
            desc.MipLODBias = 0.f;
            desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
            desc.MinLOD = 0.f;
            desc.MaxLOD = 0.f;

            detail::ThrowIfFailed(this->_d3dDevice->CreateSamplerState(&desc, &this->_fontSampler));
        }

        D3D11_VIEWPORT viewport{};
        UINT numViewports = 1;

        this->_d3dDeviceContext->RSGetViewports(&numViewports, &viewport);

        this->_projMatrix =
            DirectX::XMMatrixOrthographicOffCenterLH(viewport.TopLeftX, viewport.Width, viewport.Height,
                                                     viewport.TopLeftY, viewport.MinDepth, viewport.MaxDepth);

        D3D11_MAPPED_SUBRESOURCE mappedResource;
        detail::ThrowIfFailed(this->_d3dDeviceContext->Map(this->_screenProjectionBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0,
                                                           &mappedResource));
        {
            memcpy(mappedResource.pData, &this->_projMatrix, sizeof(DirectX::XMMATRIX));
        }
        this->_d3dDeviceContext->Unmap(this->_screenProjectionBuffer, 0);
    }

    ~Renderer()
    {
        this->Release();
    }

    inline void Release()
    {
        detail::SafeRelease(&this->_vertexShader);
        detail::SafeRelease(&this->_pixelShader);
        detail::SafeRelease(&this->_vertexBuffer);
        detail::SafeRelease(&this->_screenProjectionBuffer);
        detail::SafeRelease(&this->_inputLayout);
        detail::SafeRelease(&this->_blendState);
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
        // this->AcquireStateBlock();

        for (auto &[_, font] : this->_fonts)
        {
            font->OnResetDevice();
        }
    }

    inline void BeginFrame()
    {
        this->AcquireStateBlock();

        UINT stride = sizeof(Vertex);
        UINT offset = 0;

        this->_d3dDeviceContext->IASetVertexBuffers(0, 1, &this->_vertexBuffer, &stride, &offset);

        this->_d3dDeviceContext->IASetInputLayout(this->_inputLayout);
        this->_d3dDeviceContext->VSSetShader(this->_vertexShader, nullptr, 0);
        this->_d3dDeviceContext->VSSetConstantBuffers(0, 1, &this->_screenProjectionBuffer);
        this->_d3dDeviceContext->PSSetShader(this->_pixelShader, nullptr, 0);
        this->_d3dDeviceContext->PSSetSamplers(0, 1, &this->_fontSampler);

        const float blendFactor[4] = {0.f, 0.f, 0.f, 0.f};
        this->_d3dDeviceContext->OMSetBlendState(this->_blendState, blendFactor, 0xffffffff);
        this->_d3dDeviceContext->OMSetDepthStencilState(this->_depthStencilState, 0);
        this->_d3dDeviceContext->RSSetState(this->_rasterizerState);
    }

    inline void EndFrame()
    {
        this->RestoreStateBlock();
    }

    inline FontHandle AddFont(const std::wstring &fontFamily, long fontHeigth, uint32_t fontFlags = FONT_FLAG_NONE)
    {
        std::shared_ptr<Font> fontPtr =
            std::make_shared<Font>(this->_renderList, this->_d3dDevice, fontFamily, fontHeigth, fontFlags);

        const size_t fontHandle = this->_nextFontId++;
        this->_fonts[fontHandle] = fontPtr;
        return fontHandle;
    }

    inline void AddText(const FontHandle fontId, const std::wstring &text, float x, float y, const Color color,
                        uint32_t flags = FONT_FLAG_NONE, const Color outlineColor = Color(0, 0, 0),
                        float outlineThickness = 2.0f)
    {
        auto font = this->_fonts.find(fontId);
        if (font == this->_fonts.end())
        {
            throw std::exception("AddText(): Font not found!");
        }

        return font->second->RenderText(Vec2(x, y), text, color, flags, outlineColor, outlineThickness);
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

        renderList->AddVertices(v, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    }

    inline void AddRectFilled(const Vec2 &min, const Vec2 &max, const Color color)
    {
        return this->AddRectFilled(this->_renderList, min, max, color);
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

    inline void AddLine(const RenderListPtr &renderList, const Vec2 &v1, const Vec2 &v2, const Color color)
    {
        Vertex v[]{{v1.x, v1.y, color}, {v2.x, v2.y, color}};

        renderList->AddVertices(v, D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    }

    inline void AddLine(const Vec2 &v1, const Vec2 &v2, const Color color)
    {
        return this->AddLine(this->_renderList, v1, v2, color);
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

        renderList->AddVertices(v.data(), v.size(), D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
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
            D3D11_MAPPED_SUBRESOURCE mappedResource;
            detail::ThrowIfFailed(
                this->_d3dDeviceContext->Map(this->_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource));
            {
                memcpy(mappedResource.pData, renderList->_vertices.data(),
                       sizeof(Vertex) * renderList->_vertices.size());
            }
            this->_d3dDeviceContext->Unmap(this->_vertexBuffer, 0);
        }

        size_t pos = 0;

        for (const auto &batch : renderList->_batches)
        {
            this->_d3dDeviceContext->PSSetShaderResources(0, 1, &batch.texture);
            this->_d3dDeviceContext->IASetPrimitiveTopology(batch.topology);
            this->_d3dDeviceContext->Draw(static_cast<uint32_t>(batch.count), static_cast<uint32_t>(pos));

            pos += batch.count;
        }
    }

    inline void Render()
    {
        this->Render(_renderList);
        this->_renderList->Clear();
    }

    inline RendererPtr MakePtr()
    {
        return shared_from_this();
    }

  private:
    struct BACKUP_DX11_STATE
    {
        UINT ScissorRectsCount, ViewportsCount;
        D3D11_RECT ScissorRects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        D3D11_VIEWPORT Viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
        ID3D11RasterizerState *RS;
        ID3D11BlendState *BlendState;
        FLOAT BlendFactor[4];
        UINT SampleMask;
        UINT StencilRef;
        ID3D11DepthStencilState *DepthStencilState;
        ID3D11ShaderResourceView *PSShaderResource;
        ID3D11SamplerState *PSSampler;
        ID3D11PixelShader *PS;
        ID3D11VertexShader *VS;
        ID3D11GeometryShader *GS;
        UINT PSInstancesCount, VSInstancesCount, GSInstancesCount;
        ID3D11ClassInstance *PSInstances[256], *VSInstances[256],
            *GSInstances[256]; // 256 is max according to PSSetShader documentation
        D3D11_PRIMITIVE_TOPOLOGY PrimitiveTopology;
        ID3D11Buffer *IndexBuffer, *VertexBuffer, *VSConstantBuffer;
        UINT IndexBufferOffset, VertexBufferStride, VertexBufferOffset;
        DXGI_FORMAT IndexBufferFormat;
        ID3D11InputLayout *InputLayout;
    };

    BACKUP_DX11_STATE backupState = {};

    inline void AcquireStateBlock()
    {
        backupState.ScissorRectsCount = backupState.ViewportsCount =
            D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        this->_d3dDeviceContext->RSGetScissorRects(&backupState.ScissorRectsCount, backupState.ScissorRects);
        this->_d3dDeviceContext->RSGetViewports(&backupState.ViewportsCount, backupState.Viewports);
        this->_d3dDeviceContext->RSGetState(&backupState.RS);
        this->_d3dDeviceContext->OMGetBlendState(&backupState.BlendState, backupState.BlendFactor,
                                                 &backupState.SampleMask);
        this->_d3dDeviceContext->OMGetDepthStencilState(&backupState.DepthStencilState, &backupState.StencilRef);
        this->_d3dDeviceContext->PSGetShaderResources(0, 1, &backupState.PSShaderResource);
        this->_d3dDeviceContext->PSGetSamplers(0, 1, &backupState.PSSampler);
        backupState.PSInstancesCount = backupState.VSInstancesCount = backupState.GSInstancesCount = 256;
        this->_d3dDeviceContext->PSGetShader(&backupState.PS, backupState.PSInstances, &backupState.PSInstancesCount);
        this->_d3dDeviceContext->VSGetShader(&backupState.VS, backupState.VSInstances, &backupState.VSInstancesCount);
        this->_d3dDeviceContext->VSGetConstantBuffers(0, 1, &backupState.VSConstantBuffer);
        this->_d3dDeviceContext->GSGetShader(&backupState.GS, backupState.GSInstances, &backupState.GSInstancesCount);
    }

    inline void RestoreStateBlock()
    {
        this->_d3dDeviceContext->RSSetScissorRects(backupState.ScissorRectsCount, backupState.ScissorRects);
        this->_d3dDeviceContext->RSSetViewports(backupState.ViewportsCount, backupState.Viewports);
        this->_d3dDeviceContext->RSSetState(backupState.RS);
        if (backupState.RS)
            backupState.RS->Release();
        this->_d3dDeviceContext->OMSetBlendState(backupState.BlendState, backupState.BlendFactor,
                                                 backupState.SampleMask);
        if (backupState.BlendState)
            backupState.BlendState->Release();
        this->_d3dDeviceContext->OMSetDepthStencilState(backupState.DepthStencilState, backupState.StencilRef);
        if (backupState.DepthStencilState)
            backupState.DepthStencilState->Release();
        this->_d3dDeviceContext->PSSetShaderResources(0, 1, &backupState.PSShaderResource);
        if (backupState.PSShaderResource)
            backupState.PSShaderResource->Release();
        this->_d3dDeviceContext->PSSetSamplers(0, 1, &backupState.PSSampler);
        if (backupState.PSSampler)
            backupState.PSSampler->Release();
        this->_d3dDeviceContext->PSSetShader(backupState.PS, backupState.PSInstances, backupState.PSInstancesCount);
        if (backupState.PS)
            backupState.PS->Release();
        for (UINT i = 0; i < backupState.PSInstancesCount; i++)
            if (backupState.PSInstances[i])
                backupState.PSInstances[i]->Release();
        this->_d3dDeviceContext->VSSetShader(backupState.VS, backupState.VSInstances, backupState.VSInstancesCount);
        if (backupState.VS)
            backupState.VS->Release();
        this->_d3dDeviceContext->VSSetConstantBuffers(0, 1, &backupState.VSConstantBuffer);
        if (backupState.VSConstantBuffer)
            backupState.VSConstantBuffer->Release();
        this->_d3dDeviceContext->GSSetShader(backupState.GS, backupState.GSInstances, backupState.GSInstancesCount);
        if (backupState.GS)
            backupState.GS->Release();
        for (UINT i = 0; i < backupState.VSInstancesCount; i++)
            if (backupState.VSInstances[i])
                backupState.VSInstances[i]->Release();
        this->_d3dDeviceContext->IASetPrimitiveTopology(backupState.PrimitiveTopology);
        this->_d3dDeviceContext->IASetIndexBuffer(backupState.IndexBuffer, backupState.IndexBufferFormat,
                                                  backupState.IndexBufferOffset);
        if (backupState.IndexBuffer)
            backupState.IndexBuffer->Release();
        this->_d3dDeviceContext->IASetVertexBuffers(0, 1, &backupState.VertexBuffer, &backupState.VertexBufferStride,
                                                    &backupState.VertexBufferOffset);
        if (backupState.VertexBuffer)
            backupState.VertexBuffer->Release();
        this->_d3dDeviceContext->IASetInputLayout(backupState.InputLayout);
        if (backupState.InputLayout)
            backupState.InputLayout->Release();
    }

    ID3D11DeviceContext *_d3dDeviceContext;
    ID3D11Device *_d3dDevice;
    ID3D11InputLayout *_inputLayout;
    ID3D11BlendState *_blendState;
    ID3D11VertexShader *_vertexShader;
    ID3D11PixelShader *_pixelShader;
    ID3D11Buffer *_vertexBuffer;
    ID3D11Buffer *_screenProjectionBuffer;
    ID3D11SamplerState *_fontSampler;
    ID3D11RasterizerState *_rasterizerState;
    ID3D11DepthStencilState *_depthStencilState;
    DirectX::XMMATRIX _projMatrix;

    uint32_t _maxVertices;
    RenderListPtr _renderList;

    std::unordered_map<FontHandle, FontPtr> _fonts;
    FontHandle _nextFontId;
};
} // namespace CheatRenderFramework