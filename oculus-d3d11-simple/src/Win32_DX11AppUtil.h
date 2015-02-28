/************************************************************************************
Filename    :   Win32_DX11AppUtil.h
Content     :   D3D11 and Application/Window setup functionality for RoomTiny
Created     :   October 20th, 2014
Author      :   Tom Heath
Copyright   :   Copyright 2014 Oculus, Inc. All Rights reserved.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*************************************************************************************/

#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <comdef.h>
#include <comip.h>

#include <d3d11.h>
#include <d3dcompiler.h>

#include "Kernel/OVR_Math.h"

_COM_SMARTPTR_TYPEDEF(IDXGIFactory, __uuidof(IDXGIFactory));
_COM_SMARTPTR_TYPEDEF(IDXGIAdapter, __uuidof(IDXGIAdapter));
_COM_SMARTPTR_TYPEDEF(IDXGIDevice1, __uuidof(IDXGIDevice1));
_COM_SMARTPTR_TYPEDEF(IDXGISwapChain, __uuidof(IDXGISwapChain));
_COM_SMARTPTR_TYPEDEF(ID3D11Device, __uuidof(ID3D11Device));
_COM_SMARTPTR_TYPEDEF(ID3D11Debug, __uuidof(ID3D11Debug));
_COM_SMARTPTR_TYPEDEF(ID3D11DeviceContext, __uuidof(ID3D11DeviceContext));
_COM_SMARTPTR_TYPEDEF(ID3D11DeviceChild, __uuidof(ID3D11DeviceChild));
_COM_SMARTPTR_TYPEDEF(ID3D11Texture2D, __uuidof(ID3D11Texture2D));
_COM_SMARTPTR_TYPEDEF(ID3D11RenderTargetView, __uuidof(ID3D11RenderTargetView));
_COM_SMARTPTR_TYPEDEF(ID3D11ShaderResourceView, __uuidof(ID3D11ShaderResourceView));
_COM_SMARTPTR_TYPEDEF(ID3D11DepthStencilView, __uuidof(ID3D11DepthStencilView));
_COM_SMARTPTR_TYPEDEF(ID3D11Buffer, __uuidof(ID3D11Buffer));
_COM_SMARTPTR_TYPEDEF(ID3D11RasterizerState, __uuidof(ID3D11RasterizerState));
_COM_SMARTPTR_TYPEDEF(ID3D11DepthStencilState, __uuidof(ID3D11DepthStencilState));
_COM_SMARTPTR_TYPEDEF(ID3D11VertexShader, __uuidof(ID3D11VertexShader));
_COM_SMARTPTR_TYPEDEF(ID3D11PixelShader, __uuidof(ID3D11PixelShader));
_COM_SMARTPTR_TYPEDEF(ID3D11ShaderReflection, __uuidof(ID3D11ShaderReflection));
_COM_SMARTPTR_TYPEDEF(ID3D11InputLayout, __uuidof(ID3D11InputLayout));
_COM_SMARTPTR_TYPEDEF(ID3D11SamplerState, __uuidof(ID3D11SamplerState));
_COM_SMARTPTR_TYPEDEF(ID3D10Blob, __uuidof(ID3D10Blob));

using namespace OVR;

// Helper sets a D3D resource name string (used by PIX and debug layer leak reporting).
template <typename T>
inline void SetDebugObjectName(T resource, const char* name, size_t nameLen) {
    ID3D11DeviceChildPtr deviceChild;
    resource->QueryInterface(__uuidof(ID3D11DeviceChild), reinterpret_cast<void**>(&deviceChild));
#if !defined(NO_D3D11_DEBUG_NAME) && (defined(_DEBUG) || defined(PROFILE))
    if (deviceChild) deviceChild->SetPrivateData(WKPDID_D3DDebugObjectName, nameLen, name);
#else
    UNREFERENCED_PARAMETER(resource);
    UNREFERENCED_PARAMETER(name);
    UNREFERENCED_PARAMETER(nameLen);
#endif
}

template <typename T, size_t N>
inline void SetDebugObjectName(T resource, const char(&name)[N]) {
    SetDebugObjectName(resource, name, N - 1);
}

template <typename T>
inline void SetDebugObjectName(T resource, const std::string& name) {
    SetDebugObjectName(resource, name.c_str(), name.size());
}

struct DepthBuffer {
    ID3D11DepthStencilViewPtr TexDsv;
    std::string name;
    DepthBuffer() = default;
    DepthBuffer(const char* name, ID3D11Device* device, Sizei size);
};

struct RenderTarget {
    ID3D11Texture2DPtr Tex;
    ID3D11ShaderResourceViewPtr TexSv;
    ID3D11RenderTargetViewPtr TexRtv;
    Sizei Size = Sizei{};
    std::string name;

    RenderTarget() = default;
    RenderTarget(const char* name, ID3D11Device* device, Sizei size);
};

struct ImageBuffer {
    ID3D11ShaderResourceViewPtr TexSv;
    std::string name;

