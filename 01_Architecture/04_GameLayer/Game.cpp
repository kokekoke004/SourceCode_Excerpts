#include"Game.h"
#include"Engine/Engine.h"
#include"Engine/EngineAPI.h"
#include<string>
#include"PerlinNoiseStrategy.h"
#include"ObjectInstantiator.h"
#include"UIInteraction.h"

#include"GamePlayScene.h"
#include"TpsCameraController.h"


using namespace std;
using namespace DirectX;

namespace Game
{

	void Game::OnInit(Engine::Engine& engine)
	{

				//シーン管理クラスにゲームプレイシーンを登録と切り替え
		{
			engine.Services().sceneManager->RegisterScene(L"GamePlay", engine.Services().sceneManager->MakeSceneFactory<Scene::GameplayScene>(std::ref(engine)));
			//シーン即切り替え
			engine.Services().sceneManager->ChangeSceneNow(L"GamePlay");
			////シーン初期化
			//engine.Services().sceneManager->GetScene(SceneID::Gameplay)->Initialize();
			////現在のシーンをゲームプレイシーンに変更
			//engine.Services().sceneManager->ChangeScene(SceneID::Gameplay);
		}


		//ゲームの初期化関連
		engine.Services().modelLoader->LoadModel("Assets/hatunemiku3.fbx");
		engine.Services().modelLoader->LoadModel("Assets/BirchTree_1.fbx");
		engine.Services().modelLoader->LoadModel("Assets/commonTree1.fbx");
		engine.Services().modelLoader->LoadModel("Assets/commonTree1_LOD2.fbx");
		engine.Services().modelLoader->LoadModel("Assets/Rock_2.fbx");
		engine.Services().modelLoader->LoadModel("Assets/Grass_2.fbx");

		//std::string modelPath;
		//modelPath = "Assets/Research";

		//for (int i = 1; i <= 20; i++) {
		//	string animPath;
		//	animPath = modelPath + "/" + "CasualMan_walk_" + to_string(i) + ".fbx";
		//	engine.Services().modelLoader->LoadModel(animPath.c_str());
		//}

		//UI初期化
		{
			//UIヒープ、パイプラインをマネジャクラスから取得
			auto uiHeap = engine.Services().uiManager->GetUIHeap();
			auto& uiHeapIdx = engine.Services().uiManager->GetHiHeapIdx();
			auto uiPipeline = engine.Services().uiManager->GetUIPipeline();
			auto viewportScissor = engine.GetViewScissor();
			//UI基底クラスに渡すサービス群の生成
			//UIService uiservice(*engine.Services().textRenderer, *engine.Services().mouse);
			//UIContext uiContext(engine.GetCommandContext(), uiHeap, uiHeapIdx);
			//UI画面の登録
			//engine.Services().uiManager->AddScreen(UIScreenID::Interction, std::make_unique<UIInteraction>(uiservice, uiContext, viewportScissor, uiPipeline));
			//screens[UIScreenID::Interction] = std::make_unique<UIInteraction>(directWrite2D, mouseInput, commandContext, viewportScissor, uiPipeline, uiHeap, uiHeapIdx);
			//screens[UIScreenID::Conversation] = std::make_unique<UIConversation>(directWrite2D, mouseInput, textInputManager,screenShot,characterMgrs,commandContext, viewportScissor, uiPipeline, uiHeap, renderTargets_,uiHeapIdx);
			//screens[UIScreenID::BuildSystem] = std::make_unique<UIBuildSystem>(directWrite2D, mouseInput, commandContext, viewportScissor, uiPipeline, uiHeap, uiHeapIdx);
			//engine.Services().uiManager->ChangeScreen(UIScreenID::Interction);
		

		}

		//カメラ初期化
		{
			engine.Services().camera->RegisterCamera(L"main",std::make_unique<Engine::Camera::TpsCameraController>());
			engine.Services().camera->SetActive(L"main");
		}


		//ディスプレイスメントマッピングの適用するテクスチャ設定
		{
			//テクスチャ
			engine.Services().displacementMapping->LoadLandScapeTexture(L"HeightMap/rocky_terrain_02_diff_1024.jpg");
			//テクスチャノーマルマップ
			engine.Services().displacementMapping->LoadLandScapeTexture(L"HeightMap/rocky_terrain_02_normal_1024.png");
			//テクスチャスペキュラマップ
			engine.Services().displacementMapping->LoadLandScapeTexture(L"HeightMap/rocky_terrain_02_diff_1024_specular.png");
			//グリッドテクスチャ
			engine.Services().displacementMapping->LoadLandScapeTexture(L"HeightMap/gridTexture.png");

			engine.Services().displacementMapping->InitDisPlace();
		}


		//コメントアウト全部外せば動くけど、バグってるからとりあえずチャンク関係の処理は一旦コメントアウトしておく
		//チャンク関係の処理
		{
			////ディスプレイスメントマッピングの高さ更新処理
			engine.Services().displacementMapping->CalculateLandScapeHeight(*engine.Services().computeShaderManager);
			////FBXを配置する位置をノイズで決定////////////////////////////////
			////パーリンソンノイズで、木の配置位置決定
			auto strat = std::make_unique<PerlinNoiseStrategy>(*engine.Services().displacementMapping);
			////Instantiator（FBX配置ロジック）の生成
			auto inst = std::make_unique<ObjectInstantiator>(*engine.Services().modelRenderer);

			auto startPos = XMFLOAT3(0, 0, 0);

			engine.Services().chunkManager->GetObjectPlacerManager()->Configure(std::move(strat), std::move(inst));

			engine.Services().navigationSystem->SetObstacle();
			auto& obstacleData = engine.Services().navigationSystem->GetObstacleData();


			engine.Services().chunkManager->SetChunkDatas(engine.Services().displacementMapping->HeightDisplace(), L"Assets/commonTree1.fbx", 140, 180, 5);
			engine.Services().chunkManager->SetChunkDatas(engine.Services().displacementMapping->HeightDisplace(), L"Assets/BirchTree_1.fbx", 140, 180, 5);
			engine.Services().chunkManager->SetChunkDatas(engine.Services().displacementMapping->HeightDisplace(), L"Assets/Rock_2.fbx", 90, 100, 5);
			engine.Services().chunkManager->SetChunkDatas(engine.Services().displacementMapping->HeightDisplace(), L"Assets/Grass_2.fbx", 50, 90, 5);
			//////////////////////////////////////////////////////////////////

		}



		{

		}

	}

