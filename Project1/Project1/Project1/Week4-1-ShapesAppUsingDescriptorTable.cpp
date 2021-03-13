//***************************************************************************************
// ShapesApp.cpp 
//
// Hold down '1' key to view scene in wireframe mode.
//***************************************************************************************

#include "../../Common/d3dApp.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "../../Common/GeometryGenerator.h"
#include "FrameResource.h"
#include "Waves.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

const int gNumFrameResources = 3;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

    XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

    Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
    Opaque = 0,
    //step1
    Transparent,
    Count
};

class ShapesApp : public D3DApp
{
public:
    ShapesApp(HINSTANCE hInstance);
    ShapesApp(const ShapesApp& rhs) = delete;
    ShapesApp& operator=(const ShapesApp& rhs) = delete;
    ~ShapesApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
    void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
    void UpdateWaves(const GameTimer& gt);

    void LoadTextures();
    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildConstantBufferViews();
    void BuildLandGeometry();
    void BuildWavesGeometry();
    void BuildShapeGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

    void createShapeInWorld(UINT& objIndex, XMFLOAT3 scaling, XMFLOAT3 translation,float angle, std::string shapeName, std::string materialName);
    void createShapeInWorld(UINT& objIndex, XMFLOAT3 scaling, XMFLOAT3 translation, XMFLOAT3 angle, std::string shapeName, std::string materialName);
    void createTower(UINT& objIndex, XMFLOAT3 location);
    void createWall(UINT& objIndex, XMFLOAT3 location, float rotation, int sign);
    void createMainGate(UINT& objIndex, XMFLOAT3 location);
    void createCastlePerimeter(UINT& objIndex);
    void createTowerDoors(UINT& objIndex);
    void createCastleTowers(UINT& objIndex);
    void createMainCastle(UINT& objIndex);

    void createTorch(UINT& objIndex);

    

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

    float GetHillsHeight(float x, float z)const;
    XMFLOAT3 GetHillsNormal(float x, float z)const;
 
private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
    //ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
    std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
    std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

    RenderItem* mWavesRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

    std::unique_ptr<Waves> mWaves;

    PassConstants mMainPassCB;

    UINT mPassCbvOffset = 0;

    bool mIsWireframe = false;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f*XM_PI;
    float mPhi = 0.2f*XM_PI;
    float mRadius = 15.0f;
    XMVECTOR target = XMVectorZero();
    POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        ShapesApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

ShapesApp::ShapesApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

ShapesApp::~ShapesApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool ShapesApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

    LoadTextures();
    BuildRootSignature();
    BuildDescriptorHeaps();
    BuildShadersAndInputLayout();
    BuildLandGeometry();
    BuildWavesGeometry();
    BuildShapeGeometry();
    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    //BuildConstantBufferViews();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}
 
void ShapesApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void ShapesApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
	UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    AnimateMaterials(gt);
	UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
    UpdateWaves(gt);
}

void ShapesApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    if(mIsWireframe)
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
    }
    else
    {
        ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
    }

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Black, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
    mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

   // int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
   // auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
   // passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
   // mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

    auto passCB = mCurrFrameResource->PassCB->Resource();
    mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;
    
    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        //mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.2 unit in the scene.
        float dx = 0.05f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.05f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        //mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
 
void ShapesApp::OnKeyboardInput(const GameTimer& gt)
{
    if(GetAsyncKeyState('1') & 0x8000)
        mIsWireframe = true;
    else
        mIsWireframe = false;
}
 
void ShapesApp::UpdateCamera(const GameTimer& gt)
{
    XMVECTOR right = XMVectorSet(0.01f, 0.0f, 0.0f, 1.0f);
    XMVECTOR upward = XMVectorSet(0.00f, 0.01f, 0.0f, 1.0f);
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius*sinf(mPhi)*cosf(mTheta);
	mEyePos.z = mRadius*sinf(mPhi)*sinf(mTheta);
	mEyePos.y = mRadius*cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    //Move Right and Left

    
	XMStoreFloat4x4(&mView, view);
}

void ShapesApp::AnimateMaterials(const GameTimer& gt)
{
    // Scroll the water material texture coordinates.
    auto waterMat = mMaterials["water"].get();

    float& tu = waterMat->MatTransform(3, 0);
    float& tv = waterMat->MatTransform(3, 1);

    tu += 0.1f * gt.DeltaTime();
    tv += 0.02f * gt.DeltaTime();

    if (tu >= 1.0f)
        tu -= 1.0f;

    if (tv >= 1.0f)
        tv -= 1.0f;

    waterMat->MatTransform(3, 0) = tu;
    waterMat->MatTransform(3, 1) = tv;

    // Material has changed, so need to update cbuffer.
    waterMat->NumFramesDirty = gNumFrameResources;
}

void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
            XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
            XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void ShapesApp::UpdateMaterialCBs(const GameTimer& gt)
{
    auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
    for (auto& e : mMaterials)
    {
        // Only update the cbuffer data if the constants have changed.  If the cbuffer
        // data changes, it needs to be updated for each FrameResource.
        Material* mat = e.second.get();
        if (mat->NumFramesDirty > 0)
        {
            XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

            MaterialConstants matConstants;
            matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
            matConstants.FresnelR0 = mat->FresnelR0;
            matConstants.Roughness = mat->Roughness;
            XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

            currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

            // Next FrameResource need to be updated too.
            mat->NumFramesDirty--;
        }
    }
}

void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

    mMainPassCB.AmbientLight = { 0.15f, 0.15f, 0.15f, 1.0f };
    mMainPassCB.Lights[0].Direction = { -0.5, -0.5, 3.0 };
    mMainPassCB.Lights[0].Strength = { 0.1,0.1,0.1};

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void ShapesApp::UpdateWaves(const GameTimer& gt)
{
    // Every quarter second, generate a random wave.
    static float t_base = 0.0f;
    if ((mTimer.TotalTime() - t_base) >= 0.25f)
    {
        t_base += 0.25f;

        int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
        int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

        float r = MathHelper::RandF(0.2f, 0.5f);

        mWaves->Disturb(i, j, r);
    }

    // Update the wave simulation.
    mWaves->Update(gt.DeltaTime());

    // Update the wave vertex buffer with the new solution.
    auto currWavesVB = mCurrFrameResource->WavesVB.get();
    for (int i = 0; i < mWaves->VertexCount(); ++i)
    {
        Vertex v;

        v.Pos = mWaves->Position(i);
        v.Normal = mWaves->Normal(i);

        // Derive tex-coords from position by 
        // mapping [-w/2,w/2] --> [0,1]
        v.TexC.x = 0.5f + v.Pos.x / mWaves->Width();
        v.TexC.y = 0.5f - v.Pos.z / mWaves->Depth();

        currWavesVB->CopyData(i, v);
    }

    // Set the dynamic VB of the wave renderitem to the current frame VB.
    mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

void ShapesApp::LoadTextures()
{
    auto grassTex = std::make_unique<Texture>();
    grassTex->Name = "grassTex";
    grassTex->Filename = L"../../Textures/grass.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), grassTex->Filename.c_str(),
        grassTex->Resource, grassTex->UploadHeap));

    auto sandbrickTex = std::make_unique<Texture>();
    sandbrickTex->Name = "sandbrickTex";
    sandbrickTex->Filename = L"../../Textures/sandbrick.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), sandbrickTex->Filename.c_str(),
        sandbrickTex->Resource, sandbrickTex->UploadHeap));

    auto ironTex = std::make_unique<Texture>();
    ironTex->Name = "ironTex";
    ironTex->Filename = L"../../Textures/iron.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), ironTex->Filename.c_str(),
        ironTex->Resource, ironTex->UploadHeap));

    auto shingleTex = std::make_unique<Texture>();
    shingleTex->Name = "shingleTex";
    shingleTex->Filename = L"../../Textures/shingle.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), shingleTex->Filename.c_str(),
        shingleTex->Resource, shingleTex->UploadHeap));

    auto stoneTex = std::make_unique<Texture>();
    stoneTex->Name = "stoneTex";
    stoneTex->Filename = L"../../Textures/stone.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), stoneTex->Filename.c_str(),
        stoneTex->Resource, stoneTex->UploadHeap));

    auto stonebrickTex = std::make_unique<Texture>();
    stonebrickTex->Name = "stonebrickTex";
    stonebrickTex->Filename = L"../../Textures/stonebrick.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), stonebrickTex->Filename.c_str(),
        stonebrickTex->Resource, stonebrickTex->UploadHeap));
        
    auto woodVTex = std::make_unique<Texture>();
    woodVTex->Name = "woodVTex";
    woodVTex->Filename = L"../../Textures/woodV.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), woodVTex->Filename.c_str(),
        woodVTex->Resource, woodVTex->UploadHeap));

    auto waterTex = std::make_unique<Texture>();
    waterTex->Name = "waterTex";
    waterTex->Filename = L"../../Textures/water1.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), waterTex->Filename.c_str(),
        waterTex->Resource, waterTex->UploadHeap));

    auto wroughtIronTex = std::make_unique<Texture>();
    wroughtIronTex->Name = "wroughtIronTex";
    wroughtIronTex->Filename = L"../../Textures/wroughtIronTexture.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), wroughtIronTex->Filename.c_str(),
        wroughtIronTex->Resource, wroughtIronTex->UploadHeap));

    auto flameTex = std::make_unique<Texture>();
    flameTex->Name = "flameTex";
    flameTex->Filename = L"../../Textures/FlameTexture.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), flameTex->Filename.c_str(),
        flameTex->Resource, flameTex->UploadHeap));

    //auto crystalTex = std::make_unique<Texture>();
    //crystalTex->Name = "crystalTex";
    //crystalTex->Filename = L"../../Textures/crystal.dds";
    //ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
    //    mCommandList.Get(), crystalTex->Filename.c_str(),
    //    crystalTex->Resource, crystalTex->UploadHeap));

    mTextures[grassTex->Name] = std::move(grassTex);
    mTextures[sandbrickTex->Name] = std::move(sandbrickTex);
    mTextures[ironTex->Name] = std::move(ironTex);
    mTextures[shingleTex->Name] = std::move(shingleTex);
    mTextures[stoneTex->Name] = std::move(stoneTex);
    mTextures[stonebrickTex->Name] = std::move(stonebrickTex);
    mTextures[woodVTex->Name] = std::move(woodVTex);
    mTextures[waterTex->Name] = std::move(waterTex);
    mTextures[wroughtIronTex->Name] = std::move(wroughtIronTex);
    mTextures[flameTex->Name] = std::move(flameTex);
    //mTextures[crystalTex->Name] = std::move(crystalTex);
}
//If we have 3 frame resources and n render items, then we have three 3n object constant
//buffers and 3 pass constant buffers.Hence we need 3(n + 1) constant buffer views(CBVs).
//Thus we will need to modify our CBV heap to include the additional descriptors :


