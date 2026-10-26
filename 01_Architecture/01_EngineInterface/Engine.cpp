#include"Engine.h"
#include"EngineCore.h"
#include "Engine/EngineAPI.h"
#include"Engine/IGame.h"
#include"TpsCameraController.h"

using namespace DirectX;

namespace Engine
{
	Engine::Engine(const EngineConfig& cfg)
		:cfg_(cfg)
	{
		//エンジンコンフィグをコア用のコンフィグに変換する
		EngineCoreConfig coreCfg{
			cfg_.width,
			cfg_.height,
			cfg_.title,
			cfg_.pyRoot,
			cfg_.pySitePackages
		};

		core_ = std::make_unique<EngineCore>(coreCfg);
	}

	Engine::~Engine()
	{

	}

	void Engine::Init(IGame& game, HWND& hwnd)
	{
		//エンジンの初期化
		core_->Init(hwnd);


		InitFrameRenderContextDefaults();
		//frameRenderContext.shader
		//engine.Services().modelRenderer->UpdateFrameRenderContext();

		//ランドスケープのコンスタントバッファ更新（初期化）
		core_->Services().displacementMapping->SetConstantBuffer(0.0f, 0.0f, 0.0f, 100, frameRenderContext_.shader->view, frameRenderContext_.shader->proj, *frameRenderContext_.shader);


		//ゲームの初期化
		game.OnInit(*this);

		//デバッグドローの視錐台用ビューとプロジェクション行列更新
		core_->Services().debugDrawManager->SetDebugDrawConstant(EColliderShape::FRUSTUM, DebugDrawMgr::DebugDrawConstant{
			frameRenderContext_.shader->view,
			frameRenderContext_.shader->proj
			});

		//デバッグドローの視錐台描画
		core_->Services().debugDrawManager->PreDrawWireframe(EColliderShape::FRUSTUM);

	}

	void Engine::OnDestroy(IGame& game)
	{
		game.OnShutdown();
		core_->OnDestroy();
	}

	void Engine::Update(IGame& game)
	{





		//デルタタイムを求める
		float deltaTime = core_->Services().gameTimer->Tick();
		//ゲーム内時間のデルタタイムも進める
		core_->Services().timeScaleManager->Tick(deltaTime);
		UpdateFrameContext(deltaTime);

		//ゲームの更新処理
		game.OnUpdate(*this, frameContext_);

		//座標更新処理
		UpdateInstanceData();

		//カメラの更新をする
		UpdateGameCamera(deltaTime);

		//ライトの更新処理
		core_->Services().lightManager->UpdateLightBuffer();
	}

	void Engine::RenderFrame(IGame& game)
	{

		//影描画処理
		{
			core_->PreDrawShadow();

			if (GetDebugViewMode() != Render::DebugViewMode::WithoutShadow) {
				//モデルの影描画更新処理
				core_->Services().modelRenderer->Render(true);
			}
			//ランドスケープ影描画
			core_->Services().displacementMapping->DrawLandScape(true);


		}


		//G-バッファー生成処理
		{
			//dxApp->PreDrawPera1();
			core_->Services().postProcess->BeginGBuffer(core_->GetGraphicsListFrom(), core_->GetPostProcessContext());

			//ランドスケープ描画
			core_->Services().displacementMapping->DrawLandScape();

			//モデルの描画更新処理
			core_->Services().modelRenderer->Render();

			//デバッグ描画処理
			if (GetDebugViewMode() == Render::DebugViewMode::Debug) {
				core_->DebugWireframeDraw();
			}

			core_->Services().postProcess->EndGBuffer(core_->GetGraphicsListFrom());

		}

		//ポストプロセス生成処理
		{
			core_->Services().postProcess->Execute(core_->GetGraphicsListFrom(), core_->GetPostProcessContext());
		}


		//プリミティブ描画
		{
			core_->Services().primitive->PrimitiveRender();

		}

		//ここで、ポストプロセスの合成結果などで描画
		{
			core_->Clear();

			//ポストプロセス最終合成
			core_->Services().postProcess->DrawFinalComposite(core_->GetGraphicsListFrom(), core_->GetPostProcessContext());

			if (GetDebugViewMode() == Render::DebugViewMode::Final) {
				//UI描画処理
				core_->Services().uiManager->DrawUI();
			}

			//ゲームの描画処理
			//ここで文字の描画も行っちゃう
			core_->Flip();
		}

	}

