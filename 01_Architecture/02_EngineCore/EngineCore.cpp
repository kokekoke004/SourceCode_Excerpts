#include "EngineCore.h"
#include"CreateResource.h"
#include"DrawText/TextRenderer.h"
#include"FbxRenderFix.h"
#include"EngineAPI.h"
#include"FileManager.h"
#include"CollisionAttributeManager.h"
#include"ModelLoadSystem.h"
#include"ModelRenderSystem.h"   
#include"GraphicsUtility.h"
#include"FbxLoader.h"
#include"PythonEngine.h"
#include"FbxPos.h"
#include"PipelineManager.h"

#include"FbxPipelineBuilder.h"
#include"PostProcessBuilder.h"
#include"DisplacementBuilder.h"
#include"PipelineBuilderDebugDraw.h"
#include"LoadTexture.h"
#include "PostProcessPass.h"
#include"MouseInput.h"
#include"KeyInput.h"
#include"ComputeShaderManager.h"
#include"DebugDrawManager.h"
#include"pipelineStateObject.h"
#include"FrustumCuller.h"
#include"UpdateInstancePosCompute.h"
#include"ChunkManager.h"
#include"NavigationSystem.h"
#include"UiManager.h"
#include"PrimitiveManager.h"
#include"IDebugDraw.h"

using namespace Engine::Render::Pipeline;


namespace Engine
{
    EngineCore::EngineCore(const EngineCoreConfig& cfg)
        : title_(cfg.title),
        windowWidth_(cfg.width),
        windowHeight_(cfg.height),
        viewport_(0.0f, 0.0f, float(cfg.width), float(cfg.height)),
        scissorRect_(0, 0, LONG(cfg.width), LONG(cfg.height))
    {
        ViewportFrom(viewScissor) = viewport_;
        ScissorRectFrom(viewScissor) = scissorRect_;

        RenderSize rs{};
        rs.windowPixWidth = windowWidth_;
        rs.windowPixHeight = windowHeight_;
        RenderSizeFrom(commandContext) = rs;
    }

    EngineCore::~EngineCore()
    {

    }

    void EngineCore::Init(HWND& hwnd)
    {
        // ハンドルは先に Context に入れてOK
        HwndFrom(commandContext) = hwnd;

        // Device / Queue / Allocator / List / SwapChain / RTV/DSV / Fence
        InitializeD3D12(hwnd);

        // context に swapchain をセット（あなたの他コードと互換）
        SwapchainFrom(commandContext) = swapchain_;

        //ここでコマンドリスト関連セット
        BindCommandContext();



        //エンジンで使用するクラスをインスタンス化する
        InitEngineServices();

        //クラスで必要なパイプラインやメゾッドの値を初期化する
        InitRenderResources();
    }