void ShapesApp::BuildDescriptorHeaps()
{
   // UINT objCount = (UINT)mOpaqueRitems.size();
   //
   // // Need a CBV descriptor for each object for each frame resource,
   // // +1 for the perPass CBV for each frame resource.
   // UINT numDescriptors = (objCount+1) * gNumFrameResources;
   //
   // // Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
   // mPassCbvOffset = objCount * gNumFrameResources;
   //
   // D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
   // cbvHeapDesc.NumDescriptors = numDescriptors;
   // cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
   // cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
   // cbvHeapDesc.NodeMask = 0;
   // ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
   //     IID_PPV_ARGS(&mCbvHeap)));

    //
    // Create the SRV heap.
    //
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 10;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

    //
    // Fill out the heap with actual descriptors.
    //
    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

    auto grassTex = mTextures["grassTex"]->Resource;
    auto sandbrickTex = mTextures["sandbrickTex"]->Resource;
    auto ironTex = mTextures["ironTex"]->Resource;
    auto shingleTex = mTextures["shingleTex"]->Resource;
    auto stoneTex = mTextures["stoneTex"]->Resource;
    auto stonebrickTex = mTextures["stonebrickTex"]->Resource;
    auto woodVTex = mTextures["woodVTex"]->Resource;
    auto waterTex = mTextures["waterTex"]->Resource;
    auto wroughtIronTex = mTextures["wroughtIronTex"]->Resource;
    auto flameTex = mTextures["flameTex"]->Resource;
    //auto crystalTex = mTextures["crystalTex"]->Resource;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    srvDesc.Format = grassTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = -1;
    md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    srvDesc.Format = sandbrickTex->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(sandbrickTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    srvDesc.Format = ironTex->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(ironTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    srvDesc.Format = shingleTex->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(shingleTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    srvDesc.Format = stoneTex->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(stoneTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    srvDesc.Format = stonebrickTex->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(stonebrickTex.Get(), &srvDesc, hDescriptor);

    //// next descriptor
    //hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    //srvDesc.Format = crystalTex->GetDesc().Format;
    //md3dDevice->CreateShaderResourceView(crystalTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    srvDesc.Format = woodVTex->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(woodVTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    srvDesc.Format = waterTex->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    srvDesc.Format = wroughtIronTex->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(wroughtIronTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    srvDesc.Format = flameTex->GetDesc().Format;
    md3dDevice->CreateShaderResourceView(flameTex.Get(), &srvDesc, hDescriptor);
}

//assuming we have n renter items, we can populate the CBV heap with the following code where descriptors 0 to n-
//1 contain the object CBVs for the 0th frame resource, descriptors n to 2n−1 contains the
//object CBVs for 1st frame resource, descriptors 2n to 3n−1 contain the objects CBVs for
//the 2nd frame resource, and descriptors 3n, 3n + 1, and 3n + 2 contain the pass CBVs for the
//0th, 1st, and 2nd frame resource
//void ShapesApp::BuildConstantBufferViews()
//{
//    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
//
//    UINT objCount = (UINT)mOpaqueRitems.size();
//
//    // Need a CBV descriptor for each object for each frame resource.
//    for(int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
//    {
//        auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
//        for(UINT i = 0; i < objCount; ++i)
//        {
//            D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();
//
//            // Offset to the ith object constant buffer in the buffer.
//            cbAddress += i*objCBByteSize;
//
//            // Offset to the object cbv in the descriptor heap.
//            int heapIndex = frameIndex*objCount + i;
//
//			//we can get a handle to the first descriptor in a heap with the ID3D12DescriptorHeap::GetCPUDescriptorHandleForHeapStart
//            auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
//
//			//our heap has more than one descriptor,we need to know the size to increment in the heap to get to the next descriptor
//			//This is hardware specific, so we have to query this information from the device, and it depends on
//			//the heap type.Recall that our D3DApp class caches this information: 	mCbvSrvUavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
//            handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);
//
//            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
//            cbvDesc.BufferLocation = cbAddress;
//            cbvDesc.SizeInBytes = objCBByteSize;
//
//            md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
//        }
//    }
//
//    UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));
//
//    // Last three descriptors are the pass CBVs for each frame resource.
//    for(int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
//    {
//        auto passCB = mFrameResources[frameIndex]->PassCB->Resource();
//        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();
//
//        // Offset to the pass cbv in the descriptor heap.
//        int heapIndex = mPassCbvOffset + frameIndex;
//        auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
//        handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);
//
//        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
//        cbvDesc.BufferLocation = cbAddress;
//        cbvDesc.SizeInBytes = passCBByteSize;
//        
//        md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
//    }
//}

//A root signature defines what resources need to be bound to the pipeline before issuing a draw call and
//how those resources get mapped to shader input registers. there is a limit of 64 DWORDs that can be put in a root signature.
void ShapesApp::BuildRootSignature()
{
    //CD3DX12_DESCRIPTOR_RANGE cbvTable0;
    //cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    //
    //CD3DX12_DESCRIPTOR_RANGE cbvTable1;
    //cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
    //
	//// Root parameter can be a table, root descriptor or root constants.
	//CD3DX12_ROOT_PARAMETER slotRootParameter[2];
    //
	//// Create root CBVs.
    //slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
    //slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);
    //
	//// A root signature is an array of root parameters.
	//CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr, 
    //    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    //
	//// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	//ComPtr<ID3DBlob> serializedRootSig = nullptr;
	//ComPtr<ID3DBlob> errorBlob = nullptr;
	//HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
	//	serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
    //
	//if(errorBlob != nullptr)
	//{
	//	::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	//}
	//ThrowIfFailed(hr);
    //
	//ThrowIfFailed(md3dDevice->CreateRootSignature(
	//	0,
	//	serializedRootSig->GetBufferPointer(),
	//	serializedRootSig->GetBufferSize(),
	//	IID_PPV_ARGS(mRootSignature.GetAddressOf())));

    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    // Perfomance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

    auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
        (UINT)staticSamplers.size(), staticSamplers.data(),
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

void ShapesApp::BuildShadersAndInputLayout()
{
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_1");
	
    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

void ShapesApp::BuildLandGeometry()
{
    GeometryGenerator geoGen;
    GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

    //
    // Extract the vertex elements we are interested and apply the height function to
    // each vertex.  In addition, color the vertices based on their height so we have
    // sandy looking beaches, grassy low hills, and snow mountain peaks.
    //

    std::vector<Vertex> vertices(grid.Vertices.size());
    for (size_t i = 0; i < grid.Vertices.size(); ++i)
    {
        auto& p = grid.Vertices[i].Position;
        vertices[i].Pos = p;
        vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
        vertices[i].Normal = GetHillsNormal(p.x, p.z);
        vertices[i].TexC = grid.Vertices[i].TexC;
    }

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

    std::vector<std::uint16_t> indices = grid.GetIndices16();
    const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "landGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
    CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["grid"] = submesh;

    mGeometries["landGeo"] = std::move(geo);
}

void ShapesApp::BuildWavesGeometry()
{
    std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
    assert(mWaves->VertexCount() < 0x0000ffff);

    // Iterate over each quad.
    int m = mWaves->RowCount();
    int n = mWaves->ColumnCount();
    int k = 0;
    for (int i = 0; i < m - 1; ++i)
    {
        for (int j = 0; j < n - 1; ++j)
        {
            indices[k] = i * n + j;
            indices[k + 1] = i * n + j + 1;
            indices[k + 2] = (i + 1) * n + j;

            indices[k + 3] = (i + 1) * n + j;
            indices[k + 4] = i * n + j + 1;
            indices[k + 5] = (i + 1) * n + j + 1;

            k += 6; // next quad
        }
    }

    UINT vbByteSize = mWaves->VertexCount() * sizeof(Vertex);
    UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

    auto geo = std::make_unique<MeshGeometry>();
    geo->Name = "waterGeo";

    // Set dynamically.
    geo->VertexBufferCPU = nullptr;
    geo->VertexBufferGPU = nullptr;

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
    CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
        mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

    geo->VertexByteStride = sizeof(Vertex);
    geo->VertexBufferByteSize = vbByteSize;
    geo->IndexFormat = DXGI_FORMAT_R16_UINT;
    geo->IndexBufferByteSize = ibByteSize;

    SubmeshGeometry submesh;
    submesh.IndexCount = (UINT)indices.size();
    submesh.StartIndexLocation = 0;
    submesh.BaseVertexLocation = 0;

    geo->DrawArgs["grid"] = submesh;

    mGeometries["waterGeo"] = std::move(geo);
}

void ShapesApp::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3, 0.5, 0.5);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.05f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(1.0f,0.75f,1.0f,4,20);
    GeometryGenerator::MeshData cone = geoGen.CreateCone(1.0f, 1.0f, 20, 20);
    GeometryGenerator::MeshData pyramid = geoGen.CreatePyramid(1.0f, 1.0f, 20);
    GeometryGenerator::MeshData triprism = geoGen.CreateTriangularPrism(1.0f, 0.1f,1.0f, 20);
    GeometryGenerator::MeshData diamond = geoGen.CreateDiamond(3.0f, 4.0f, 4, 20);
    GeometryGenerator::MeshData wedge = geoGen.CreateWedge(1.0f, 1.0f, 1.0f, 3);
    GeometryGenerator::MeshData torus = geoGen.CreateTorus(1.0f, 0.25f, 10, 12);
    GeometryGenerator::MeshData grayBox = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3, 0.25, 1);
    GeometryGenerator::MeshData fireBox = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3, 1, 1);
    GeometryGenerator::MeshData blackCylinder = geoGen.CreateCylinder(1.0f, 1.0f, 1.0f, 14, 20);
    GeometryGenerator::MeshData roundcylinder = geoGen.CreateCylinder(1.0f, 1.0f, 1.0f, 20, 20);
    GeometryGenerator::MeshData torchHandle = geoGen.CreateCone(0.25f, 0.75f, 20, 20);
    
    


	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
    UINT coneVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size();
    UINT pyramidVertexOffset = coneVertexOffset + (UINT)cone.Vertices.size();
    UINT triprismVertexOffset = pyramidVertexOffset + (UINT)pyramid.Vertices.size();
    UINT diamondVertexOffset = triprismVertexOffset + (UINT)triprism.Vertices.size();
    UINT wedgeVertexOffset = diamondVertexOffset + (UINT)diamond.Vertices.size();
    UINT torusVertexOffset = wedgeVertexOffset + (UINT)wedge.Vertices.size();
    UINT grayBoxVertexOffset = torusVertexOffset + (UINT)torus.Vertices.size();
    UINT brownBoxVertexOffset = grayBoxVertexOffset + (UINT)grayBox.Vertices.size();
    UINT blackCylinderVertexOffset = brownBoxVertexOffset + (UINT)fireBox.Vertices.size();
    UINT roundCylinderVertexOffset = blackCylinderVertexOffset + (UINT)blackCylinder.Vertices.size();
    UINT torchHandleVertexOffset = roundCylinderVertexOffset + (UINT)roundcylinder.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
    UINT coneIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size();
    UINT pyramidIndexOffset = coneIndexOffset + (UINT)cone.Indices32.size();
    UINT triprismIndexOffset = pyramidIndexOffset + (UINT)pyramid.Indices32.size();
    UINT diamondIndexOffset = triprismIndexOffset + (UINT)triprism.Indices32.size();
    UINT wedgeIndexOffset = diamondIndexOffset + (UINT)diamond.Indices32.size();
    UINT torusIndexOffset = wedgeIndexOffset + (UINT)wedge.Indices32.size();
    UINT grayBoxIndexOffset = torusIndexOffset + (UINT)torus.Indices32.size();
    UINT brownBoxIndexOffset = grayBoxIndexOffset + (UINT)grayBox.Indices32.size();
    UINT blackCylinderIndexOffset = brownBoxIndexOffset + (UINT)fireBox.Indices32.size();
    UINT roundCylinderIndexOffset = blackCylinderIndexOffset + (UINT)blackCylinder.Indices32.size();
    UINT torchHandleIndexOffset = roundCylinderIndexOffset + (UINT)roundcylinder.Indices32.size();

    // Define the SubmeshGeometry that cover different 
    // regions of the vertex/index buffers.

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

    SubmeshGeometry coneSubmesh;
    coneSubmesh.IndexCount = (UINT)cone.Indices32.size();
    coneSubmesh.StartIndexLocation = coneIndexOffset;
    coneSubmesh.BaseVertexLocation = coneVertexOffset;

    SubmeshGeometry pyramidSubmesh;
    pyramidSubmesh.IndexCount = (UINT)pyramid.Indices32.size();
    pyramidSubmesh.StartIndexLocation = pyramidIndexOffset;
    pyramidSubmesh.BaseVertexLocation = pyramidVertexOffset;

    SubmeshGeometry triprismSubmesh;
    triprismSubmesh.IndexCount = (UINT)triprism.Indices32.size();
    triprismSubmesh.StartIndexLocation = triprismIndexOffset;
    triprismSubmesh.BaseVertexLocation = triprismVertexOffset;

    SubmeshGeometry diamondSubmesh;
    diamondSubmesh.IndexCount = (UINT)diamond.Indices32.size();
    diamondSubmesh.StartIndexLocation = diamondIndexOffset;
    diamondSubmesh.BaseVertexLocation = diamondVertexOffset;

    SubmeshGeometry wedgeSubmesh;
    wedgeSubmesh.IndexCount = (UINT)wedge.Indices32.size();
    wedgeSubmesh.StartIndexLocation = wedgeIndexOffset;
    wedgeSubmesh.BaseVertexLocation = wedgeVertexOffset;

    SubmeshGeometry torusSubmesh;
    torusSubmesh.IndexCount = (UINT)torus.Indices32.size();
    torusSubmesh.StartIndexLocation = torusIndexOffset;
    torusSubmesh.BaseVertexLocation = torusVertexOffset;

    SubmeshGeometry grayBoxSubmesh;
    grayBoxSubmesh.IndexCount = (UINT)grayBox.Indices32.size();
    grayBoxSubmesh.StartIndexLocation = grayBoxIndexOffset;
    grayBoxSubmesh.BaseVertexLocation = grayBoxVertexOffset;

    SubmeshGeometry brownBoxSubmesh;
    brownBoxSubmesh.IndexCount = (UINT)fireBox.Indices32.size();
    brownBoxSubmesh.StartIndexLocation = brownBoxIndexOffset;
    brownBoxSubmesh.BaseVertexLocation = brownBoxVertexOffset;

    SubmeshGeometry blackCylinderSubmesh;
    blackCylinderSubmesh.IndexCount = (UINT)blackCylinder.Indices32.size();
    blackCylinderSubmesh.StartIndexLocation = blackCylinderIndexOffset;
    blackCylinderSubmesh.BaseVertexLocation = blackCylinderVertexOffset;

    SubmeshGeometry roundCylinderSubmesh;
    roundCylinderSubmesh.IndexCount = (UINT)roundcylinder.Indices32.size();
    roundCylinderSubmesh.StartIndexLocation = roundCylinderIndexOffset;
    roundCylinderSubmesh.BaseVertexLocation = roundCylinderVertexOffset;

    SubmeshGeometry torchHandleSubmesh;
    torchHandleSubmesh.IndexCount = (UINT)torchHandle.Indices32.size();
    torchHandleSubmesh.StartIndexLocation = torchHandleIndexOffset;
    torchHandleSubmesh.BaseVertexLocation = torchHandleVertexOffset;


	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

    auto totalVertexCount =
        box.Vertices.size() +
        grid.Vertices.size() +
        sphere.Vertices.size() +
        cylinder.Vertices.size() +
        cone.Vertices.size() +
        pyramid.Vertices.size() +
        triprism.Vertices.size() +
        diamond.Vertices.size() +
        wedge.Vertices.size() +
        torus.Vertices.size() +
        grayBox.Vertices.size() +
        fireBox.Vertices.size() +
        blackCylinder.Vertices.size() +
        roundcylinder.Vertices.size() + 
        torchHandle.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for(size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::Wheat);
        vertices[k].Normal = box.Vertices[i].Normal;
        vertices[k].TexC = box.Vertices[i].TexC;
	}

	for(size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::ForestGreen);
        vertices[k].Normal = grid.Vertices[i].Normal;
        vertices[k].TexC = grid.Vertices[i].TexC;
	}

	for(size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::Black);
        vertices[k].Normal = sphere.Vertices[i].Normal;
        vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for(size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		//vertices[k].Color = XMFLOAT4(DirectX::Colors::Wheat);
        vertices[k].Normal = cylinder.Vertices[i].Normal;
        vertices[k].TexC = cylinder.Vertices[i].TexC;
	}

    for (size_t i = 0; i < cone.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = cone.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::MidnightBlue);
        vertices[k].Normal = cone.Vertices[i].Normal;
        vertices[k].TexC = cone.Vertices[i].TexC;
    }

    for (size_t i = 0; i < pyramid.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = pyramid.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkGray);
        vertices[k].Normal = pyramid.Vertices[i].Normal;
        vertices[k].TexC = pyramid.Vertices[i].TexC;
    }

    for (size_t i = 0; i < triprism.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = triprism.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::Indigo);
        vertices[k].Normal = triprism.Vertices[i].Normal;
        vertices[k].TexC = triprism.Vertices[i].TexC;
    }

    for (size_t i = 0; i < diamond.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = diamond.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::SpringGreen);
        vertices[k].Normal = diamond.Vertices[i].Normal;
        vertices[k].TexC = diamond.Vertices[i].TexC;
    }
    for (size_t i = 0; i < wedge.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = wedge.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::SaddleBrown);
        vertices[k].Normal = wedge.Vertices[i].Normal;
        vertices[k].TexC = wedge.Vertices[i].TexC;
    }
    for (size_t i = 0; i < torus.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = torus.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::Black);
        vertices[k].Normal = torus.Vertices[i].Normal;
        vertices[k].TexC = torus.Vertices[i].TexC;
    }
    for (size_t i = 0; i < grayBox.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = grayBox.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkGray);
        vertices[k].Normal = grayBox.Vertices[i].Normal;
        vertices[k].TexC = grayBox.Vertices[i].TexC;
    }
    for (size_t i = 0; i < fireBox.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = fireBox.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::BurlyWood);
        vertices[k].Normal = fireBox.Vertices[i].Normal;
        vertices[k].TexC = fireBox.Vertices[i].TexC;
    }
    for (size_t i = 0; i < blackCylinder.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = blackCylinder.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::Black);
        vertices[k].Normal = blackCylinder.Vertices[i].Normal;
        vertices[k].TexC = blackCylinder.Vertices[i].TexC;
    }
    for (size_t i = 0; i < roundcylinder.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = roundcylinder.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::BlanchedAlmond);
        vertices[k].Normal = roundcylinder.Vertices[i].Normal;
        vertices[k].TexC = roundcylinder.Vertices[i].TexC;
    }
    for (size_t i = 0; i < torchHandle.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = torchHandle.Vertices[i].Position;
        //vertices[k].Color = XMFLOAT4(DirectX::Colors::BlanchedAlmond);
        vertices[k].Normal = torchHandle.Vertices[i].Normal;
        vertices[k].TexC = torchHandle.Vertices[i].TexC;
    }

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
    indices.insert(indices.end(), std::begin(cone.GetIndices16()), std::end(cone.GetIndices16()));
    indices.insert(indices.end(), std::begin(pyramid.GetIndices16()), std::end(pyramid.GetIndices16()));
    indices.insert(indices.end(), std::begin(triprism.GetIndices16()), std::end(triprism.GetIndices16()));
    indices.insert(indices.end(), std::begin(diamond.GetIndices16()), std::end(diamond.GetIndices16()));
    indices.insert(indices.end(), std::begin(wedge.GetIndices16()), std::end(wedge.GetIndices16()));
    indices.insert(indices.end(), std::begin(torus.GetIndices16()), std::end(torus.GetIndices16()));
    indices.insert(indices.end(), std::begin(grayBox.GetIndices16()), std::end(grayBox.GetIndices16()));
    indices.insert(indices.end(), std::begin(fireBox.GetIndices16()), std::end(fireBox.GetIndices16()));
    indices.insert(indices.end(), std::begin(blackCylinder.GetIndices16()), std::end(blackCylinder.GetIndices16()));
    indices.insert(indices.end(), std::begin(roundcylinder.GetIndices16()), std::end(roundcylinder.GetIndices16()));
    indices.insert(indices.end(), std::begin(torchHandle.GetIndices16()), std::end(torchHandle.GetIndices16()));


    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size()  * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
    geo->DrawArgs["cone"] = coneSubmesh;
    geo->DrawArgs["pyramid"] = pyramidSubmesh;
    geo->DrawArgs["triprism"] = triprismSubmesh;
    geo->DrawArgs["diamond"] = diamondSubmesh;
    geo->DrawArgs["wedge"] = wedgeSubmesh;
    geo->DrawArgs["torus"] = torusSubmesh;
    geo->DrawArgs["grayBox"] = grayBoxSubmesh;
    geo->DrawArgs["brownBox"] = brownBoxSubmesh;
    geo->DrawArgs["blackCylinder"] = blackCylinderSubmesh;
    geo->DrawArgs["roundcylinder"] = roundCylinderSubmesh;
    geo->DrawArgs["torchHandle"] = torchHandleSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void ShapesApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), 
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    //opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


    //
    // PSO for opaque wireframe objects.
    //

    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
    opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}

void ShapesApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), mWaves->VertexCount()));
    }
}

void ShapesApp::BuildMaterials()
{
    auto grass = std::make_unique<Material>();
    grass->Name = "grass";
    grass->MatCBIndex = 0;
    grass->DiffuseSrvHeapIndex = 0;
    grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    grass->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    grass->Roughness = 0.25f;

    auto sandbrick = std::make_unique<Material>();
    sandbrick->Name = "sandbrick";
    sandbrick->MatCBIndex = 1;
    sandbrick->DiffuseSrvHeapIndex = 1;
    sandbrick->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    sandbrick->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    sandbrick->Roughness = 1.0f;

    auto iron = std::make_unique<Material>();
    iron->Name = "iron";
    iron->MatCBIndex = 2;
    iron->DiffuseSrvHeapIndex = 2;
    iron->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    iron->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    iron->Roughness = 0.7f;

    auto shingle = std::make_unique<Material>();
    shingle->Name = "shingle";
    shingle->MatCBIndex = 3;
    shingle->DiffuseSrvHeapIndex = 3;
    shingle->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    shingle->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    shingle->Roughness = 1.0f;

    auto stone = std::make_unique<Material>();
    stone->Name = "stone";
    stone->MatCBIndex = 4;
    stone->DiffuseSrvHeapIndex = 4;
    stone->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    stone->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    stone->Roughness = 1.0f;

    auto stonebrick = std::make_unique<Material>();
    stonebrick->Name = "stonebrick";
    stonebrick->MatCBIndex = 5;
    stonebrick->DiffuseSrvHeapIndex = 5;
    stonebrick->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    stonebrick->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    stonebrick->Roughness = 1.0f;
    
    auto woodV = std::make_unique<Material>();
    woodV->Name = "woodV";
    woodV->MatCBIndex = 6;
    woodV->DiffuseSrvHeapIndex = 6;
    woodV->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    woodV->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    woodV->Roughness = 0.9f;

    auto water = std::make_unique<Material>();
    water->Name = "water";
    water->MatCBIndex = 7;
    water->DiffuseSrvHeapIndex = 7;
    //step 6: what happens if you change the alpha to 1.0? 100% water and no blending?
    water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.6f);
    water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
    water->Roughness = 0.0f;

    auto wroughtIron = std::make_unique<Material>();
    wroughtIron->Name = "wroughtIron";
    wroughtIron->MatCBIndex = 8;
    wroughtIron->DiffuseSrvHeapIndex = 8;
    //step 6: what happens if you change the alpha to 1.0? 100% water and no blending?
    wroughtIron->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.6f);
    wroughtIron->FresnelR0 = XMFLOAT3(0.9f, 0.9f, 0.9f);
    wroughtIron->Roughness = 1.0f;

    auto flames = std::make_unique<Material>();
    flames->Name = "flames";
    flames->MatCBIndex = 9;
    flames->DiffuseSrvHeapIndex = 9;
    //step 6: what happens if you change the alpha to 1.0? 100% water and no blending?
    flames->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.6f);
    flames->FresnelR0 = XMFLOAT3(0.3f, 0.3f, 0.3f);
    flames->Roughness = 0.2f;

    mMaterials["grass"] = std::move(grass);
    mMaterials["sandbrick"] = std::move(sandbrick);
    mMaterials["iron"] = std::move(iron);
    mMaterials["shingle"] = std::move(shingle);
    mMaterials["stone"] = std::move(stone);
    mMaterials["stonebrick"] = std::move(stonebrick);
    mMaterials["woodV"] = std::move(woodV);
    mMaterials["water"] = std::move(water);
    mMaterials["wroughtIron"] = std::move(wroughtIron);
    mMaterials["flames"] = std::move(flames);
}