    ImageBuffer() = default;
    ImageBuffer(const char* name, ID3D11Device* device, ID3D11DeviceContext* deviceContext, Sizei size, unsigned char* data);
};

struct DataBuffer {
    ID3D11BufferPtr D3DBuffer;

    DataBuffer(ID3D11Device* device, D3D11_BIND_FLAG use, const void* buffer, size_t size);

    void Refresh(ID3D11DeviceContext* deviceContext, const void* buffer, size_t size);
};

struct SecondWindow {
    const wchar_t* className = L"OVRSecondWindow";
    HINSTANCE hinst = nullptr;
    int width = 0;
    int height = 0;
    HWND Window = nullptr;
    IDXGISwapChainPtr SwapChain;
    ID3D11RenderTargetViewPtr BackBufferRT;
    std::unique_ptr<DepthBuffer> depthBuffer;

    ~SecondWindow();
    void Init(HINSTANCE hinst, ID3D11Device* device);
};

struct DirectX11 {
    HWND Window = nullptr;
    bool Key[256];
    Sizei RenderTargetSize;
    ID3D11DevicePtr Device;
    ID3D11DeviceContextPtr Context;
    IDXGISwapChainPtr SwapChain;
    ID3D11RenderTargetViewPtr BackBufferRT;
    std::unique_ptr<DataBuffer> UniformBufferGen;
    std::unique_ptr<SecondWindow> secondWindow;

    DirectX11();
    ~DirectX11();
    bool InitWindowAndDevice(HINSTANCE hinst, Recti vp);
    void InitSecondWindow(HINSTANCE hinst);
    void ClearAndSetRenderTarget(ID3D11RenderTargetView* rendertarget, DepthBuffer* depthbuffer,
                                 Recti vp);
    void Render(struct ShaderFill* fill, DataBuffer* vertices, DataBuffer* indices, UINT stride,
                int count);
    bool IsAnyKeyPressed() const;
    void HandleMessages();
    void ReleaseWindow(HINSTANCE hinst);
};

struct Shader {
    ID3D11VertexShaderPtr D3DVert;
    ID3D11PixelShaderPtr D3DPix;
    std::vector<unsigned char> UniformData;

    struct Uniform {
        char Name[40];
        int Offset, Size;
    };

    int numUniformInfo;
    Uniform UniformInfo[10];

    Shader(ID3D11Device* device, ID3D10Blob* s, int which_type);

    void SetUniform(const char* name, int n, const float* v);
};

struct ShaderFill {
    std::unique_ptr<Shader> VShader;
    std::unique_ptr<Shader> PShader;
    std::unique_ptr<ImageBuffer> OneTexture;
    ID3D11InputLayoutPtr InputLayout;
    ID3D11SamplerStatePtr SamplerState;

    ShaderFill(ID3D11Device* device, D3D11_INPUT_ELEMENT_DESC* VertexDesc, int numVertexDesc,
               char* vertexShader, char* pixelShader, std::unique_ptr<ImageBuffer>&& t,
               bool wrap = 1);
};

struct Model {
    struct Color {
        unsigned char R, G, B, A;

        Color(unsigned char r = 0, unsigned char g = 0, unsigned char b = 0, unsigned char a = 0xff)
            : R(r), G(g), B(b), A(a) {}
    };
    struct Vertex {
        Vector3f Pos;
        Color C;
        float U, V;
    };

    Vector3f Pos;
    Quatf Rot;
    Matrix4f Mat;
    int numVertices, numIndices;
    Vertex Vertices[2000];  // Note fixed maximum
    uint16_t Indices[2000];
    std::unique_ptr<ShaderFill> Fill;
    std::unique_ptr<DataBuffer> VertexBuffer;
    std::unique_ptr<DataBuffer> IndexBuffer;

    Model(Vector3f arg_pos, std::unique_ptr<ShaderFill>&& arg_Fill) {
        numVertices = 0;
        numIndices = 0;
        Pos = arg_pos;
        Fill = std::move(arg_Fill);
    }
    Matrix4f& GetMatrix() {
        Mat = Matrix4f(Rot);
        Mat = Matrix4f::Translation(Pos) * Mat;
        return Mat;
    }
    void AddVertex(const Vertex& v) {
        Vertices[numVertices++] = v;
        OVR_ASSERT(numVertices < 2000);
    }
    void AddIndex(uint16_t a) {
        Indices[numIndices++] = a;
        OVR_ASSERT(numIndices < 2000);
    }

    void AllocateBuffers(ID3D11Device* device);

    void Model::AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2,
        Color c);
};
//-------------------------------------------------------------------------
struct Scene {
    int num_models;
    std::unique_ptr<Model> Models[10];

    void Add(Model* n) { Models[num_models++].reset(n); }

    Scene(ID3D11Device* device, ID3D11DeviceContext* deviceContext, int reducedVersion);

    Scene(ID3D11Device* device, int renderTargetWidth, int renderTargetHeight);

    void Render(DirectX11& dx11, Matrix4f view, Matrix4f proj);
};