    void EngineCore::InitEngineServices()
    {

        {
            textInputManager_ = std::make_unique<TextInputManager>();
            services_.textInputManager = textInputManager_.get();
        }

		//フラスタムカリングクラスインスタンス化
        {
            frustumCuller_ = std::make_unique<FrustumCuller>();
			services_.frustumCuller = frustumCuller_.get();
        }

        //パイプライン生成クラスをインスタンス化
        {
            pipelineManager_ = std::make_unique<Render::Pipeline::PipelineManager>();
            Engine::Render::Pipeline::PipelineBuildContext contex(DeviceFrom(commandContext).Get());
            pipelineManager_->Init(contex);
			services_.pipelineManager = pipelineManager_.get();
        }

        
        //textRenderer初期化
        {
            textRenderer_ = std::make_unique<TextRenderer>();
            textRenderer_->Initialize(commandContext, renderTargets_, swapchain_);
            services_.textRenderer = textRenderer_.get();
        }

		//パイソンスクリプト実行クラスインスタンス化
        {
            //pythonEngine_ = std::make_unique<PythonEngine>(u8"C:/DirectX12/研究用Python/Resarch", u8"C:/DirectX12/研究用Python/Resarch/env/Lib/site-packages");
            std::string exeDir = GetExeDirectory();
            printf("\nPython初期化開始");
            pythonEngine_ = std::make_unique<PythonEngine>(exeDir, exeDir + "/python/Lib/site-packages");
            printf("\nPython初期化完了");

            printf("\nPythonモジュール読み込み開始");
            pythonEngine_->SetModuleName("Resarch");
            printf("\nPythonモジュール読み込み完了");

            services_.pythonEngine = pythonEngine_.get();

        }

		//スクリーンショットクラスインスタンス化
        {
			sreenshot_.reset(new Screenshot(commandContext));
			services_.screenshot = sreenshot_.get();
        }

        //トランスフォームシステムインスタンス化
        {
            transformSystem_.reset(new Components::TransformSystem());
            services_.transform = transformSystem_.get();
        }

        //モデルのアニメション設定や描画設定を管理するクラスインスタンス化
        {
            fbxInstance_.reset(new Assets::FbxInstanceRegistry());
            services_.fbxInstance = fbxInstance_.get();
        }


        //モデルロードクラスインスタンス化
        {
            //ファイルマネージャインスタンス化
            fileManager_ = std::make_unique<FileManager>(pythonEngine_);

			services_.fileManager = fileManager_.get();

            //コリジョン特性クラスインスタンス化
            collisionAttributeManager_ = std::make_unique<CollisionAttributeManager>();

            //エンジンサービスにcollisionAttributeセット
            services_.collisionAttribute = collisionAttributeManager_.get();


            //モデルロードシステムのインスタンス化
            modelLoadSystem_ = std::make_unique<Engine::ModelLoadSystem>(*fileManager_, *collisionAttributeManager_, *transformSystem_, *fbxInstance_);

            services_.modelLoader = modelLoadSystem_.get();

			//エンジンサービスにmodelProviderセット
			services_.modelProvider = modelLoadSystem_.get();


        }

        //テクスチャロードクラスインスタンス化
        {
            loadTex_ = std::make_unique<LoadTex>(DeviceFrom(commandContext));
            services_.loadTex = loadTex_.get();
        }


        //コンピュートシェーダインスタンス化
        {
            computeShaderManager_ = std::make_unique<ComputeShaderManager>(commandContext, loadTex_, *commandSystem_);
            services_.computeShaderManager = computeShaderManager_.get();
        }


        //デバッグドロー関係初期化
        {

            updateInstancePosCompute_ = std::make_unique<Compute::UpdateInstancePos::UpdateInstancePosCompute>(computeShaderManager_);

            debugDrawManager_ = std::make_unique<DebugDrawManager>(createResource_, *fbxInstance_,pipelineManager_->GetRegistry(), *modelLoadSystem_, frustumCuller_, computeShaderManager_, updateInstancePosCompute_, commandContext);
			services_.debugDrawManager = debugDrawManager_.get();
            //std::shared_ptr<CreateResource>& createResource, std::shared_ptr<PipelineStateObject>& pipelineStateObject, Engine::IModelProvider& iModelProvider, std::shared_ptr<FrustumCuller>& frustumCuller, std::shared_ptr<ComputeShaderManager>& computeShaderManager, std::shared_ptr<Compute::UpdateInstancePos::UpdateInstancePosCompute>& updateInstancePosCompute,
            //CommandContext& commandContext
        }

        //経路探索用クラスインスタンス化
        {
            navigationSystem_.reset(new Navigation::NavigationSystem(*modelLoadSystem_,*fbxInstance_));
            services_.navigationSystem = navigationSystem_.get();
        }

        //チャンクマネージャクラスインスタンス化
        {
            chunkManager_ = std::make_unique<ChunkManager>(0, 16);
            services_.chunkManager = chunkManager_.get();
        }

        //リソース生成クラスをサービスに渡す
        {
			services_.createResource = createResource_.get();
        }


        //LODの管理クラスインスタンス化
        {
            modelLodManager_ = std::make_unique<Model::Lod::ModelLodManager>();
            services_.modelLodManager = modelLodManager_.get();
        }


        //モデルレンダーシステムクラスインスタンス化
        {
            fbxRenderer_ = std::make_unique<Render::FbxRender>(debugDrawManager_,computeShaderManager_,pipelineManager_->GetRegistry(),*modelLoadSystem_, frustumCuller_,*transformSystem_,*fbxInstance_);
            modelRenderSystem_ = std::make_unique<Engine::ModelRenderSystem>(*fbxRenderer_, *modelLoadSystem_,*modelLodManager_);
            services_.modelRenderer = modelRenderSystem_.get();
        }


        //ポストプロセスクラスインスタンス化
        {
            postProcess_ = std::make_unique<Render::PostProcess::PostProcess>();
            services_.postProcess = postProcess_.get();

        }

		//ライティングクラスインスタンス化
        {
			lightManager_ = std::make_unique<Render::Lighting::LightManager>();
			services_.lightManager = lightManager_.get();
        }

        //カメラクラスを初期化する
        {
            cameraManager_ = std::make_unique<CameraManager>(*transformSystem_);
            services_.camera = cameraManager_.get();
        }

        //マウス入力処理初期化
        {
            mouseInput_ = std::make_unique<MouseInput>(commandContext);
            services_.mouse = mouseInput_.get();
        }

        //キー入力初期化
        {
            keyInput_ = std::make_unique<KeyInput>();
            services_.key = keyInput_.get();

        }

        //ゲームタイマークラスインスタンス化
        {
            gameTimer_ = std::make_unique<GameTimer>();
            services_.gameTimer = gameTimer_.get();
        }


        //ディスプレイスメントマッピングインスタンス化
        {
            std::wstring heightMap = L"HeightMap/heightMap3.jpg";
            displacementMapping_ = std::make_unique<DisplacementMapping>(heightMap, pipelineManager_->GetRegistry(), commandContext, viewScissor, lightDepthBuffer_);
            services_.displacementMapping = displacementMapping_.get();
        }

        //UIマネージャーインスタンス化
        {
            UIRenderContext uiRenderContext(commandContext, rtvHeap_, renderTargets_, viewScissor);
            uiManager = std::make_unique<UiManager>(uiRenderContext);
            services_.uiManager = uiManager.get();
		}

		//プリミティブ描画クラスインスタンス化
        {
            Primitive::CreateResources::PrimitiveService primitiveService;
            Primitive::Pipeline::PrimitiveCreatePipelineService primitiveCreatePipelineService;
			Primitive::Manager::PrimitiveManagerContext primitiveManagerContext;
			primitiveManagerContext.dsvHeap = dsvHeap_;
			pipelineStateObject_.reset(new PipelineStateObject(DeviceFrom(commandContext)));

            primitiveService.loadTex = loadTex_.get();
            primitiveCreatePipelineService.pipelineStateObject = pipelineStateObject_.get();
            Primitive::Manager::PrimitiveManagerService primitiveManagerService(*updateInstancePosCompute_,*postProcess_);
            primitiveManager_.reset(new Primitive::Manager::PrimitiveManager(commandContext, viewScissor, primitiveService, primitiveCreatePipelineService, primitiveManagerService, primitiveManagerContext));
			services_.primitive = primitiveManager_.get();
        }

		//コリジョンシステムクラスインスタンス化
        {
			collisionSystem_.reset(new CollisionSystem(*modelLoadSystem_));
			services_.collisionSystem = collisionSystem_.get();
        }

        //シーンマネージャークラスインスタンス化
        {
            sceneManager_.reset(new Scene::SceneManager());
            services_.sceneManager = sceneManager_.get();
		}

        //移動による更新全般
        {
            locomotion_.reset(new Engine::Movement::LocomotionSystem(*transformSystem_));
            services_.locomotion = locomotion_.get();
        }

		//地形への接地判定や、接地しているかの判定などを行うクラスインスタンス化
        {
			Movement::GroundingServices groundingServices(*transformSystem_,*displacementMapping_);
			groundingSystem_.reset(new Engine::Movement::GroundingSystem(groundingServices));
			services_.groundingSystem = groundingSystem_.get();
        }

		//アニメーション関連の再生や、アニメーションの更新などを行うクラスインスタンス化
        {
			animationController_.reset(new Engine::Animation::AnimationController());
			services_.animationController = animationController_.get();
        }

        //時間倍率適用
        {
            timeScaleManager_.reset(new Engine::Time::TimeScaleManager());
            services_.timeScaleManager = timeScaleManager_.get();
        }

    }