void ShapesApp::createShapeInWorld(UINT& objIndex, XMFLOAT3 scaling, XMFLOAT3 translation,float angle, std::string shapeName,std::string materialName)
{
    auto temp = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&temp->World, XMMatrixScaling(scaling.x, scaling.y, scaling.z) * XMMatrixRotationY(XMConvertToRadians(angle)) * XMMatrixTranslation(translation.x, translation.y + (0.5*scaling.y), translation.z));

    temp->ObjCBIndex = objIndex++;
    temp->Geo = mGeometries["shapeGeo"].get();
    temp->Mat = mMaterials[materialName].get();
    temp->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    temp->IndexCount = temp->Geo->DrawArgs[shapeName].IndexCount;
    temp->StartIndexLocation = temp->Geo->DrawArgs[shapeName].StartIndexLocation;
    temp->BaseVertexLocation = temp->Geo->DrawArgs[shapeName].BaseVertexLocation;
    mAllRitems.push_back(std::move(temp));
}

//This function allows for rotation on all axis
void ShapesApp::createShapeInWorld(UINT& objIndex, XMFLOAT3 scaling, XMFLOAT3 translation, XMFLOAT3 angle, std::string shapeName, std::string materialName)
{
    auto temp = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&temp->World, XMMatrixScaling(scaling.x, scaling.y, scaling.z) * XMMatrixRotationRollPitchYaw(XMConvertToRadians(angle.x), 
        XMConvertToRadians(angle.y), XMConvertToRadians(angle.z))* XMMatrixTranslation(translation.x, translation.y + (0.5 * scaling.y), translation.z));

    temp->ObjCBIndex = objIndex++;
    temp->Geo = mGeometries["shapeGeo"].get();
    temp->Mat = mMaterials[materialName].get();
    temp->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    temp->IndexCount = temp->Geo->DrawArgs[shapeName].IndexCount;
    temp->StartIndexLocation = temp->Geo->DrawArgs[shapeName].StartIndexLocation;
    temp->BaseVertexLocation = temp->Geo->DrawArgs[shapeName].BaseVertexLocation;
    mAllRitems.push_back(std::move(temp));
}

