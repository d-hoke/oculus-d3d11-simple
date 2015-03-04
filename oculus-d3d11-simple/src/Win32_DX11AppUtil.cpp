#include "Win32_DX11AppUtil.h"

#include <algorithm>
#include <stdexcept>
#include <string>

using namespace std;

void ThrowOnFailure(HRESULT hr) {
    if (FAILED(hr)) {
        _com_error err{hr};
        OutputDebugString(err.ErrorMessage());
        throw std::runtime_error{"Failed HRESULT"};
    }
}

EyeTarget::EyeTarget(ID3D11Device* device, Sizei requestedSize) {
    CD3D11_TEXTURE2D_DESC texDesc(DXGI_FORMAT_R8G8B8A8_UNORM, requestedSize.w, requestedSize.h);
    texDesc.MipLevels = 1;
    texDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    device->CreateTexture2D(&texDesc, nullptr, &tex);
    device->CreateShaderResourceView(tex, nullptr, &srv);
    device->CreateRenderTargetView(tex, nullptr, &rtv);
    tex->GetDesc(&texDesc);
    size = Sizei(texDesc.Width, texDesc.Height);

    CD3D11_TEXTURE2D_DESC dsDesc(DXGI_FORMAT_D32_FLOAT, texDesc.Width, texDesc.Height);
    dsDesc.MipLevels = 1;
    dsDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ID3D11Texture2DPtr dsTex;
    device->CreateTexture2D(&dsDesc, nullptr, &dsTex);
    device->CreateDepthStencilView(dsTex, nullptr, &dsv);

    viewport.Pos = Vector2i(0, 0);
    viewport.Size = Sizei(texDesc.Width, texDesc.Height);
}

LRESULT CALLBACK SystemWindowProc(HWND arg_hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static DirectX11* dx11 = nullptr;

    switch (msg) {
        case (WM_NCCREATE): {
            CREATESTRUCT* createStruct = reinterpret_cast<CREATESTRUCT*>(lp);
            if (createStruct->lpCreateParams) {
                dx11 = reinterpret_cast<DirectX11*>(createStruct->lpCreateParams);
                dx11->Window = arg_hwnd;
            }
            break;
        }
        case WM_KEYDOWN:
            dx11->Key[(unsigned)wp] = true;
            break;
        case WM_KEYUP:
            dx11->Key[(unsigned)wp] = false;
            break;
        case WM_SETFOCUS:
            SetCapture(dx11->Window);
            ShowCursor(FALSE);
            break;
        case WM_KILLFOCUS:
            ReleaseCapture();
            ShowCursor(TRUE);
            break;
    }
    return DefWindowProc(arg_hwnd, msg, wp, lp);
}

