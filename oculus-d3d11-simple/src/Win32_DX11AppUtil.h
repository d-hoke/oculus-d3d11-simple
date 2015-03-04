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
#include <unordered_map>
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

    EyeTarget(ID3D11Device* device, Sizei size);
};

struct VertexShader {
    ID3D11VertexShaderPtr D3DVert;
    std::vector<unsigned char> UniformData;
    std::unordered_map<std::string, int> UniformOffsets;

    VertexShader(ID3D11Device* device, ID3D10Blob* s);

    void SetUniform(const char* name, int n, const float* v);
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
    ID3D11SamplerStatePtr SamplerState;
    std::unique_ptr<VertexShader> VShader;
    ID3D11PixelShaderPtr PShader;
    ID3D11InputLayoutPtr InputLayout;

    DirectX11(HINSTANCE hinst, Recti vp);
    ~DirectX11();
    void ClearAndSetEyeTarget(const EyeTarget& eyeTarget);
    void Render(ID3D11ShaderResourceView* texSrv, ID3D11Buffer* vertices,
                ID3D11Buffer* indices, UINT stride, int count);
    bool IsAnyKeyPressed() const;
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
    std::vector<Vertex> Vertices;
    std::vector<uint16_t> Indices;
    ID3D11BufferPtr VertexBuffer;
    ID3D11BufferPtr IndexBuffer;
    ID3D11ShaderResourceViewPtr textureSrv;

    Model(Vector3f pos_, ID3D11ShaderResourceView* texSrv) : Pos(pos_), textureSrv(texSrv) {}

    Matrix4f GetMatrix() { return Matrix4f::Translation(Pos) * Matrix4f(Rot); }
    void AllocateBuffers(ID3D11Device* device);
    void Model::AddSolidColorBox(float x1, float y1, float z1, float x2, float y2, float z2,
                                 Color c);
};

struct Scene {
    std::vector<std::unique_ptr<Model>> Models;

    Scene(ID3D11Device* device, ID3D11DeviceContext* deviceContext);

    void Render(DirectX11& dx11, Matrix4f view, Matrix4f proj);
};
