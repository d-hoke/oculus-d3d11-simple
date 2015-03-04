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
#include "OVR_CAPI.h"           // Include the OculusVR SDK

#define OVR_D3D_VERSION 11
#include "OVR_CAPI_D3D.h"  // Include SDK-rendered code for the D3D version

#include "libovrwrapper.h"

#include <algorithm>
#include <cassert>
#include <regex>
#include <string>

using namespace libovrwrapper;

using namespace std;

//-------------------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR /*args*/, int) {
    Ovr ovr;
    Hmd hmd(ovr.CreateHmd());

    Recti windowRect = Recti(hmd.getWindowsPos(), hmd.getResolution());
    DirectX11 DX11(hinst, windowRect);

    hmd.attachToWindow(DX11.Window);
    hmd.setCap(ovrHmdCap_LowPersistence);
    hmd.setCap(ovrHmdCap_DynamicPrediction);

    // Start the sensor which informs of the Rift's pose and motion
    hmd.configureTracking(ovrTrackingCap_Orientation | ovrTrackingCap_MagYawCorrection |
                          ovrTrackingCap_Position);

    // Make the eye render buffers (caution if actual size < requested due to HW limits).
    EyeTarget eyeTargets[] = {{DX11.Device, hmd.getFovTextureSize(ovrEye_Left)},
                               {DX11.Device, hmd.getFovTextureSize(ovrEye_Right)}};

    // Setup VR components
    ovrD3D11Config d3d11cfg;
    d3d11cfg.D3D11.Header.API = ovrRenderAPI_D3D11;
    d3d11cfg.D3D11.Header.BackBufferSize = hmd.getResolution();
    d3d11cfg.D3D11.Header.Multisample = 1;
    d3d11cfg.D3D11.pDevice = DX11.Device;
    d3d11cfg.D3D11.pDeviceContext = DX11.Context;
    d3d11cfg.D3D11.pBackBufferRT = DX11.BackBufferRT;
    d3d11cfg.D3D11.pSwapChain = DX11.SwapChain;

    auto EyeRenderDesc = hmd.configureRendering(
        &d3d11cfg.Config, ovrDistortionCap_Chromatic | ovrDistortionCap_Vignette |
                              ovrDistortionCap_TimeWarp | ovrDistortionCap_Overdrive);

    {
        // Create the room model
        Scene roomScene(DX11.Device, DX11.Context,
                        false);  // Can simplify scene further with parameter if required.

        float Yaw(3.141592f);             // Horizontal rotation of the player
        Vector3f Pos(0.0f, 1.6f, -5.0f);  // Position of player

        // MAIN LOOP
        // =========
        int appClock = 0;

        while (!(DX11.Key['Q'] && DX11.Key[VK_CONTROL]) && !DX11.Key[VK_ESCAPE]) {
            ++appClock;

            DX11.HandleMessages();

            const float speed = 1.0f;    // Can adjust the movement speed.
            int timesToRenderScene = 1;  // Can adjust the render burden on the app.
            ovrVector3f useHmdToEyeViewOffset[2] = {EyeRenderDesc[0].HmdToEyeViewOffset,
                                                    EyeRenderDesc[1].HmdToEyeViewOffset};

            hmd.beginFrame(0);

            // Handle key toggles for re-centering, meshes, FOV, etc.
            // Recenter the Rift by pressing 'R'
            if (DX11.Key['R']) hmd.recenterPose();

            // Dismiss the Health and Safety message by pressing any key
            if (DX11.IsAnyKeyPressed()) hmd.dismissHSWDisplay();

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
            Pos.y = hmd.getProperty(OVR_KEY_EYE_HEIGHT, Pos.y);

            // Animate the cube
            if (speed)
                roomScene.Models[0]->Pos =
                    Vector3f(9 * sin(0.01f * appClock), 3, 9 * cos(0.01f * appClock));

            // Get both eye poses simultaneously, with IPD offset already included.
            auto eyePoses = hmd.getEyePoses(0, useHmdToEyeViewOffset);

            // Render the two undistorted eye views into their render buffers.
            for (int eye = 0; eye < 2; eye++) {
                EyeTarget& useTarget = eyeTargets[eye];
                ovrPosef& useEyePose = eyePoses.first[eye];
                bool clearEyeImage = true;
                bool updateEyeImage = true;

                if (clearEyeImage)
                    DX11.ClearAndSetRenderTarget(useTarget.rtv, useTarget.dsv,
                                                 Recti(useTarget.viewport));
                if (updateEyeImage) {
                    // Get view and projection matrices (note near Z to reduce eye strain)
                    Matrix4f rollPitchYaw = Matrix4f::RotationY(Yaw);
                    Matrix4f finalRollPitchYaw = rollPitchYaw * Matrix4f(useEyePose.Orientation);
                    Vector3f finalUp = finalRollPitchYaw.Transform(Vector3f(0, 1, 0));
                    Vector3f finalForward = finalRollPitchYaw.Transform(Vector3f(0, 0, -1));
                    Vector3f shiftedEyePos = Pos + rollPitchYaw.Transform(useEyePose.Position);

                    Matrix4f view =
                        Matrix4f::LookAtRH(shiftedEyePos, shiftedEyePos + finalForward, finalUp);
                    Matrix4f proj =
                        ovrMatrix4f_Projection(EyeRenderDesc[eye].Fov, 0.2f, 1000.0f, true);

                    // Render the scene
                    for (int t = 0; t < timesToRenderScene; t++)
                        roomScene.Render(DX11, view, proj.Transposed());
                }
            }

            // Do distortion rendering, Present and flush/sync
            ovrD3D11Texture eyeTexture[2];  // Gather data for eye textures
            for (int eye = 0; eye < 2; eye++) {
                eyeTexture[eye].D3D11.Header.API = ovrRenderAPI_D3D11;
                eyeTexture[eye].D3D11.Header.TextureSize = eyeTargets[eye].size;
                eyeTexture[eye].D3D11.Header.RenderViewport = eyeTargets[eye].viewport;
                eyeTexture[eye].D3D11.pTexture = eyeTargets[eye].tex;
                eyeTexture[eye].D3D11.pSRView = eyeTargets[eye].srv;
            }
            hmd.endFrame(eyePoses.first.data(), &eyeTexture[0].Texture);
        }
    }

    hmd.shutdownRendering();
    return 0;
}