void ShapesApp::BuildRenderItems()
{
	UINT objCBIndex = 0;

    //auto gridRitem = std::make_unique<RenderItem>();
    //gridRitem->World = MathHelper::Identity4x4();
    //XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
    //gridRitem->ObjCBIndex = objCBIndex++;
    //gridRitem->Mat = mMaterials["grass"].get();
    //gridRitem->Geo = mGeometries["shapeGeo"].get();
    //gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    //gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    //gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    //gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
    //mAllRitems.push_back(std::move(gridRitem));

	//auto boxRitem = std::make_unique<RenderItem>();
	//XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f)*XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	//boxRitem->ObjCBIndex = objCBIndex++;
    //boxRitem->Mat = mMaterials["grass"].get();
	//boxRitem->Geo = mGeometries["shapeGeo"].get();
	//boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	//boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	//boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	//mAllRitems.push_back(std::move(boxRitem));

    auto wavesRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&wavesRitem->World, XMMatrixTranslation(-10.0f, -1.5f, 0.0f));
    XMStoreFloat4x4(&wavesRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
    wavesRitem->ObjCBIndex = objCBIndex++;
    wavesRitem->Mat = mMaterials["water"].get();
    wavesRitem->Geo = mGeometries["waterGeo"].get();
    wavesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
    wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;

    //// we use mVavesRitem in updatewaves() to set the dynamic VB of the wave renderitem to the current frame VB.
    mWavesRitem = wavesRitem.get();

    //step5
    mAllRitems.push_back(std::move(wavesRitem));

    auto gridRitem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&gridRitem->World, XMMatrixTranslation(-10.0f, -1.5f, 0.0f));
    XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
    gridRitem->ObjCBIndex = objCBIndex++;
    gridRitem->Mat = mMaterials["grass"].get();
    gridRitem->Geo = mGeometries["landGeo"].get();
    gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
    gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
    gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
    mAllRitems.push_back(std::move(gridRitem));


    createCastlePerimeter(objCBIndex);

    createMainCastle(objCBIndex);
    createTorch(objCBIndex);
    
    //Test shape for lighting
    //createShapeInWorld(objCBIndex, XMFLOAT3(5.0f, 5.0f, 5.0f), XMFLOAT3(0.0, 10.0, -20.0), 0.0f, "box", "woodV");

 

	// All the render items are opaque.
	for(auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}


//The DrawRenderItems method is invoked in the main Draw call:
void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
 
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
    auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // For each render item...
    for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

       // // Offset to the CBV in the descriptor heap for this object and for this frame resource.
       // UINT cbvIndex = mCurrFrameResourceIndex*(UINT)mOpaqueRitems.size() + ri->ObjCBIndex;
       // auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
       // cbvHandle.Offset(cbvIndex, mCbvSrvUavDescriptorSize);
       //
       // cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);
       //
       // cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
        CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
        tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

        cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