	void Engine::UpdateInput()
	{
		//マウス・キーボード入力更新処理
		core_->Services().mouse->SetKeyCooperative();
		core_->Services().mouse->UpDateInput();
		core_->Services().key->SetKeyCooperative();
	}

	void Engine::RunFrame(IGame& game)
	{
		//コマンドリストのリセットや、RTVとDSVのセット、クリアなどのフレーム開始前の処理
		core_->BeginFrame();

		//マウスやキー入力などの更新処理
		UpdateInput();

		//UIの更新は描画モードがノーマルのときのみ更新する
		if (GetDebugViewMode() == Render::DebugViewMode::Final) {
			//UI更新処理
			core_->Services().uiManager->UpdateUI();
		}

		//ゲームの更新処理
		Update(game);


		//シェーダーに渡す値の更新ろレンダークラスに渡す
		UpdateShaderInfoForFrame();


		//ランドスケープのコンスタントバッファ更新
		core_->Services().displacementMapping->SetConstantBuffer(0.0f, 0.0f, 0.0f, 100, shaderInfo_.view, shaderInfo_.proj, shaderInfo_, GetDebugViewMode());

		//デバッグドローの視錐台用ビューとプロジェクション行列更新
		core_->Services().debugDrawManager->UpdateDebugDrawConstantAll(DebugDrawMgr::DebugDrawConstant{
			shaderInfo_.view,
			shaderInfo_.proj
			});

		if (GetDebugViewMode() == Render::DebugViewMode::Debug) {
			//デバッグ描画の更新
			core_->Services().debugDrawManager->UpdateDrawInstance();
		}

		//SSAO用定数バッファ更新処理
		UpdateSsaoConstantForFrame();

		//DeferredRendering用定数バッファ更新処理
		UpdateDeferredRenderingForFrame();



		//プリミティブ描画のレンダラーが１つ以上ある場合
		if (core_->Services().primitive->GetRendererCount() != 0) {
			//プリミティブ描画で使用する定数バッファ更新
			core_->Services().primitive->UpdateCBBuffer(XMMatrixTranspose(shaderInfo_.view), XMMatrixTranspose(shaderInfo_.proj));

			//プリミティブインスタンス更新
			core_->Services().primitive->PrimitiveUpdate();
		}

		//アニメーションの更新処理
		{
			auto& batches = core_->Services().fbxInstance->GetBatches();
			float deltTime = frameContext_.simulationDeltaTime;
			core_->Services().animationController->UpdateBatch(batches, deltTime);
		}

		//モデルのインスタンスデータ更新処理
		{
			//モデルのDirtyListを取得
			auto objDirtyList = core_->Services().transform->ConsumeDirtyTransforms();
			//アニメーションのDirtyListを取得
			auto animDirtyList = core_->Services().animationController->ConsumeAnimDirty();
			//描画用のインスタンスデータ更新
			core_->Services().modelRenderer->UpdateInstanceData(objDirtyList,animDirtyList);
		}
		

		{
			//ゲームの描画処理
			RenderFrame(game);
		}

	}

	unsigned int Engine::GetWindowWidth()
	{
		return core_->GetWindowWidth();
	}

	unsigned int Engine::GetWindowHeight()
	{
		return core_->GetWindowHeight();

	}

	float Engine::GetAspectRatio()
	{
		return static_cast<float>(GetWindowWidth()) / static_cast<float>(GetWindowHeight());
	}

	EngineServices& Engine::Services()
	{
		return core_->Services();
	}

	const EngineServices& Engine::Services() const
	{
		return core_->Services();
	}

