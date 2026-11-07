// [TEMP] [CHOSEN, not recovered] Submission failure uses the production error latch.
extern "C" void metalTestDrawableAvailable(void*, bool);

static int commandFailureChecks(IDirect3DDevice9* device, IDirect3DSwapChain9* chain,
    void* window, D3DPRESENT_PARAMETERS& pp)
{
    const auto alphaBlendEnable = static_cast<D3DRENDERSTATETYPE>(27);
    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        CHECK(device->TestCooperativeLevel() == D3D_OK);
        CHECK(device->SetRenderState(alphaBlendEnable,TRUE) == D3D_OK);
        IDirect3DSurface9* back = nullptr;
        CHECK(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back) == D3D_OK);
        CHECK(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff204060,1,0) == D3D_OK);
        bf2metal::injectCommandBufferFailure(device);
        CHECK((attempt ? chain->Present(nullptr,nullptr,nullptr,nullptr,0) :
            device->Present(nullptr,nullptr,nullptr,nullptr)) == D3DERR_DEVICELOST);
        CHECK(device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET);
        CHECK(device->Present(nullptr,nullptr,nullptr,nullptr) == D3DERR_DEVICELOST);
        CHECK(chain->Present(nullptr,nullptr,nullptr,nullptr,0) == D3DERR_DEVICELOST);
        CHECK(device->Reset(&pp) == D3DERR_INVALIDCALL); // same reference rule as resize
        CHECK(device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET);
        CHECK(back->Release() == 0);

        if (attempt) {
            metalTestDrawableAvailable(window,false);
            CHECK(device->TestCooperativeLevel() == D3DERR_DEVICELOST);
            CHECK(device->Present(nullptr,nullptr,nullptr,nullptr) == D3DERR_DEVICELOST);
            CHECK(device->Reset(&pp) == D3DERR_DEVICELOST); // explicit size cannot bypass this
            metalTestDrawableAvailable(window,true);
            CHECK(device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET);
        }
        D3DPRESENT_PARAMETERS invalid = pp;
        invalid.MultiSampleType = D3DMULTISAMPLE_NONMASKABLE; // 2..16 samples are accepted (MSAA)
        CHECK(device->Reset(&invalid) == D3DERR_NOTAVAILABLE);
        CHECK(device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET);
        invalid = pp;
        invalid.BackBufferWidth = 8193; // bridge target-size refusal must retain the latch
        CHECK(device->Reset(&invalid) == D3DERR_DEVICELOST);
        CHECK(device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET);
        CHECK(device->Reset(&pp) == D3D_OK);
        CHECK(device->TestCooperativeLevel() == D3D_OK);
        DWORD blend = TRUE;
        CHECK(device->GetRenderState(alphaBlendEnable,&blend) == D3D_OK && !blend);
        D3DVIEWPORT9 viewport = {};
        CHECK(device->GetViewport(&viewport) == D3D_OK &&
            viewport.Width == pp.BackBufferWidth && viewport.Height == pp.BackBufferHeight);
        CHECK(device->BeginScene() == D3D_OK);
        CHECK(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
            0xff406080,1,0) == D3D_OK);
        CHECK(device->EndScene() == D3D_OK);
        CHECK(device->Present(nullptr,nullptr,nullptr,nullptr) == D3D_OK);
        CHECK(chain->Present(nullptr,nullptr,nullptr,nullptr,0) == D3D_OK);
    }
    std::puts("PASS: injected command errors, lost/not-reset, refused resets, recovery and present");
    return 0;
}