void ShapesApp::createTower(UINT& objIndex, XMFLOAT3 location)
{
    //Creates main tower structure
    createShapeInWorld(objIndex, XMFLOAT3(2.0f, 7.0f, 2.0f), XMFLOAT3(location.x, location.y + 1.0f, location.z), 45.0f, "cylinder", "sandbrick");
    //Creates gray stone foundation 
    createShapeInWorld(objIndex, XMFLOAT3(3.0f, 1.0f, 3.0f), XMFLOAT3(location.x, location.y, location.z), 0.0f, "grayBox", "stonebrick");
    //Creates platoform at top of towers 
    createShapeInWorld(objIndex, XMFLOAT3(3.0f, 0.5f, 3.0f), XMFLOAT3(location.x, location.y + 8.0f, location.z), 0.0f, "box", "sandbrick");
    //Creates thin platform "walkway" for better color variety
   createShapeInWorld(objIndex, XMFLOAT3(2.5f, 0.5f, 2.5f), XMFLOAT3(location.x, location.y + 8.1f, location.z), 0.0f, "box", "woodV");

    //Creates pirapits for tops of towers
    for (int i = 0; i < 7; i++)
    {
        float temp = i;

        if (i % 2 == 0)
        {
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.5f), XMFLOAT3(location.x - 1.5f + (temp / 2), location.y + 8.5f, location.z - 1.5f), 0.0f, "box", "sandbrick");
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x - 1.5f + (temp / 2), location.y + 9.5f, location.z - 1.5f), 45.0f, "pyramid", "stone");
        }
        else
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x - 1.5f + (temp / 2), location.y + 8.5f, location.z - 1.5f), 0.0f, "box", "sandbrick");

    }
    for (int i = 0; i < 7; i++)
    {
        float temp = i;

        if (i % 2 == 0)
        {
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.5f), XMFLOAT3(location.x - 1.5f + (temp / 2), location.y + 8.5f, location.z + 1.5f), 0.0f, "box", "sandbrick");
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x - 1.5f + (temp / 2), location.y + 9.5f, location.z + 1.5f), 45.0f, "pyramid", "stone");
        }
        else
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x - 1.5f + (temp / 2), location.y + 8.5f, location.z + 1.5f), 0.0f, "box", "sandbrick");
    }

    for (int i = 0; i < 5; i++)
    {
        float temp = i;

        if (i % 2 == 0)
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x - 1.5f , location.y + 8.5f, location.z - 1.0f + (temp / 2)), 0.0f, "box", "sandbrick");
        else
        {
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.5f), XMFLOAT3(location.x - 1.5f, location.y + 8.5f, location.z - 1.0f + (temp / 2)), 0.0f, "box", "sandbrick");
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x - 1.5f, location.y + 9.5f, location.z - 1.0f + (temp/2)), 45.0f, "pyramid", "stone");
        }
    }

    for (int i = 0; i < 5; i++)
    {
        float temp = i;

        if (i % 2 == 0)
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x + 1.5f, location.y + 8.5f, location.z - 1.0f + (temp / 2)), 0.0f, "box", "sandbrick");
        else
        {
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.5f), XMFLOAT3(location.x + 1.5f, location.y + 8.5f, location.z - 1.0f + (temp / 2)), 0.0f, "box", "sandbrick");
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x + 1.5f, location.y + 9.5f, location.z - 1.0f + (temp / 2)), 45.0f, "pyramid", "stone");
        }
    }
}

void ShapesApp::createWall(UINT& objIndex, XMFLOAT3 location, float rotation, int sign)
{
    //Signs moves the piratits to correct location on wall depending on walls rotations

    //Creates gray stone foundation 
    createShapeInWorld(objIndex, XMFLOAT3(18.0f, 1.0f, 2.0f), XMFLOAT3(location.x, location.y, location.z), rotation, "greyBox", "stonebrick");
    //Creates main wall structure
    createShapeInWorld(objIndex, XMFLOAT3(18.0f, 4.0f, 2.0f), XMFLOAT3(location.x, location.y +1.0f, location.z), rotation, "box", "sandbrick");
    //Creates thin platform "walkway" for better color variety
    createShapeInWorld(objIndex, XMFLOAT3(18.0f, 0.1f, 2.0f), XMFLOAT3(location.x, location.y + 5.0f, location.z), rotation, "box", "woodV");
 
    //Creates pirapits for tops of walls
    if (rotation == 0.0f)
    {
        for (int i = 0; i < 36; i++)
        {
            float temp = i;

            if (i % 2 == 0)
            {
                createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.5f), XMFLOAT3(location.x - 8.5f + (temp / 2), location.y + 5.0f, location.z - 1.0f * sign), rotation, "box", "sandbrick");
                //Adds pyramid top to pirapites
                createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x - 8.5f + (temp / 2), location.y + 6.0f, location.z - 1.0f *sign), 45.0f, "pyramid", "stone");
            }
            else
                createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x - 8.5f + (temp / 2), location.y + 5.0f, location.z - 1.0f * sign), rotation, "box", "sandbrick");
        }
    }
    else
    {
        for (int i = 0; i < 36; i++)
        {
            float temp = i;

            if (i % 2 == 0)
            {
                createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.5f), XMFLOAT3(location.x - 1.0f * sign, location.y + 5.0f, location.z - 8.5f + (temp / 2)), rotation, "box", "sandbrick");
                //Adds pyramid top to pirapites
                createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x - 1.0f * sign, location.y + 6.0f, location.z - 8.5f + (temp / 2)), 45.0f, "pyramid", "stone");
            }
            else
                createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x - 1.0f * sign, location.y + 5.0f, location.z - 1.0f - 8.5f + (temp / 2)), rotation, "box", "sandbrick");
        }
    }

}
void ShapesApp::createMainGate(UINT& objIndex, XMFLOAT3 location)
{
    //Creates gray stone foundation 
    createShapeInWorld(objIndex, XMFLOAT3(18.0f, 1.0f, 2.0f), XMFLOAT3(location.x, location.y, location.z), 0.0f, "grayBox", "stonebrick");
    //Creates left portion of main wall
    createShapeInWorld(objIndex, XMFLOAT3(6.0f, 4.0f, 2.0f), XMFLOAT3(location.x -6.0f, location.y + 1.0f, location.z), 0.0f, "box", "sandbrick");
    //Creates right portion of main wall
    createShapeInWorld(objIndex, XMFLOAT3(6.0f, 4.0f, 2.0f), XMFLOAT3(location.x + 6.0f, location.y + 1.0f, location.z), 0.0f, "box", "sandbrick");
    //Creates fills in gap below pirapits at center of wall
    createShapeInWorld(objIndex, XMFLOAT3(6.0f, 1.0f, 2.0f), XMFLOAT3(location.x , location.y + 4.0f, location.z), 0.0f, "box", "sandbrick");
    //Creats "drawbridge" 
    createShapeInWorld(objIndex, XMFLOAT3(6.0f, 1.0f, 5.0f), XMFLOAT3(location.x, location.y, location.z-3.5f), 0.0f, "wedge", "woodV");
    //Creates thin platform "walkway" for better color variety
    createShapeInWorld(objIndex, XMFLOAT3(18.0f, 0.1f, 2.0f), XMFLOAT3(location.x, location.y + 5.0f, location.z), 0.0f, "box", "woodV");
    //Creates left drawbridge rope
    createShapeInWorld(objIndex, XMFLOAT3(0.05f, 0.05f, 2.75f), XMFLOAT3(location.x - 3.0f, location.y + 2.0f, location.z - 2.75f), XMFLOAT3(-45.0f,0.0f,60.0f), "blackCylinder", "iron");
    //Creates right drawbridge rope
    createShapeInWorld(objIndex, XMFLOAT3(0.05f, 0.05f, 2.75f), XMFLOAT3(location.x + 3.0f, location.y + 2.0f, location.z - 2.75f), XMFLOAT3(-45.0f, 0.0f, 40.0f), "blackCylinder", "iron");
    
    //Creates pirapits for tops of walls
    for (int i = 0; i < 36; i++)
    {
        float temp = i;

        if (i % 2 == 0)
        {
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.5f), XMFLOAT3(location.x - 8.5f + (temp / 2), location.y + 5.0f, location.z - 1.0f), 0.0f, "box", "sandbrick");
            //Adds pyramid top to pirapites
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x - 8.5f + (temp / 2), location.y + 6.0f, location.z - 1.0f), 45.0f, "pyramid", "stone");
        }
        else
            createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(location.x - 8.5f + (temp / 2), location.y + 5.0f, location.z - 1.0f), 0.0f, "box", "sandbrick");
    }
}
void ShapesApp::createTowerDoors(UINT& objIndex)
{
    //Doorframes & Doors for front left tower
    createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.2f), XMFLOAT3(-10.0f, 5.0f, -8.75f), 0.0f, "torus", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.25f, 0.5f), XMFLOAT3(-10.0f, 5.0f, -9.0f), 0.0f, "box", "woodV");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(-10.1f, 5.2f, -8.7f), 0.0f, "sphere", "iron" );


    createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.2f), XMFLOAT3(-8.75f, 5.0f, -10.0f), 90.0f, "torus", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.25f, 0.5f), XMFLOAT3(-9.0f, 5.0f, -10.0f), 90.0f, "wedge", "woodV");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(-8.65f, 5.2f, -9.9f), 0.0f, "sphere", "iron");

    //Doorframes & Doors for front right tower
    createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.2f), XMFLOAT3(10.0f, 5.0f, -8.75f), 0.0f, "torus", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.25f, 0.5f), XMFLOAT3(10.0f, 5.0f, -9.0f), 0.0f, "wedge", "woodV");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(9.9f, 5.2f, -8.7f), 0.0f, "sphere", "iron");


    createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.2f), XMFLOAT3(8.75f, 5.0f, -10.0f), 90.0f, "torus", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.25f, 0.5f), XMFLOAT3(9.0f, 5.0f, -10.0f), 270.0f, "wedge", "woodV");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(8.65f, 5.2f, -10.1f), 0.0f, "sphere", "iron");

    //Doorframes & Doors for back left tower
    createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.2f), XMFLOAT3(-10.0f, 5.0f, 8.75f), 0.0f, "torus", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.25f, 0.5f), XMFLOAT3(-10.0f, 5.0f, 9.0f), 180.0f, "wedge", "woodV");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(-9.9f, 5.2f, 8.7f), 0.0f, "sphere", "iron");


    createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.2f), XMFLOAT3(-8.75f, 5.0f, 10.0f), 90.0f, "torus", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.25f, 0.5f), XMFLOAT3(-9.0f, 5.0f, 10.0f), 90.0f, "wedge", "woodV");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(-8.65f, 5.2f, 10.1f), 0.0f, "sphere", "iron");


    //Doorframes & Doors for back right tower
    createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.2f), XMFLOAT3(10.0f, 5.0f, 8.75f), 0.0f, "torus", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.25f, 0.5f), XMFLOAT3(10.0f, 5.0f, 9.0f), 180.0f, "wedge", "woodV");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(10.1f, 5.2f, 8.7f), 0.0f, "sphere", "iron");


    createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.0f, 0.2f), XMFLOAT3(8.75f, 5.0f, 10.0f), 90.0f, "torus", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.25f, 0.5f), XMFLOAT3(9.0f, 5.0f, 10.0f), 270.0f, "wedge", "woodV");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(8.65f, 5.2f, 9.9f), 0.0f, "sphere", "iron");

    // Castle Door
    createShapeInWorld(objIndex, XMFLOAT3(1.5f, 2.0f, 0.2f), XMFLOAT3(0.0f, -1.0f, -5.1f), 0.0f, "torus", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(2.5f, 1.5f, 0.5f), XMFLOAT3(0.0f, 0.0f, -4.8f), 180.0f, "wedge", "woodV");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.5f, 0.2f, -5.1f), 0.0f, "sphere", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(-0.5f, 0.2f, -5.1f), 0.0f, "sphere", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(0.05f, 1.5f, 0.05f), XMFLOAT3(0.0f, 0.0f, -5.1f), 0.0f, "blackCylinder", "iron");
}

