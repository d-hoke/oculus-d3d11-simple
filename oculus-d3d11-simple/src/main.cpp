/************************************************************************************
This is a cut down, cleaned up version of the Oculus SDK tiny room demo.
Simplifications include SDK distortion and Direct to Rift support only.
Original copyright notice below:

---

Content     :   First-person view test application for Oculus Rift
Created     :   October 4, 2012
Authors     :   Tom Heath, Michael Antonov, Andrew Reisse, Volga Aksoy
Copyright   :   Copyright 2012 Oculus, Inc. All Rights reserved.

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

// This app renders a simple room, with right handed coord system :  Y->Up, Z->Back, X->Right
// 'W','A','S','D' and arrow keys to navigate.

#include "Win32_DX11AppUtil.h"  // Include Non-SDK supporting utilities
#include <OVR_CAPI.h>           // Include the OculusVR SDK

#define OVR_D3D_VERSION 11
#include <OVR_CAPI_D3D.h>  // Include SDK-rendered code for the D3D version

#include <algorithm>
#include <array>
#include <stdexcept>

void throwOnError(ovrBool res, ovrHmd hmd = nullptr) {
    if (!res) {
        auto errString = ovrHmd_GetLastError(hmd);
#ifdef _DEBUG
        OutputDebugStringA(errString);
#endif
        throw std::runtime_error{errString};
    }
}

using namespace std;

template <typename Func>
struct scope_exit {
    scope_exit(Func f) : onExit(f) {}
    ~scope_exit() { onExit(); }

private:
    Func onExit;
};

template <typename Func>
scope_exit<Func> on_scope_exit(Func f) {
    return scope_exit<Func>{f};
};

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR /*args*/, int) {
    // Initialize the OVR SDK
    throwOnError(ovr_Initialize());
    auto ovr = on_scope_exit([] { ovr_Shutdown(); });

    // Create the HMD
    auto hmdCreate = [] {
        auto hmd = ovrHmd_Create(0);
        if (!hmd) {
            MessageBoxA(NULL, "Oculus Rift not detected.\nAttempting to create debug HMD.", "",
                        MB_OK);

            // If we didn't detect an Hmd, create a simulated one for debugging.
            hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
            throwOnError(hmd != nullptr);
        }

        if (hmd->ProductName[0] == '\0')
            MessageBoxA(NULL, "Rift detected, display not enabled.", "", MB_OK);
        return hmd;
    };
    auto hmdDestroy = [](ovrHmd hmd) { ovrHmd_Destroy(hmd); };
    unique_ptr<const ovrHmdDesc, decltype(hmdDestroy)> hmd{hmdCreate(), hmdDestroy};

    // Create the Direct3D11 device and window
    DirectX11 DX11(hinst, Recti(hmd->WindowsPos, hmd->Resolution));

    // Attach HMD to window and initialize tracking
    throwOnError(ovrHmd_AttachToWindow(hmd.get(), DX11.Window, nullptr, nullptr), hmd.get());
    ovrHmd_SetEnabledCaps(hmd.get(), ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction);
    throwOnError(ovrHmd_ConfigureTracking(hmd.get(), ovrTrackingCap_Orientation |
                                                         ovrTrackingCap_MagYawCorrection |
                                                         ovrTrackingCap_Position,
                                          0),
                 hmd.get());

    // Create the eye render targets.
    EyeTarget eyeTargets[] = {
        {DX11.Device,
         ovrHmd_GetFovTextureSize(hmd.get(), ovrEye_Left, hmd->DefaultEyeFov[ovrEye_Left], 1.0f)},
        {DX11.Device, ovrHmd_GetFovTextureSize(hmd.get(), ovrEye_Right,
                                               hmd->DefaultEyeFov[ovrEye_Right], 1.0f)}};

    // Configure SDK rendering
    auto eyeRenderDesc = [&DX11, &hmd] {
        ovrD3D11Config d3d11cfg;
        d3d11cfg.D3D11.Header.API = ovrRenderAPI_D3D11;
        d3d11cfg.D3D11.Header.BackBufferSize = hmd->Resolution;
        d3d11cfg.D3D11.Header.Multisample = 1;
        d3d11cfg.D3D11.pDevice = DX11.Device;
        d3d11cfg.D3D11.pDeviceContext = DX11.Context;
        d3d11cfg.D3D11.pBackBufferRT = DX11.BackBufferRT;
        d3d11cfg.D3D11.pSwapChain = DX11.SwapChain;

        array<ovrEyeRenderDesc, 2> eyeRenderDesc;
        throwOnError(
            ovrHmd_ConfigureRendering(hmd.get(), &d3d11cfg.Config,
            ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette |
            ovrDistortionCap_TimeWarp | ovrDistortionCap_Overdrive,
            hmd->DefaultEyeFov, &eyeRenderDesc[0]),
            hmd.get());
        return eyeRenderDesc;
    }();

    // Create the room model
    Scene roomScene(DX11.Device, DX11.Context);

    float Yaw(3.141592f);             // Horizontal rotation of the player
    Vector3f Pos(0.0f, 1.6f, -5.0f);  // Position of player

    // MAIN LOOP
    // =========
    int appClock = 0;

    while (!(DX11.Key['Q'] && DX11.Key[VK_CONTROL]) && !DX11.Key[VK_ESCAPE]) {
        ++appClock;

        MSG msg;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        const float speed = 1.0f;  // Can adjust the movement speed.
        ovrVector3f useHmdToEyeViewOffset[2] = {eyeRenderDesc[0].HmdToEyeViewOffset,
                                                eyeRenderDesc[1].HmdToEyeViewOffset};

        ovrHmd_BeginFrame(hmd.get(), 0);

        // Recenter the Rift by pressing 'R'
        if (DX11.Key['R']) ovrHmd_RecenterPose(hmd.get());

        // Dismiss the Health and Safety message by pressing any key
        if (DX11.IsAnyKeyPressed()) ovrHmd_DismissHSWDisplay(hmd.get());

        // Keyboard inputs to adjust player orientation
        if (DX11.Key[VK_LEFT]) Yaw += 0.02f;
        if (DX11.Key[VK_RIGHT]) Yaw -= 0.02f;

        // Keyboard inputs to adjust player position
        if (DX11.Key['W'] || DX11.Key[VK_UP])
            Pos += Matrix4f::RotationY(Yaw).Transform(Vector3f(0, 0, -speed * 0.05f));
        if (DX11.Key['S'] || DX11.Key[VK_DOWN])
            Pos += Matrix4f::RotationY(Yaw).Transform(Vector3f(0, 0, +speed * 0.05f));
        if (DX11.Key['D'])
            Pos += Matrix4f::RotationY(Yaw).Transform(Vector3f(+speed * 0.05f, 0, 0));
        if (DX11.Key['A'])
            Pos += Matrix4f::RotationY(Yaw).Transform(Vector3f(-speed * 0.05f, 0, 0));
        Pos.y = ovrHmd_GetFloat(hmd.get(), OVR_KEY_EYE_HEIGHT, Pos.y);

        // Animate the cube
        roomScene.Models[0]->Pos =
            Vector3f(9 * sin(0.01f * appClock), 3, 9 * cos(0.01f * appClock));

        // Get both eye poses simultaneously, with IPD offset already included.
        ovrPosef eyePoses[2] = {};
        ovrHmd_GetEyePoses(hmd.get(), 0, useHmdToEyeViewOffset, eyePoses, nullptr);

        // Render the two undistorted eye views into their render buffers.
        for (int eye = 0; eye < 2; ++eye) {
            EyeTarget& useTarget = eyeTargets[eye];
            ovrPosef& useEyePose = eyePoses[eye];

            DX11.ClearAndSetEyeTarget(useTarget);
            // Get view and projection matrices (note near Z to reduce eye strain)
            Matrix4f rollPitchYaw = Matrix4f::RotationY(Yaw);
            Matrix4f finalRollPitchYaw = rollPitchYaw * Matrix4f(useEyePose.Orientation);
            Vector3f finalUp = finalRollPitchYaw.Transform(Vector3f(0, 1, 0));
            Vector3f finalForward = finalRollPitchYaw.Transform(Vector3f(0, 0, -1));
            Vector3f shiftedEyePos = Pos + rollPitchYaw.Transform(useEyePose.Position);

            Matrix4f view =
                Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp);
            Matrix4f proj = ovrMatrix4f_Projection(eyeRenderDesc[eye].Fov, 0.2f, 1000.0f, true);

            // Render the scene
            roomScene.Render(DX11, view, proj.Transposed());
        }

        // Do distortion rendering, Present and flush/sync
        [&eyeTargets, &eyePoses, &hmd] {
            ovrD3D11Texture eyeTexture[2];
            for (int eye = 0; eye < 2; eye++) {
                eyeTexture[eye].D3D11.Header.API = ovrRenderAPI_D3D11;
                eyeTexture[eye].D3D11.Header.TextureSize = eyeTargets[eye].size;
                eyeTexture[eye].D3D11.Header.RenderViewport = eyeTargets[eye].viewport;
                eyeTexture[eye].D3D11.pTexture = eyeTargets[eye].tex;
                eyeTexture[eye].D3D11.pSRView = eyeTargets[eye].srv;
            }
            ovrHmd_EndFrame(hmd.get(), eyePoses, &eyeTexture[0].Texture);
        }();
    }

    return 0;
}