    void EngineCore::InitRenderResources()
    {

        //モデルロードシステムの初期化
        {
            //FBXロードで使う変数をここで初期化
            UINT alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT - 1;
            UINT m_cBSize = (sizeof(Cbuffer) + alignment) & ~alignment;
            UINT m_cBSizeInstance = sizeof(InstanceDraw);

            modelLoadSystem_->Init( DeviceFrom(commandContext), m_cBSize, m_cBSizeInstance, lightDepthBuffer_, mainHeap, cPos_);

        }


        //パイプライン構築
        {
            pipelineManager_->AddBuilder(std::make_unique<Builders::FbxPipelineBuilder>());
            pipelineManager_->AddBuilder(std::make_unique<Builders::PostProcessBuilder>());
            pipelineManager_->AddBuilder(std::make_unique<Builders::DisplacementBuilder>());
			pipelineManager_->AddBuilder(std::make_unique<Builders::PipelineBuilderDebugDraw>());

            //ここでパイプラインを構築する
            pipelineManager_->Build(PipelineSetId::Fbx);
            pipelineManager_->Build(PipelineSetId::PostProcess);
            pipelineManager_->Build(PipelineSetId::Displacement);
			pipelineManager_->Build(PipelineSetId::DebugDraw);
        }

        //モデルレンダーシステムで使う変数などのしょきか
        {
            modelRenderSystem_->Init(commandContext, viewScissor,pipelineManager_->GetRegistry(), computeShaderManager_, mainHeap);
        
        }

		//デバッグ用ワイヤーフレームモデル読み込み
        {
            modelLoadSystem_->LoadModel("Assets/Wireframe.fbx");
        }


        //ポストプロセスクラスの初期化で必要
        {
            postProcessContext_.createResource = createResource_.get();
            postProcessContext_.depth = depthBuffer_;
            postProcessContext_.device = DeviceFrom(commandContext).Get();
            postProcessContext_.dsvHeap_ = dsvHeap_;
            postProcessContext_.lightDepth = lightDepthBuffer_;
            postProcessContext_.loadTex = loadTex_.get();
            postProcessContext_.pipelineManager = pipelineManager_.get();
            postProcessContext_.renderTargets = renderTargets_;
            postProcessContext_.rtvHeaps_ = rtvHeap_;
            postProcessContext_.scissor = ScissorRectFrom(viewScissor);
            postProcessContext_.viewport = ViewportFrom(viewScissor);

            postProcess_->Init(postProcessContext_);
        }


        //テキスト描画用のフォント設定
        {
            //ここで文字の色やフォント、大きさを設定する
            textRenderer_->CreateFontColor("Red", D2D1::ColorF(D2D1::ColorF::Red));
            textRenderer_->CreateFontColor("Blue", D2D1::ColorF(D2D1::ColorF::Blue));

            textRenderer_->CreateFontColor("default", D2D1::ColorF(D2D1::ColorF::White));
            textRenderer_->CreateTextFormat("default", L"Meiryo", DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f);

            textRenderer_->CreateTextFormat("defaultCenter", L"Meiryo", DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16.0f, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);


            textRenderer_->CreateTextFormat("MeiSize24", L"Meiryo", DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.0f);

            textRenderer_->CreateTextFormat("MeiSize24AllCenter", L"Meiryo", DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.0f, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

            textRenderer_->CreateTextFormat("MeiSize24HeightCenter", L"Meiryo", DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.0f, DWRITE_TEXT_ALIGNMENT_LEADING, DWRITE_PARAGRAPH_ALIGNMENT_CENTER);


            textRenderer_->CreateTextFormat("ConvText", L"Meiryo", DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 24.0f);


            textRenderer_->CreateTextFormat("mini", L"Arial", DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 10.0f);

            textRenderer_->CreateTextFormat("font", L"Yu Mincho", DWRITE_FONT_WEIGHT_NORMAL,
                DWRITE_FONT_STYLE_OBLIQUE, DWRITE_FONT_STRETCH_NORMAL, 14.0f);

        }

        //プリミティブ形状登録
        {
			auto& primitiveRendererService = primitiveManager_->GetRendererService();
			auto& primitiveHeapResource = primitiveManager_->GetHeapResource();
			auto& renderInterfaceService = primitiveManager_->GetRenderInterfaceService();
			auto& primitiveInstanceService = primitiveManager_->GetInstanceService();

            //プリミティブ板ポリゴン通常描画用をインスタンス化
            primitiveManager_->RegisterRenderer(
                Primitive::PrimitiveUtility::PrimitiveKind::Normal,
                std::make_unique<Primitive::Quad::NormalPrimitive::PrimitiveNormalQuadRenderer>(
                    primitiveRendererService, primitiveHeapResource, renderInterfaceService, primitiveInstanceService, commandContext, viewScissor)
            );


            //Primitive::PrimitiveUtility::PrimitivePreDrawParam primitivePreDrawParam(DirectX::XMFLOAT4(420, 114, 240, 1.0f));
            //Primitive::PrimitiveUtility::TextureClipParam texTureparam(0, 0, 512, 512, true);
            //primitiveManager_->PreDrawPrimitive(L"Pri", L"Pri", L"PrimitiveTexture/test_image.png", Primitive::PrimitiveUtility::PrimitiveType::QUADPRIMITIVE, primitivePreDrawParam);

        }

        //ライティングクラスの初期化で必要な変数などの初期化
        {
            const auto& postProcessHeap = postProcess_->GetPeraSRVHeap();
            Engine::Render::Lighting::LightManagerContext lightManagerContext(commandContext, postProcessHeap);
            lightManager_->LightManagerInit(createResource_, *primitiveManager_, lightManagerContext);
        }

    }


    // 「搭載 VRAM が最大」のハードウェアアダプターを返す
    ComPtr<IDXGIAdapter4> EngineCore::SelectAdapterWithMostVRAM(IDXGIFactory6* factory)
    {
        ComPtr<IDXGIAdapter1> adapter1;
        ComPtr<IDXGIAdapter4> bestAdapter;
        UINT64 maxVRAM = 0;

        for (UINT i = 0; SUCCEEDED(factory->EnumAdapters1(i, &adapter1)); ++i)
        {
            DXGI_ADAPTER_DESC1 desc{};
            adapter1->GetDesc1(&desc);

            // ソフトウェアアダプターは除外
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                adapter1.Reset();
                continue;
            }

            if (desc.DedicatedVideoMemory > maxVRAM) {
                maxVRAM = desc.DedicatedVideoMemory;
                adapter1.As(&bestAdapter);
            }

            adapter1.Reset();
        }

        const double vramGB = static_cast<double>(maxVRAM) / (1024.0 * 1024.0 * 1024.0);
        printf("使用 GPU RAM: %.2f GB\n", vramGB);

        return bestAdapter;
    }