void ShapesApp::createCastlePerimeter(UINT& objIndex)
{
    //Creates front left tower 
    createTower(objIndex, XMFLOAT3(-10.0f, 0.0f, -10.0f));
    //Creates back left tower 
    createTower(objIndex, XMFLOAT3(-10.0f, 0.0f, 10.0f));
    //Creates front right tower 
    createTower(objIndex, XMFLOAT3(10.0f, 0.0f, -10.0f));
    //Creates back right tower 
    createTower(objIndex, XMFLOAT3(10.0f, 0.0f, 10.0f));

    //Creates right wall
    createWall(objIndex, XMFLOAT3(0.0f, 0.0f, 10.0f), 0.f, -1);
    //Creates left wall
    createWall(objIndex, XMFLOAT3(-10.0f, 0.0f, 0.0f), 90.f, 1);
    //Creates back wall
    createWall(objIndex, XMFLOAT3(10.0f, 0.0f, 0.0f), 90.f, -1);

    //Creates front gate
    createMainGate(objIndex, XMFLOAT3(0.0f, 0.0f, -10.0f));

    //Creates doorframes/doors/doorknobs
    createTowerDoors(objIndex);
}
void ShapesApp::createCastleTowers(UINT& objIndex)
{
    // Lower main cylinders
    createShapeInWorld(objIndex, XMFLOAT3(1.5f, 10.0f, 1.5f), XMFLOAT3(5.0, 0.0, 5.0), 0.0f, "roundcylinder", "sandbrick");
    createShapeInWorld(objIndex, XMFLOAT3(1.5f, 10.0f, 1.5f), XMFLOAT3(-5.0, 0.0, 5.0), 0.0f, "roundcylinder", "sandbrick");
    createShapeInWorld(objIndex, XMFLOAT3(1.5f, 10.0f, 1.5f), XMFLOAT3(5.0, 0.0, -5.0), 0.0f, "roundcylinder", "sandbrick");
    createShapeInWorld(objIndex, XMFLOAT3(1.5f, 10.0f, 1.5f), XMFLOAT3(-5.0, 0.0, -5.0), 0.0f, "roundcylinder", "sandbrick");

    // Uppers main cylinders
    createShapeInWorld(objIndex, XMFLOAT3(1.75f, 5.0f, 1.75f), XMFLOAT3(5.0, 8.0, 5.0), 0.0f, "roundcylinder", "sandbrick");
    createShapeInWorld(objIndex, XMFLOAT3(1.75f, 5.0f, 1.75f), XMFLOAT3(-5.0, 8.0, 5.0), 0.0f, "roundcylinder", "sandbrick");
    createShapeInWorld(objIndex, XMFLOAT3(1.75f, 5.0f, 1.75f), XMFLOAT3(5.0, 8.0, -5.0), 0.0f, "roundcylinder", "sandbrick");
    createShapeInWorld(objIndex, XMFLOAT3(1.75f, 5.0f, 1.75f), XMFLOAT3(-5.0, 8.0, -5.0), 0.0f, "roundcylinder", "sandbrick");

    // Cylinder breaks
    createShapeInWorld(objIndex, XMFLOAT3(2.0f, 0.5f, 2.0f), XMFLOAT3(5.0, 7.5, 5.0), 0.0f, "blackCylinder",  "iron");
    createShapeInWorld(objIndex, XMFLOAT3(2.0f, 0.5f, 2.0f), XMFLOAT3(-5.0, 7.5, 5.0), 0.0f, "blackCylinder", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(2.0f, 0.5f, 2.0f), XMFLOAT3(5.0, 7.5, -5.0), 0.0f, "blackCylinder", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(2.0f, 0.5f, 2.0f), XMFLOAT3(-5.0, 7.5, -5.0), 0.0f, "blackCylinder","iron");
      
    // Under cone breaks
    createShapeInWorld(objIndex, XMFLOAT3(2.1f, 0.2f, 2.1f), XMFLOAT3(5.0, 12.8, 5.0), 0.0f, "blackCylinder",  "iron");
    createShapeInWorld(objIndex, XMFLOAT3(2.1f, 0.2f, 2.1f), XMFLOAT3(-5.0, 12.8, 5.0), 0.0f, "blackCylinder", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(2.1f, 0.2f, 2.1f), XMFLOAT3(5.0, 12.8, -5.0), 0.0f, "blackCylinder", "iron");
    createShapeInWorld(objIndex, XMFLOAT3(2.1f, 0.2f, 2.1f), XMFLOAT3(-5.0, 12.8, -5.0), 0.0f, "blackCylinder","iron");

    // Tower Base
    createShapeInWorld(objIndex, XMFLOAT3(2.1f, 1.0f, 2.1f), XMFLOAT3(5.0, 0.0, 5.0), 0.0f, "blackCylinder",  "stonebrick");
    createShapeInWorld(objIndex, XMFLOAT3(2.1f, 1.0f, 2.1f), XMFLOAT3(-5.0, 0.0, 5.0), 0.0f, "blackCylinder", "stonebrick");
    createShapeInWorld(objIndex, XMFLOAT3(2.1f, 1.0f, 2.1f), XMFLOAT3(5.0, 0.0, -5.0), 0.0f, "blackCylinder", "stonebrick");
    createShapeInWorld(objIndex, XMFLOAT3(2.1f, 1.0f, 2.1f), XMFLOAT3(-5.0, 0.0, -5.0), 0.0f, "blackCylinder","stonebrick");

    // Top of tower cones
    createShapeInWorld(objIndex, XMFLOAT3(2.5f, 5.0f, 2.5f), XMFLOAT3(5.0, 13.0, 5.0), 0.0f, "cone",  "shingle");
    createShapeInWorld(objIndex, XMFLOAT3(2.5f, 5.0f, 2.5f), XMFLOAT3(-5.0, 13.0, 5.0), 0.0f, "cone", "shingle");
    createShapeInWorld(objIndex, XMFLOAT3(2.5f, 5.0f, 2.5f), XMFLOAT3(5.0, 13.0, -5.0), 0.0f, "cone", "shingle");
    createShapeInWorld(objIndex, XMFLOAT3(2.5f, 5.0f, 2.5f), XMFLOAT3(-5.0, 13.0, -5.0), 0.0f, "cone","shingle");

}

