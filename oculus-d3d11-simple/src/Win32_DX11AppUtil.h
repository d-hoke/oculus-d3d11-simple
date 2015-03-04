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

#include <OVR_CAPI.h>

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

struct EyeTarget {
    ID3D11Texture2DPtr tex;
    ID3D11ShaderResourceViewPtr srv;
    ID3D11RenderTargetViewPtr rtv;
    ID3D11DepthStencilViewPtr dsv;
    ovrRecti viewport;
    Sizei size;
    
    EyeTarget() = default;
    EyeTarget(ID3D11Device* device, Sizei size);
};

struct ImageBuffer {
    ID3D11ShaderResourceViewPtr TexSv;

    ImageBuffer() = default;
    ImageBuffer(ID3D11Device* device, ID3D11DeviceContext* deviceContext, Sizei size, unsigned char* data);
};

struct DirectX11 {
    HINSTANCE hinst = nullptr;
    HWND Window = nullptr;
    bool Key[256];
    ID3D11DevicePtr Device;
    ID3D11DeviceContextPtr Context;
    IDXGISwapChainPtr SwapChain;
    ID3D11RenderTargetViewPtr BackBufferRT;
    ID3D11BufferPtr UniformBufferGen;

    DirectX11(HINSTANCE hinst, Recti vp);
    ~DirectX11();
    void ClearAndSetRenderTarget(ID3D11RenderTargetView* rendertarget,
                                 ID3D11DepthStencilView* depthbuffer, Recti vp);
    void Render(struct ShaderFill* fill, ID3D11Buffer* vertices, ID3D11Buffer* indices, UINT stride,
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

    std::vector<Uniform> UniformInfo;

    Shader(ID3D11Device* device, ID3D10Blob* s, int which_type);

    void SetUniform(const char* name, int n, const float* v);
};

struct ShaderFill {
    Shader* VShader;
    Shader* PShader;
    ID3D11InputLayout* InputLayout;

    std::unique_ptr<ImageBuffer> OneTexture;
    ID3D11SamplerStatePtr SamplerState;

    ShaderFill(ID3D11Device* device, Shader* vertexShader, Shader* pixelShader, ID3D11InputLayout* inputLayout, std::unique_ptr<ImageBuffer>&& t);
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
    std::vector<Vertex> Vertices;
    std::vector<uint16_t> Indices;
    std::unique_ptr<ShaderFill> Fill;
    ID3D11BufferPtr VertexBuffer;
    ID3D11BufferPtr IndexBuffer;

    Model(Vector3f arg_pos, std::unique_ptr<ShaderFill>&& arg_Fill) {
        Pos = arg_pos;
        Fill = std::move(arg_Fill);
    }

    Matrix4f& GetMatrix() {
        Mat = Matrix4f(Rot);
        Mat = Matrix4f::Translation(Pos) * Mat;
        return Mat;
    }
    void AllocateBuffers(ID3D11Device* device);
    void Model::AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2,
                                 Color c);
};

struct Scene {
    std::unique_ptr<Shader> VShader;
    std::unique_ptr<Shader> PShader;
    ID3D11InputLayoutPtr InputLayout;
    std::vector<std::unique_ptr<Model>> Models;

    Scene(ID3D11Device* device, ID3D11DeviceContext* deviceContext);

    void Render(DirectX11& dx11, Matrix4f view, Matrix4f proj);
};