    void EngineCore::InitializeD3D12(HWND hwnd)
    {
        UINT dxgiFactoryFlags = 0;

#ifdef _DEBUG
        {
            ComPtr<ID3D12Debug> debugLayer;
            if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
                debugLayer->EnableDebugLayer();
                dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
            }
        }
#endif

        ComPtr<IDXGIFactory6> dxgiFactory;
        ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

        ComPtr<IDXGIAdapter4> hardwareAdapter = SelectAdapterWithMostVRAM(dxgiFactory.Get());
        if (!hardwareAdapter)
        {
            ComPtr<IDXGIAdapter1> adapter1;
            ThrowIfFailed(dxgiFactory->EnumAdapters1(0, adapter1.GetAddressOf()));
            ThrowIfFailed(adapter1.As(&hardwareAdapter));
        }

        // Device
        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(DeviceFrom(commandContext).ReleaseAndGetAddressOf())
        ));

		//コマンドシステムの初期化
        {
            commandSystem_ = std::make_unique<Engine::Render::CommandSystem>();
            commandSystem_->Init(DeviceFrom(commandContext).Get(), kFrameCount,true);
            services_.commandSystem = commandSystem_.get();
        }

        // SwapChain
        {
            DXGI_SWAP_CHAIN_DESC1 scDesc{};
            scDesc.BufferCount = kFrameCount;
            scDesc.Width = windowWidth_;
            scDesc.Height = windowHeight_;
            scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            scDesc.SampleDesc.Count = 1;
            scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

            ComPtr<IDXGISwapChain1> sc1;
            ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(
                commandSystem_->GetGraphicsQueue().Get(),
                hwnd,
                &scDesc,
                nullptr,
                nullptr,
                sc1.GetAddressOf()
            ));

            ThrowIfFailed(sc1.As(&swapchain_));

            swapchain_->SetMaximumFrameLatency(1);
            frameLatencyWaitableObject_ = swapchain_->GetFrameLatencyWaitableObject();
        }

        // RTV Heap
        {
            D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
            rtvDesc.NumDescriptors = kFrameCount;
            rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

            ThrowIfFailed(DeviceFrom(commandContext)->CreateDescriptorHeap(
                &rtvDesc, IID_PPV_ARGS(rtvHeap_.ReleaseAndGetAddressOf())
            ));

            rtvDescriptorSize_ = DeviceFrom(commandContext)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        }

        // BackBuffers -> RTV
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeap_->GetCPUDescriptorHandleForHeapStart());

            for (UINT i = 0; i < kFrameCount; ++i)
            {
                ThrowIfFailed(swapchain_->GetBuffer(i, IID_PPV_ARGS(renderTargets_[i].ReleaseAndGetAddressOf())));
                DeviceFrom(commandContext)->CreateRenderTargetView(renderTargets_[i].Get(), nullptr, rtvHandle);
                rtvHandle.Offset(1, rtvDescriptorSize_);
            }
        }

        //リソース生成クラスだけここで初期化してしまう
        createResource_ = std::make_unique<CreateResource>(DeviceFrom(commandContext), rtvHeap_);
        // Depth
        CreateDepthHeap();
        CreateDepthBuffer();
        CreateShadowDepthBuffer();
        CreateDepthResourceView();

        //メインで使うヒープの作成
        CreateCbvSrvUavHeap();

    }

    void EngineCore::CreateDepthHeap()
    {
        //深度バッファヒープ
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 2;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(DeviceFrom(commandContext)->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHeap_.ReleaseAndGetAddressOf())));

    }

    void EngineCore::CreateDepthBuffer()
    {
        // Depth resource
        D3D12_RESOURCE_DESC depthDesc{};
        depthDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthDesc.Width = windowWidth_;
        depthDesc.Height = windowHeight_;
        depthDesc.DepthOrArraySize = 1;
        depthDesc.MipLevels = 1;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        depthDesc.Format = DXGI_FORMAT_R32_TYPELESS;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Format = DXGI_FORMAT_D32_FLOAT;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        ThrowIfFailed(DeviceFrom(commandContext)->CreateCommittedResource(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &depthDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(depthBuffer_.ReleaseAndGetAddressOf())
        ));


    }

    void EngineCore::CreateShadowDepthBuffer()
    {
        //深度バッファー生成
        D3D12_RESOURCE_DESC depthResDesc = {};
        depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthResDesc.Width = windowWidth_;
        depthResDesc.Height = windowHeight_;
        depthResDesc.DepthOrArraySize = 1;
        depthResDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        depthResDesc.SampleDesc.Count = 1;
        depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        depthResDesc.MipLevels = 1;
        depthResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthResDesc.Alignment = 0;
        auto depthHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        // クリアバリューの設定
        D3D12_CLEAR_VALUE _depthClearValue = {};
        _depthClearValue.DepthStencil.Depth = 1.0f;      //深さ１(最大値)でクリア
        _depthClearValue.Format = DXGI_FORMAT_D32_FLOAT; //32bit深度値としてクリア

        //ライトデプスバッファ作成
        depthResDesc.Width = shadow_difinition; //ライトデプス幅
        depthResDesc.Height = shadow_difinition; //ライトデプス高

        ThrowIfFailed(DeviceFrom(commandContext)->CreateCommittedResource(
            &depthHeapProp,
            D3D12_HEAP_FLAG_NONE,
            &depthResDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &_depthClearValue,
            IID_PPV_ARGS(lightDepthBuffer_.ReleaseAndGetAddressOf())
        ), L"\nライトデプスバッファの作成に失敗しました");



    }

    void EngineCore::CreateDepthResourceView()
    {
        auto handle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
        // 深度バッファービュー作成
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        //DSVビューの作成
        //通常のデプス
        createResource_->CreatCustomeDepthStencilView(depthBuffer_, dsvDesc, handle);
        //ライトデプス
        createResource_->CreatCustomeDepthStencilView(lightDepthBuffer_, dsvDesc, handle);

    }

    void EngineCore::CreateCbvSrvUavHeap()
    {
        //コンスタントバッファビューとテクスチャビューのヒープ
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.NumDescriptors = MaxFbxType * kMaxPartsMesh * (ALL_NUM_CBV_SRV);	//ロードするFBXファイルの数だけディスクリプタ作成
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(DeviceFrom(commandContext)->CreateDescriptorHeap(&desc, IID_PPV_ARGS(mainHeap.ReleaseAndGetAddressOf())));

    }


    //終了処理 
    void EngineCore::OnDestroy()
    {
		//CPUとGPUの同期を取るため、コマンドキューをフラッシュしておく
        if (commandSystem_) {
            commandSystem_->GetGraphicsQueue().Flush();
            commandSystem_->GetComputeQueue().Flush();
        }

		//HANDLEの開放を行う
        if (frameLatencyWaitableObject_) {
            CloseHandle(frameLatencyWaitableObject_);
            frameLatencyWaitableObject_ = nullptr;
        }

    }

    void EngineCore::BeginFrame()
    {

        if (frameLatencyWaitableObject_)
        {
            WaitForSingleObjectEx(frameLatencyWaitableObject_, 1000, TRUE);
        }

        commandContext.frameIndex_ = swapchain_->GetCurrentBackBufferIndex();

		//Computeはコンピュートシェーダマネージャクラスで管理することにしたので、ここではGraphicsコマンドリストだけセットする
        //// Compute を使うなら
        //auto c = commandSystem_->BeginCompute(fi);
        //commandContext.computeList = c.cl;

        //// ComputeQueue を使う設定ならそれを、使わないなら GraphicsQueue を入れておく
        //commandContext.computeQueue =
        //    commandSystem_->UsesComputeQueue()
        //    ? commandSystem_->GetComputeQueue().Get()
        //    : commandSystem_->GetGraphicsQueue().Get();
        /////////////////////////////////////////////////////////////////////////////////////////////////////

        // Graphics も同様に
        auto g = commandSystem_->BeginGraphics(commandContext.frameIndex_);
        commandContext.graphicsList = g.cl;
        commandContext.graphicsQueue = commandSystem_->GetGraphicsQueue().Get();
    }

    bool EngineCore::Clear()
    {
        //バックバッファのインデックスを取得する
        //スワップチェーンのセット処理


        Barrier(GraphicsListFrom(commandContext), renderTargets_[commandContext.frameIndex_],
            D3D12_RESOURCE_STATE_PRESENT,
            D3D12_RESOURCE_STATE_RENDER_TARGET);

        auto rtvHeapPointer = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
        rtvHeapPointer.ptr += commandContext.frameIndex_ * DeviceFrom(commandContext)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        GraphicsListFrom(commandContext)->OMSetRenderTargets(1, &rtvHeapPointer, false, nullptr);

        //クリアカラー
        float clsClr[4] = { 0.0f,0.0f,0.0f,0.0f };
        GraphicsListFrom(commandContext)->ClearRenderTargetView(rtvHeapPointer, clsClr, 0, nullptr);

        return true;
    }

    void EngineCore::Flip()
    {

        //バックバッファのインデックスを取得する
        //スワップチェーンのセット処理
        //コマンドリストのリセット
        frameFenceValues_[commandContext.frameIndex_] = commandSystem_->ExecuteGraphicsOnly(commandContext.frameIndex_);

        commandSystem_->GetComputeQueue().Flush();
        commandSystem_->GetGraphicsQueue().Flush();

        //ここでDirectWrite（D2D）実行
        Services().textRenderer->RenderTextFrame();


        auto result = swapchain_->Present(0, 0);
        UpdateFrameRate();
        ThrowIfFailed(result, L"\nFlip()が失敗しました");
    }

    void EngineCore::UpdateFrameRate()
    {
        frameRate_.frameCount++;

        auto now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed = now - frameRate_.prevTime;

        if (elapsed.count() >= 1.0)
        {
            frameRate_.fps = frameRate_.frameCount / static_cast<float>(elapsed.count());
            frameRate_.frameCount = 0;
            frameRate_.prevTime = now;
        }
    }

    void EngineCore::PreDrawShadow()
    {
        auto dsvHandle = dsvHeap_->GetCPUDescriptorHandleForHeapStart();
        dsvHandle.ptr += DeviceFrom(commandContext)->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        D3D12_VIEWPORT vp = CD3DX12_VIEWPORT(0.0f, 0.0f, shadow_difinition, shadow_difinition);
        CD3DX12_RECT rc(0, 0, shadow_difinition, shadow_difinition);


        //ライト深度
        GraphicsListFrom(commandContext)->OMSetRenderTargets(0, nullptr, false, &dsvHandle);

        GraphicsListFrom(commandContext)->ClearDepthStencilView(dsvHandle,
            D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);


        //カメラやライトなど初期化
        //SetupCameraAndLight();
    }

    void EngineCore::DebugWireframeDraw()
    {
        debugDrawManager_->RenderAll(L"Assets/Wireframe.fbx", viewScissor);
    }


    void EngineCore::BindCommandContext()
    {
        commandContext.graphicsQueue = commandSystem_->GetGraphicsQueue().Get();
        commandContext.computeQueue = commandSystem_->UsesComputeQueue()
            ? commandSystem_->GetComputeQueue().Get()
            : commandSystem_->GetGraphicsQueue().Get();

        //記録開始はしない。単に参照を入れるだけ。
        commandContext.graphicsList = commandSystem_->GetGraphicsList().Get(); // getterを用意する想定
        commandContext.computeList = commandSystem_->GetComputeList().Get();

        commandContext.frameIndex_ = swapchain_->GetCurrentBackBufferIndex();

    }
}