DirectX11::DirectX11(HINSTANCE hinst_, Recti vp) : hinst(hinst_) {
    fill(begin(Key), end(Key), false);

    Window = [this, vp] {
        const auto className = L"OVRAppWindow";
        WNDCLASSW wc{};
        wc.lpszClassName = className;
        wc.lpfnWndProc = SystemWindowProc;
        RegisterClassW(&wc);

        const DWORD wsStyle = WS_POPUP | WS_OVERLAPPEDWINDOW;
        const auto sizeDivisor = 2;
        RECT winSize = {0, 0, vp.w / sizeDivisor, vp.h / sizeDivisor};
        AdjustWindowRect(&winSize, wsStyle, false);
        return CreateWindowW(className, L"OculusRoomTiny", wsStyle | WS_VISIBLE, vp.x, vp.y,
                             winSize.right - winSize.left, winSize.bottom - winSize.top, nullptr,
                             nullptr, hinst, this);
    }();

    [this, vp] {
        IDXGIFactoryPtr DXGIFactory;
        ThrowOnFailure(
            CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&DXGIFactory)));

        IDXGIAdapterPtr Adapter;
        ThrowOnFailure(DXGIFactory->EnumAdapters(0, &Adapter));

        const UINT creationFlags = [] {
#ifdef _DEBUG
            return D3D11_CREATE_DEVICE_DEBUG;
#else
            return 0u;
#endif
        }();

        DXGI_SWAP_CHAIN_DESC scDesc{};
        scDesc.BufferCount = 2;
        scDesc.BufferDesc.Width = vp.GetSize().w;
        scDesc.BufferDesc.Height = vp.GetSize().h;
        scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scDesc.OutputWindow = Window;
        scDesc.SampleDesc.Count = 1;
        scDesc.SampleDesc.Quality = 0;
        scDesc.Windowed = TRUE;
        scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        ThrowOnFailure(D3D11CreateDeviceAndSwapChain(
            Adapter, Adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr,
            creationFlags, nullptr, 0, D3D11_SDK_VERSION, &scDesc, &SwapChain, &Device, nullptr,
            &Context));
    }();

    ID3D11Texture2DPtr backBuffer;
    ThrowOnFailure(
        SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer)));
    ThrowOnFailure(Device->CreateRenderTargetView(backBuffer, nullptr, &BackBufferRT));

    {
        CD3D11_BUFFER_DESC desc(2000, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC,
                                D3D11_CPU_ACCESS_WRITE);
        ThrowOnFailure(Device->CreateBuffer(&desc, nullptr, &UniformBufferGen));
    }

    [this] {
        CD3D11_RASTERIZER_DESC rs{D3D11_DEFAULT};
        ID3D11RasterizerStatePtr Rasterizer;
        ThrowOnFailure(Device->CreateRasterizerState(&rs, &Rasterizer));
        Context->RSSetState(Rasterizer);
    }();

    [this] {
        CD3D11_DEPTH_STENCIL_DESC dss{D3D11_DEFAULT};
        ID3D11DepthStencilStatePtr DepthState;
        ThrowOnFailure(Device->CreateDepthStencilState(&dss, &DepthState));
        Context->OMSetDepthStencilState(DepthState, 0);
    }();

    CD3D11_SAMPLER_DESC ss{ D3D11_DEFAULT };
    ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    ss.Filter = D3D11_FILTER_ANISOTROPIC;
    ss.MaxAnisotropy = 8;
    Device->CreateSamplerState(&ss, &SamplerState);
}

DirectX11::~DirectX11() {
    Context->ClearState();
    Context->Flush();

    DestroyWindow(Window);
    UnregisterClassW(L"OVRAppWindow", hinst);
}