	void Engine::InitFrameRenderContextDefaults()
	{
		//初期のカメラ設定　
		core_->Services().camera->RegisterCamera(L"Basic", std::make_unique<Camera::TpsCameraController>());
		core_->Services().camera->SetActive(L"Basic");

		const std::wstring& mainCameraName = core_->Services().camera->GetMainCameraName();
		core_->Services().camera->GetViewProjection(
			mainCameraName,
			static_cast<float>(GetWindowWidth()) / static_cast<float>(GetWindowHeight())
		);

		auto& mainCamera = core_->Services().camera->GetCameraData()[mainCameraName].cameraInfo;
		auto& mainCamViewProj = core_->Services().camera->GetCameraData()[mainCameraName].viewProj;

		//View / Proj
		shaderInfo_.view = mainCamViewProj.view;
		shaderInfo_.proj = mainCamViewProj.proj;

		XMVECTOR det;
		shaderInfo_.invProj = XMMatrixInverse(&det, shaderInfo_.view * shaderInfo_.proj);


		////視錐台
		//fbxRender->ExtractFrustumPlanes(shaderInfo.view * shaderInfo.proj);

		//環境色など
		shaderInfo_.groundColor = XMFLOAT4(0.7f, 0.5f, 0.3f, 0.0f);
		shaderInfo_.skyColor = XMFLOAT4(0.7f, 0.8f, 0.95f, 0.0f);
		shaderInfo_.groundNormal = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);

		//ライト
		shaderInfo_.lightDir = XMFLOAT4(1, 1, -1, 0);
		shaderInfo_.lightColor = XMFLOAT4(0.8f, 0.8f, 0.8f, 0);

		//基本
		shaderInfo_.world = XMMatrixIdentity();

		//シャドウ解像度
		shaderInfo_.shadowResolution = shadow_difinition;

		frameRenderContext_.camera = &mainCamera;
		frameRenderContext_.shader = &shaderInfo_;