	void Game::OnUpdate(Engine::Engine& engine, ::Engine::FrameContext& frameContext)
	{
		//ゲームの入力や移動などの更新処理

		//Engine::FrameRenderContext frameRenderContext;

		//engine.Services().modelRenderer->UpdateFrameRenderContext();

		//engine.Services().chunkManager->Update(engine.Services().computeShaderManager, navigationSystem, collisionAttributeManager->GetCollisionAttributeData(), engine.Services().displacementMapping->HeightDisplace());

		engine.Services().sceneManager->Update(frameContext);


		//engine.Services().navigationSystem->SetObstacle();


		//if (engine.Services().mouse->OnMouseDown(Left)) {
		//	Primitive::PrimitiveUtility::PrimitivePreDrawParam primitivePreDrawParam(XMFLOAT4(420, 118, 230, 1.0f));
		//	Primitive::PrimitiveUtility::TextureClipParam texTureparam(0, 0, 500, 500, true);

		//	engine.Services().primitive->UpdatePreDrawPrimitive(L"Pri", L"Pri", primitivePreDrawParam, true);

		//}


		//if (engine.Services().mouse->OnMouseDown(Right)) {
		//	Primitive::PrimitiveUtility::PrimitivePreDrawParam primitivePreDrawParam(XMFLOAT4(420, 114, 230, 1.0f));
		//	Primitive::PrimitiveUtility::TextureClipParam texTureparam(0, 0, 500, 500, true);

		//	engine.Services().primitive->UpdatePreDrawPrimitive(L"Pri", L"Pri", primitivePreDrawParam, true);

		//}


	}

	void Game::OnRender(Engine::Engine& engine)
	{
		//ゲームの描画処理
	}

	void Game::OnShutdown()
	{
		//ゲームの終了処理

	}

}