void ShapesApp::createMainCastle(UINT& objIndex)
{
    createCastleTowers(objIndex);

    // Main Cube
    createShapeInWorld(objIndex, XMFLOAT3(10.0f, 10.0f, 10.0f), XMFLOAT3(0.0, 0.0, 0.0), 0.0f, "box", "sandbrick");

    // Raised platforms
    createShapeInWorld(objIndex, XMFLOAT3(2.0f, 1.0f, 10.0f), XMFLOAT3(5.0, 10.0, 0.0), 0.0f, "box",  "woodV");
    createShapeInWorld(objIndex, XMFLOAT3(2.0f, 1.0f, 10.0f), XMFLOAT3(-5.0, 10.0, 0.0), 0.0f, "box", "woodV");
    createShapeInWorld(objIndex, XMFLOAT3(10.0f, 1.0f, 2.0f), XMFLOAT3(0.0, 10.0, 5.0), 0.0f, "box",  "woodV");
    createShapeInWorld(objIndex, XMFLOAT3(10.0f, 1.0f, 2.0f), XMFLOAT3(0.0, 10.0, -5.0), 0.0f, "box", "woodV");

    // Platform Walls
    createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.5f, 10.0f), XMFLOAT3(6.0, 10.0, 0.0), 0.0f, "box", "sandbrick");
    createShapeInWorld(objIndex, XMFLOAT3(0.5f, 1.5f, 10.0f), XMFLOAT3(-6.0, 10.0, 0.0), 0.0f, "box", "sandbrick");
    createShapeInWorld(objIndex, XMFLOAT3(10.0f, 1.5f, 0.5f), XMFLOAT3(0.0, 10.0, 6.0), 0.0f, "box", "sandbrick");
    createShapeInWorld(objIndex, XMFLOAT3(10.0f, 1.5f, 0.5f), XMFLOAT3(0.0, 10.0, -6.0), 0.0f, "box", "sandbrick");

    // Castle Studs
    for (int i = 0; i < 3; i++)
    {
        // Front Studs
          createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(-2.0 + (i * 2), 7.5, -5.5), XMFLOAT3(-90.0f, 0.0f, 0.0f), "pyramid", "iron");
          createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(-2.0 + (i * 2), 4.0, -5.5), XMFLOAT3(-90.0f, 0.0f, 0.0f), "pyramid", "iron");
       // Back Studs
          createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(-2.0 + (i * 2), 7.5, 5.5), XMFLOAT3(90.0f, 0.0f, 0.0f), "pyramid", "iron");
          createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(-2.0 + (i * 2), 4.0, 5.5), XMFLOAT3(90.0f, 0.0f, 0.0f), "pyramid", "iron");
       // Right Studs
          createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(5.5, 7.5, -2.0 + (i * 2)), XMFLOAT3(90.0f, 90.0f, 0.0f), "pyramid", "iron");
          createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(5.5, 4.0, -2.0 + (i * 2)), XMFLOAT3(90.0f, 90.0f, 0.0f), "pyramid", "iron");
      // Left Studs
          createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(-5.5, 7.5, -2.0 + (i * 2)), XMFLOAT3(90.0f, -90.0f, 0.0f), "pyramid", "iron");
          createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(-5.5, 4.0, -2.0 + (i * 2)), XMFLOAT3(90.0f, -90.0f, 0.0f), "pyramid", "iron");
    }

    // Upper platform support wedges
    for (int i = 0; i < 7; i++)
    {
         createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(-3.0 + i, 9.5, -5.5), XMFLOAT3(0.0f, 0.0f, 180.0f), "wedge",  "woodV");
         createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(-3.0 + i, 9.5, 5.5), XMFLOAT3(0.0f, 180.0f, 180.0f), "wedge", "woodV");
         createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(5.5, 9.5, -3.0 + i), XMFLOAT3(0.0f, -90.0f, 180.0f), "wedge", "woodV");
         createShapeInWorld(objIndex, XMFLOAT3(0.5f, 0.5f, 0.5f), XMFLOAT3(-5.5, 9.5, -3.0 + i), XMFLOAT3(0.0f, 90.0f, 180.0f), "wedge", "woodV");
    }

    // Upper platform top wedges
    for (int i = 0; i < 5; i++)
    {
        createShapeInWorld(objIndex, XMFLOAT3(1.0f, 0.5f, 0.5f), XMFLOAT3(-3.0 + (i * 2), 11.5, -6.0), XMFLOAT3(0.0f, 0.0f, 0.0f), "wedge",  "stone");
        createShapeInWorld(objIndex, XMFLOAT3(1.0f, 0.5f, 0.5f), XMFLOAT3(-3.0 + (i * 2), 11.5, 6.0), XMFLOAT3(0.0f, 180.0f, 0.0f), "wedge", "stone");
        createShapeInWorld(objIndex, XMFLOAT3(1.0f, 0.5f, 0.5f), XMFLOAT3(-6.0, 11.5, -3.0 + (i *2)), XMFLOAT3(0.0f, 90.0f, 0.0f), "wedge",  "stone");
        createShapeInWorld(objIndex, XMFLOAT3(1.0f, 0.5f, 0.5f), XMFLOAT3(6.0, 11.5, -3.0 + (i * 2)), XMFLOAT3(0.0f, -90.0f, 0.0f), "wedge", "stone");
    }
    // Top of castle ground
    createShapeInWorld(objIndex, XMFLOAT3(10.0f, 0.1f, 10.0f), XMFLOAT3(0.0, 10.1, 0.0), 0.0f, "box", "stonebrick");

    // Diamond Holders
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 2.0f, 1.0f), XMFLOAT3(1.0, 10.0, 1.0), 75.0f, "triprism",   "iron");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 2.0f, 1.0f), XMFLOAT3(-1.0, 10.0, 1.0),-15.0f, "triprism",  "iron");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 2.0f, 1.0f), XMFLOAT3(1.0, 10.0, -1.0), 45.0f, "triprism",  "iron");
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 2.0f, 1.0f), XMFLOAT3(-1.0, 10.0, -1.0), 15.0f, "triprism", "iron");

    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.5f, 1.0f), XMFLOAT3(0.0, 12.5, 0.0), 0.0f, "diamond", "sandbrick");
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> ShapesApp::GetStaticSamplers()
{
    // Applications usually only need a handful of samplers.  So just define them all up front
    // and keep them available as part of the root signature.  

    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
        0, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        1, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        2, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        3, // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
        4, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
        0.0f,                             // mipLODBias
        8);                               // maxAnisotropy

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
        5, // shaderRegister
        D3D12_FILTER_ANISOTROPIC, // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
        0.0f,                              // mipLODBias
        8);                                // maxAnisotropy

    return {
        pointWrap, pointClamp,
        linearWrap, linearClamp,
        anisotropicWrap, anisotropicClamp };
}

float ShapesApp::GetHillsHeight(float x, float z) const
{
    return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z)) + 0.1f;
}

XMFLOAT3 ShapesApp::GetHillsNormal(float x, float z) const
{
    // n = (-df/dx, 1, -df/dz)
    XMFLOAT3 n(
        -0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
        1.0f,
        -0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

    XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
    XMStoreFloat3(&n, unitNormal);

    return n;
}
void ShapesApp::createTorch(UINT& objIndex)
{
    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(4.0, 2, -11.5), XMFLOAT3(0.0f, 0.0f, 180.0f), "torchHandle", "wroughtIron");
    createShapeInWorld(objIndex, XMFLOAT3(0.4f, 0.4f, 0.4f), XMFLOAT3(4.0, 2.89, -11.5), XMFLOAT3(0.0f, 0.0f, 0.0f), "box", "flames");

    createShapeInWorld(objIndex, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(-4.0, 2, -11.5), XMFLOAT3(0.0f, 0.0f, 180.0f), "torchHandle", "wroughtIron");
    createShapeInWorld(objIndex, XMFLOAT3(0.4f, 0.4f, 0.4f), XMFLOAT3(-4.0, 2.89, -11.5), XMFLOAT3(0.0f, 0.0f, 0.0f), "box", "flames");

    mMainPassCB.Lights[1].Position = { 4.0, 3.5, -11.5 };
    mMainPassCB.Lights[1].Strength = { 1.0,0.5,0.0 };
    mMainPassCB.Lights[1].FalloffStart = 1;
    mMainPassCB.Lights[1].FalloffEnd = 3;

    mMainPassCB.Lights[2].Position = { -4.0, 3.5, -11.5 };
    mMainPassCB.Lights[2].Strength = { 1.0,0.5,0.0 };
    mMainPassCB.Lights[2].FalloffStart = 1;
    mMainPassCB.Lights[2].FalloffEnd = 3;
}