void DirectX11::ClearAndSetEyeTarget(const EyeTarget& eyeTarget) {
    const float black[] = {0.f, 0.f, 0.f, 1.f};
    ID3D11RenderTargetView* rtvs[] = { eyeTarget.rtv };
    Context->OMSetRenderTargets(1, rtvs, eyeTarget.dsv);
    Context->ClearRenderTargetView(eyeTarget.rtv, black);
    Context->ClearDepthStencilView(eyeTarget.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
    D3D11_VIEWPORT d3dvp;
    d3dvp.TopLeftX = static_cast<float>(eyeTarget.viewport.Pos.x);
    d3dvp.TopLeftY = static_cast<float>(eyeTarget.viewport.Pos.y);
    d3dvp.Width = static_cast<float>(eyeTarget.viewport.Size.w);
    d3dvp.Height = static_cast<float>(eyeTarget.viewport.Size.h);
    d3dvp.MinDepth = 0.f;
    d3dvp.MaxDepth = 1.f;
    Context->RSSetViewports(1, &d3dvp);
}

void DirectX11::Render(VertexShader* vertexShader, ID3D11PixelShader* pixelShader,
                       ID3D11InputLayout* inputLayout, ID3D11ShaderResourceView* texSrv, ID3D11Buffer* vertices,
                       ID3D11Buffer* indices, UINT stride, int count) {
    Context->IASetInputLayout(inputLayout);
    Context->IASetIndexBuffer(indices, DXGI_FORMAT_R16_UINT, 0);

    UINT offset = 0;
    ID3D11Buffer* vertexBuffers[] = {vertices};
    Context->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);

    D3D11_MAPPED_SUBRESOURCE map;
    Context->Map(UniformBufferGen, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
    memcpy(map.pData, vertexShader->UniformData.data(), vertexShader->UniformData.size());
    Context->Unmap(UniformBufferGen, 0);

    ID3D11Buffer* vsConstantBuffers[] = {UniformBufferGen};
    Context->VSSetConstantBuffers(0, 1, vsConstantBuffers);

    Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->VSSetShader(vertexShader->D3DVert, nullptr, 0);
    Context->PSSetShader(pixelShader, nullptr, 0);
    ID3D11SamplerState* samplerStates[] = { SamplerState };
    Context->PSSetSamplers(0, 1, samplerStates);

    if (texSrv) {
        ID3D11ShaderResourceView* srvs[] = {texSrv};
        Context->PSSetShaderResources(0, 1, srvs);
    }
    Context->DrawIndexed(count, 0, 0);
}

bool DirectX11::IsAnyKeyPressed() const {
    return any_of(begin(Key), end(Key), [](bool b) { return b; });
}

VertexShader::VertexShader(ID3D11Device* device, ID3D10Blob* s) {
    ThrowOnFailure(
        device->CreateVertexShader(s->GetBufferPointer(), s->GetBufferSize(), NULL, &D3DVert));

    ID3D11ShaderReflectionPtr ref;
    D3DReflect(s->GetBufferPointer(), s->GetBufferSize(), IID_ID3D11ShaderReflection,
               reinterpret_cast<void**>(&ref));
    ID3D11ShaderReflectionConstantBuffer* buf = ref->GetConstantBufferByIndex(0);
    D3D11_SHADER_BUFFER_DESC bufd;
    ThrowOnFailure(buf->GetDesc(&bufd));

    for (unsigned i = 0; i < bufd.Variables; ++i) {
        ID3D11ShaderReflectionVariable* var = buf->GetVariableByIndex(i);
        D3D11_SHADER_VARIABLE_DESC vd;
        var->GetDesc(&vd);
        UniformOffsets[vd.Name] = vd.StartOffset;
    }
    UniformData.resize(bufd.Size);
}

void VertexShader::SetUniform(const char* name, int n, const float* v) {
    memcpy(UniformData.data() + UniformOffsets[name], v, n * sizeof(float));
}

void Model::AllocateBuffers(ID3D11Device* device) {
    D3D11_SUBRESOURCE_DATA sr{};

    const CD3D11_BUFFER_DESC vbdesc(Vertices.size() * sizeof(Vertices[0]), D3D11_BIND_VERTEX_BUFFER,
                                    D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    sr.pSysMem = Vertices.data();
    ThrowOnFailure(device->CreateBuffer(&vbdesc, &sr, &VertexBuffer));

    const CD3D11_BUFFER_DESC ibdesc(Indices.size() * sizeof(Indices[0]), D3D11_BIND_INDEX_BUFFER,
                                    D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    sr.pSysMem = Indices.data();
    ThrowOnFailure(device->CreateBuffer(&ibdesc, &sr, &IndexBuffer));
}

void Model::AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2, Color c) {
    const uint16_t CubeIndices[] = {0,  1,  3,  3,  1,  2,  5,  4,  6,  6,  4,  7,
                                    8,  9,  11, 11, 9,  10, 13, 12, 14, 14, 12, 15,
                                    16, 17, 19, 19, 17, 18, 21, 20, 22, 22, 20, 23};

    const uint16_t offset = static_cast<uint16_t>(Vertices.size());
    for (const auto& index : CubeIndices) Indices.push_back(index + offset);

    const Vector3f Vert[][2] = {
        Vector3f(x1, y2, z1), Vector3f(z1, x1), Vector3f(x2, y2, z1), Vector3f(z1, x2),
        Vector3f(x2, y2, z2), Vector3f(z2, x2), Vector3f(x1, y2, z2), Vector3f(z2, x1),
        Vector3f(x1, y1, z1), Vector3f(z1, x1), Vector3f(x2, y1, z1), Vector3f(z1, x2),
        Vector3f(x2, y1, z2), Vector3f(z2, x2), Vector3f(x1, y1, z2), Vector3f(z2, x1),
        Vector3f(x1, y1, z2), Vector3f(z2, y1), Vector3f(x1, y1, z1), Vector3f(z1, y1),
        Vector3f(x1, y2, z1), Vector3f(z1, y2), Vector3f(x1, y2, z2), Vector3f(z2, y2),
        Vector3f(x2, y1, z2), Vector3f(z2, y1), Vector3f(x2, y1, z1), Vector3f(z1, y1),
        Vector3f(x2, y2, z1), Vector3f(z1, y2), Vector3f(x2, y2, z2), Vector3f(z2, y2),
        Vector3f(x1, y1, z1), Vector3f(x1, y1), Vector3f(x2, y1, z1), Vector3f(x2, y1),
        Vector3f(x2, y2, z1), Vector3f(x2, y2), Vector3f(x1, y2, z1), Vector3f(x1, y2),
        Vector3f(x1, y1, z2), Vector3f(x1, y1), Vector3f(x2, y1, z2), Vector3f(x2, y1),
        Vector3f(x2, y2, z2), Vector3f(x2, y2), Vector3f(x1, y2, z2), Vector3f(x1, y2),
    };

    for (int v = 0; v < 24; v++) {
        Vertex vvv;
        vvv.Pos = Vert[v][0];
        vvv.U = Vert[v][1].x;
        vvv.V = Vert[v][1].y;
        const float dist1 = (vvv.Pos - Vector3f(-2, 4, -2)).Length();
        const float dist2 = (vvv.Pos - Vector3f(3, 4, -3)).Length();
        const float dist3 = (vvv.Pos - Vector3f(-4, 3, 25)).Length();
        const int bri = rand() % 160;
        const float mod = (bri + 192.0f * (0.65f + 8 / dist1 + 1 / dist2 + 4 / dist3)) / 255.0f;
        vvv.C.R = static_cast<unsigned char>(min(c.R * mod, 255.0f));
        vvv.C.G = static_cast<unsigned char>(min(c.G * mod, 255.0f));
        vvv.C.B = static_cast<unsigned char>(min(c.B * mod, 255.0f));
        Vertices.push_back(vvv);
    }
}

Scene::Scene(ID3D11Device* device, ID3D11DeviceContext* deviceContext) {
    D3D11_INPUT_ELEMENT_DESC ModelVertexDesc[] = {
        {"Position", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Model::Vertex, Pos),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"Color", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(Model::Vertex, C),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TexCoord", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Model::Vertex, U),
         D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    ID3D10BlobPtr blobData;
    const char* VertexShaderSrc = R"(
        float4x4 Proj, View;
        float4 NewCol;
        void main(in float4 Position : POSITION, in float4 Color : COLOR0, in float2 TexCoord : TEXCOORD0,
                  out float4 oPosition : SV_Position, out float4 oColor : COLOR0, out float2 oTexCoord : TEXCOORD0)
        {
            oPosition = mul(Proj, mul(View, Position)); 
            oTexCoord = TexCoord; 
            oColor = Color; 
        })";

    ThrowOnFailure(D3DCompile(VertexShaderSrc, strlen(VertexShaderSrc), nullptr, nullptr, nullptr,
                              "main", "vs_4_0", 0, 0, &blobData, nullptr));
    VShader = std::make_unique<VertexShader>(device, blobData);
    device->CreateInputLayout(ModelVertexDesc, 3, blobData->GetBufferPointer(),
                              blobData->GetBufferSize(), &InputLayout);

    const char* PixelShaderSrc = R"(
        Texture2D Texture : register(t0);
        SamplerState Linear : register(s0); 
        float4 main(in float4 Position : SV_Position, in float4 Color : COLOR0, in float2 TexCoord : TEXCOORD0) : SV_Target
        {
            return Color * Texture.Sample(Linear, TexCoord);
        })";

    ThrowOnFailure(D3DCompile(PixelShaderSrc, strlen(PixelShaderSrc), nullptr, nullptr, nullptr,
                              "main", "ps_4_0", 0, 0, &blobData, nullptr));
    [device, blobData, this] {
        ThrowOnFailure(device->CreatePixelShader(blobData->GetBufferPointer(),
                                                 blobData->GetBufferSize(), nullptr, &PShader));
    }();

    // Construct textures
    const auto texWidthHeight = 256;
    const auto texCount = 5;
    static Model::Color tex_pixels[texCount][texWidthHeight * texWidthHeight];
    ID3D11ShaderResourceViewPtr generated_texture[texCount];

    for (int k = 0; k < texCount; ++k) {
        for (int j = 0; j < texWidthHeight; ++j)
            for (int i = 0; i < texWidthHeight; ++i) {
                if (k == 0)
                    tex_pixels[0][j * texWidthHeight + i] =
                        (((i >> 7) ^ (j >> 7)) & 1) ? Model::Color(180, 180, 180, 255)
                                                    : Model::Color(80, 80, 80, 255);  // floor
                if (k == 1)
                    tex_pixels[1][j * texWidthHeight + i] =
                        (((j / 4 & 15) == 0) ||
                         (((i / 4 & 15) == 0) && ((((i / 4 & 31) == 0) ^ ((j / 4 >> 4) & 1)) == 0)))
                            ? Model::Color(60, 60, 60, 255)
                            : Model::Color(180, 180, 180, 255);  // wall
                if (k == 2 || k == 4)
                    tex_pixels[k][j * texWidthHeight + i] =
                        (i / 4 == 0 || j / 4 == 0) ? Model::Color(80, 80, 80, 255)
                                                   : Model::Color(180, 180, 180, 255);  // ceiling
                if (k == 3)
                    tex_pixels[3][j * texWidthHeight + i] =
                        Model::Color(128, 128, 128, 255);  // blank
            }

        generated_texture[k] = [device, deviceContext, texWidthHeight](unsigned char* data) {
            CD3D11_TEXTURE2D_DESC dsDesc(DXGI_FORMAT_R8G8B8A8_UNORM, texWidthHeight,
                                         texWidthHeight);
            ID3D11Texture2DPtr tex;
            device->CreateTexture2D(&dsDesc, nullptr, &tex);
            ID3D11ShaderResourceViewPtr texSrv;
            device->CreateShaderResourceView(tex, nullptr, &texSrv);

            // Note data is trashed
            auto wh = texWidthHeight;
            tex->GetDesc(&dsDesc);
            for (auto level = 0u; level < dsDesc.MipLevels; ++level) {
                deviceContext->UpdateSubresource(tex, level, nullptr, data, wh * 4, wh * 4);
                for (int j = 0; j < (wh & ~1); j += 2) {
                    const uint8_t* psrc = data + (wh * j * 4);
                    uint8_t* pdest = data + ((wh >> 1) * (j >> 1) * 4);
                    for (int i = 0; i < (wh >> 1); ++i, psrc += 8, pdest += 4) {
                        pdest[0] =
                            (((int)psrc[0]) + psrc[4] + psrc[wh * 4 + 0] + psrc[wh * 4 + 4]) >> 2;
                        pdest[1] =
                            (((int)psrc[1]) + psrc[5] + psrc[wh * 4 + 1] + psrc[wh * 4 + 5]) >> 2;
                        pdest[2] =
                            (((int)psrc[2]) + psrc[6] + psrc[wh * 4 + 2] + psrc[wh * 4 + 6]) >> 2;
                        pdest[3] =
                            (((int)psrc[3]) + psrc[7] + psrc[wh * 4 + 3] + psrc[wh * 4 + 7]) >> 2;
                    }
                }
                wh >>= 1;
            }
            return texSrv;
        }(&tex_pixels[k][0].R);
    }

    // Construct geometry
    Model* m = new Model(Vector3f(0, 0, 0), std::move(generated_texture[2]));  // Moving box
    m->AddSolidColorBox(0, 0, 0, +1.0f, +1.0f, 1.0f, Model::Color(64, 64, 64));
    m->AllocateBuffers(device);
    Models.emplace_back(m);

    m = new Model(Vector3f(0, 0, 0), std::move(generated_texture[1]));  // Walls
    m->AddSolidColorBox(-10.1f, 0.0f, -20.0f, -10.0f, 4.0f, 20.0f,
                        Model::Color(128, 128, 128));  // Left Wall
    m->AddSolidColorBox(-10.0f, -0.1f, -20.1f, 10.0f, 4.0f, -20.0f,
                        Model::Color(128, 128, 128));  // Back Wall
    m->AddSolidColorBox(10.0f, -0.1f, -20.0f, 10.1f, 4.0f, 20.0f,
                        Model::Color(128, 128, 128));  // Right Wall
    m->AllocateBuffers(device);
    Models.emplace_back(m);

    m = new Model(Vector3f(0, 0, 0), std::move(generated_texture[0]));  // Floors
    m->AddSolidColorBox(-10.0f, -0.1f, -20.0f, 10.0f, 0.0f, 20.1f,
                        Model::Color(128, 128, 128));  // Main floor
    m->AddSolidColorBox(-15.0f, -6.1f, 18.0f, 15.0f, -6.0f, 30.0f,
                        Model::Color(128, 128, 128));  // Bottom floor
    m->AllocateBuffers(device);
    Models.emplace_back(m);

    m = new Model(Vector3f(0, 0, 0), std::move(generated_texture[4]));  // Ceiling
    m->AddSolidColorBox(-10.0f, 4.0f, -20.0f, 10.0f, 4.1f, 20.1f, Model::Color(128, 128, 128));
    m->AllocateBuffers(device);
    Models.emplace_back(m);

    m = new Model(Vector3f(0, 0, 0), std::move(generated_texture[3]));  // Fixtures & furniture
    m->AddSolidColorBox(9.5f, 0.75f, 3.0f, 10.1f, 2.5f, 3.1f,
                        Model::Color(96, 96, 96));  // Right side shelf// Verticals
    m->AddSolidColorBox(9.5f, 0.95f, 3.7f, 10.1f, 2.75f, 3.8f,
                        Model::Color(96, 96, 96));  // Right side shelf
    m->AddSolidColorBox(9.55f, 1.20f, 2.5f, 10.1f, 1.30f, 3.75f,
                        Model::Color(96, 96, 96));  // Right side shelf// Horizontals
    m->AddSolidColorBox(9.55f, 2.00f, 3.05f, 10.1f, 2.10f, 4.2f,
                        Model::Color(96, 96, 96));  // Right side shelf
    m->AddSolidColorBox(5.0f, 1.1f, 20.0f, 10.0f, 1.2f, 20.1f,
                        Model::Color(96, 96, 96));  // Right railing
    m->AddSolidColorBox(-10.0f, 1.1f, 20.0f, -5.0f, 1.2f, 20.1f,
                        Model::Color(96, 96, 96));  // Left railing
    for (float f = 5.0f; f <= 9.0f; f += 1.0f) {
        m->AddSolidColorBox(f, 0.0f, 20.0f, f + 0.1f, 1.1f, 20.1f,
                            Model::Color(128, 128, 128));  // Left Bars
        m->AddSolidColorBox(-f, 1.1f, 20.0f, -f - 0.1f, 0.0f, 20.1f,
                            Model::Color(128, 128, 128));  // Right Bars
    }
    m->AddSolidColorBox(-1.8f, 0.8f, 1.0f, 0.0f, 0.7f, 0.0f, Model::Color(128, 128, 0));  // Table
    m->AddSolidColorBox(-1.8f, 0.0f, 0.0f, -1.7f, 0.7f, 0.1f,
                        Model::Color(128, 128, 0));  // Table Leg
    m->AddSolidColorBox(-1.8f, 0.7f, 1.0f, -1.7f, 0.0f, 0.9f,
                        Model::Color(128, 128, 0));  // Table Leg
    m->AddSolidColorBox(0.0f, 0.0f, 1.0f, -0.1f, 0.7f, 0.9f,
                        Model::Color(128, 128, 0));  // Table Leg
    m->AddSolidColorBox(0.0f, 0.7f, 0.0f, -0.1f, 0.0f, 0.1f,
                        Model::Color(128, 128, 0));  // Table Leg
    m->AddSolidColorBox(-1.4f, 0.5f, -1.1f, -0.8f, 0.55f, -0.5f,
                        Model::Color(44, 44, 128));  // Chair Set
    m->AddSolidColorBox(-1.4f, 0.0f, -1.1f, -1.34f, 1.0f, -1.04f,
                        Model::Color(44, 44, 128));  // Chair Leg 1
    m->AddSolidColorBox(-1.4f, 0.5f, -0.5f, -1.34f, 0.0f, -0.56f,
                        Model::Color(44, 44, 128));  // Chair Leg 2
    m->AddSolidColorBox(-0.8f, 0.0f, -0.5f, -0.86f, 0.5f, -0.56f,
                        Model::Color(44, 44, 128));  // Chair Leg 2
    m->AddSolidColorBox(-0.8f, 1.0f, -1.1f, -0.86f, 0.0f, -1.04f,
                        Model::Color(44, 44, 128));  // Chair Leg 2
    m->AddSolidColorBox(-1.4f, 0.97f, -1.05f, -0.8f, 0.92f, -1.10f,
                        Model::Color(44, 44, 128));  // Chair Back high bar

    for (float f = 3.0f; f <= 6.6f; f += 0.4f)
        m->AddSolidColorBox(-3, 0.0f, f, -2.9f, 1.3f, f + 0.1f, Model::Color(64, 64, 64));  // Posts

    m->AllocateBuffers(device);
    Models.emplace_back(m);
}

void Scene::Render(DirectX11& dx11, Matrix4f view, Matrix4f proj) {
    for (auto& model : Models) {
        Matrix4f modelmat = model->GetMatrix();
        Matrix4f mat = (view * modelmat).Transposed();

        VShader->SetUniform("View", 16, (float*)&mat);
        VShader->SetUniform("Proj", 16, (float*)&proj);

        dx11.Render(VShader.get(), PShader, InputLayout, model->textureSrv, model->VertexBuffer, model->IndexBuffer,
                    sizeof(Model::Vertex), model->Indices.size());
    }
}