		core_->Services().modelRenderer->UpdateFrameRenderContext(frameRenderContext_);

	}

	void Engine::UpdateShaderInfoForFrame()
	{

		const std::wstring& mainCameraName = core_->Services().camera->GetMainCameraName();
		core_->Services().camera->GetViewProjection(
			mainCameraName,
			static_cast<float>(GetWindowWidth()) / static_cast<float>(GetWindowHeight())
		);



		auto& mainCamera = core_->Services().camera->GetCameraData()[mainCameraName].cameraInfo;
		auto& mainCamViewProj = core_->Services().camera->GetCameraData()[mainCameraName].viewProj;

		shaderInfo_.view = mainCamViewProj.view;
		shaderInfo_.proj = mainCamViewProj.proj;
		XMVECTOR det;
		shaderInfo_.invProj = XMMatrixInverse(&det, shaderInfo_.view * shaderInfo_.proj);


		shaderInfo_.eye = XMFLOAT4(mainCamera.eye.x, mainCamera.eye.y, mainCamera.eye.z, 0);

		// ライトカメラ行列（シャドウ用）
		auto eyePos = XMLoadFloat3(&mainCamera.eye);
		auto upVec = XMVectorSet(0, 1, 0, 0);

		auto ligVec = XMVector4Normalize(XMLoadFloat4(&shaderInfo_.lightDir));
		auto tar = XMFLOAT3(mainCamera.at.x, mainCamera.at.y, mainCamera.at.z);
		auto targetPos = XMLoadFloat3(&tar);

		float shadowDistance = 100.0f;
		auto lightPos = XMVectorMultiplyAdd(ligVec, XMVectorReplicate(shadowDistance), targetPos);

		shaderInfo_.lightCamera =
			XMMatrixLookAtLH(lightPos, targetPos, upVec) *
			XMMatrixOrthographicLH(200, 200, 1, 200);

		shaderInfo_.lightCount = core_->Services().lightManager->GetLightCount();

		frameRenderContext_.camera = &mainCamera;
		frameRenderContext_.shader = &shaderInfo_;

		core_->Services().modelRenderer->UpdateFrameRenderContext(frameRenderContext_);

		//ここでフラスタムカリング用の形状を更新する
		core_->Services().frustumCuller->ExtractFrustumPlanes(shaderInfo_.view * shaderInfo_.proj);

	}

	void Engine::UpdateSsaoConstantForFrame()
	{
		SsaoInfo ssaoInfo;

		ssaoInfo.proj = (shaderInfo_.proj);

		ssaoInfo.invProj = (shaderInfo_.invProj);
		ssaoInfo.view = (shaderInfo_.view);
		ssaoInfo.flagTest = 1;
		//ポストプロセスで使う定数バッファ更新
		core_->Services().postProcess->UpdateConstantBuffer(Render::PostProcess::PostProcessCBId::Ssao, &ssaoInfo, sizeof(ssaoInfo));
	}

	void Engine::UpdateDeferredRenderingForFrame()
	{
		const std::wstring& mainCameraName = core_->Services().camera->GetMainCameraName();
		auto& mainCamera = core_->Services().camera->GetCameraData()[mainCameraName].cameraInfo;
		auto& mainCamViewProj = core_->Services().camera->GetCameraData()[mainCameraName].viewProj;


		DeferredRendering deferredRendering;

		XMVECTOR det;

		//ディファードレンダリングコンスタントバッファ用の値渡してる
		deferredRendering.dirLight = shaderInfo_.lightDir;
		deferredRendering.lightColor = shaderInfo_.lightColor;
		deferredRendering.vEye = XMFLOAT4(mainCamera.eye.x, mainCamera.eye.y, mainCamera.eye.z, 1.0f);
		deferredRendering.groundColor = shaderInfo_.groundColor;
		deferredRendering.groundNormal = shaderInfo_.groundNormal;
		deferredRendering.skyColor = shaderInfo_.skyColor;
		deferredRendering.invProj = (XMMatrixInverse(&det, shaderInfo_.view * shaderInfo_.proj)); //逆プロジェクション行列は転置しなくていい
		deferredRendering.lightCamera = XMMatrixTranspose(shaderInfo_.lightCamera);
		deferredRendering.lightCount = shaderInfo_.lightCount;
		//ここでディファードレンダリング用コンスタントバッファを渡す
		core_->Services().postProcess->UpdateConstantBuffer(Render::PostProcess::PostProcessCBId::Deferred, &deferredRendering, sizeof(deferredRendering));

		PostProcessDebugCB postProcessDebug;
		postProcessDebug.isDebugMode = static_cast<int>(GetDebugViewMode());
		core_->Services().postProcess->UpdateConstantBuffer(Render::PostProcess::PostProcessCBId::PostProcessDebug, &postProcessDebug, sizeof(postProcessDebug));
	}

	void Engine::UpdateInstanceData()
	{
		//ここでdirtyフラグ立ったものの座標をすべて更新する
		core_->Services().transform->UpdateWorldAll();

		core_->Services().fbxInstance->RebuildIndex();
	}

	CommandContext& Engine::GetCommandContext()
	{
		return core_->GetCommandContext();
	}

	const CommandContext& Engine::GetCommandContext() const
	{
		return core_->GetCommandContext();
	}

	ViewportScissor& Engine::GetViewScissor()
	{
		return core_->GetViewScissor();
	}

	const ViewportScissor& Engine::GetViewScissor() const
	{
		return core_->GetViewScissor();
	}

	void Engine::UpdateFrameContext(float deltaTime)
	{
		frameContext_.time.deltaTime = deltaTime;
		frameContext_.viewScissor = GetViewScissor();
		frameContext_.viewProjection =
			Services().camera->GetCameraData()
			[Services().camera->GetMainCameraName()].viewProj;
		//初期値としてゲーム内経過時間をデルタタイムにする
		frameContext_.simulationDeltaTime = deltaTime;
	}

	void Engine::UpdateGameCamera(float deltaTime)
	{
		auto& mouse = *core_->Services().mouse;

		//更新データをカメラの更新関数へ反映
		Camera::CameraUpdateContext cameraUpdateContext{};
		cameraUpdateContext.deltaTime = deltaTime;
		cameraUpdateContext.targetPosition = core_->Services().camera->GetCameraTargetPosition();
		core_->Services().camera->Update(*core_->Services().mouse, *core_->Services().key, cameraUpdateContext);
	}

}