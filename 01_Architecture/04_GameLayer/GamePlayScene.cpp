#include"GamePlayScene.h"
#include"Engine/Engine.h"
#include"Engine/EngineAPI.h"

#include"CombatSystem.h"
#include"DamageData.h"
#include"DamageManager.h"
#include"ItemSystem.h"

#include"CharacterBase.h"
#include"CharacterSystem.h"
#include"CharacterRenderBridge.h"

#include"PlayerController.h"
#include"NpcController.h"

#include"HPBarRenderer.h"
#include"HpBarController.h"
#include"HealthManager.h"

#include"EntityInitializer.h"

#include"PrimitiveHpBar.h"

#include"WorldItemDropSystem.h"
#include"WorldItemDropManager.h"
#include"WorldItemDropVisualSystem.h"

#include"WorldObjectSystem.h"
#include"WorldObjectDestroySystem.h"

#include"ObjectAttributeManager.h"

#include"DropQueryService.h"
#include"ItemDropManager.h"

#include"PrimitiveUtility.h"

#include"UIResourcePanel.h"
#include"ItemMasterIconProvider.h"

#include"GameUIScreenKeys.h"
#include"UIGamePlayScreen.h"

#include"JobType.h"

#include"StorageSystem.h"
#include"BuildingDatabase.h"
#include"BuildRecipeData.h"
#include"BuildObjectData.h"
#include"BuildProgressManager.h"

#include"BuildSystem.h"

#include"UIGamePlayScreenUnderBar.h"
#include"GameTimeSystem.h"
#include"GameTimeSystemUtility.h"

#include"Engine/DebugViewMode.h"
#include"AgricultureSystem.h"
#include"BuildObjectUtility.h"
#include"StorageUtility.h"
#include"Ray.h"
#include"UIGamePlayScreenResidentPanel.h"

namespace Game::Scene
{
	GameplayScene::GameplayScene(Engine::Engine& engine)
		:engine_(&engine)
	{
		//クラスのインスタンス化
		InitializeServices();
		//描画用HPバーの初期化
		InitHpBar();
	}

	GameplayScene::~GameplayScene() = default;

	void GameplayScene::InitializeServices()
	{
		//オブジェクトの属性管理クラスの初期化
		objectAttributeManager_.reset(new Object::ObjectAttributeManager());

		//HP管理クラスの初期化
		healthManager_.reset(new HealthManager());

		Combat::CombatSystemServices combatSystemServices(*healthManager_);
		//ここでコンバットシステムの初期化
		combatSystem_.reset(new Combat::CombatSystem(combatSystemServices));

		Item::Runtime::ItemSystemServices itemSysServices(engine_->Services().displacementMapping->HeightDisplace(), *engine_->Services().collisionSystem);
		//アイテムシステムの初期化
		itemSystem.reset(new Item::Runtime::ItemSystem(itemSysServices));

		Character::CharacterService characterService{
			*engine_->Services().modelRenderer,
			* engine_->Services().modelLoader,
			* engine_->Services().groundingSystem,
			* engine_->Services().transform
		};

		//キャラクターシステムの初期化
		characterSystem_.reset(new Character::CharacterSystem(characterService));
		//エンティティ初期化クラスの初期化
		entityInitializer_.reset(new Init::EntityInitializer());

		HP::Render::HPBarService hpBarService(*healthManager_, *engine_->Services().primitive, *engine_->Services().modelProvider,
			*engine_->Services().transform, *engine_->Services().fbxInstance);
		hPBarRenderer_.reset(new HP::Render::HPBarRenderer(hpBarService));

		//HPバーコントローラー（HPバーの削除で用いる）
		hpBarController_.reset(new Game::UI::HpBarController(*engine_->Services().primitive));

		//ワールドアイテムドロップの管理クラスの初期化
		//worldItemDropManager_.reset(new ::World::Runtime::WorldItemDropManager());

		::World::Runtime::WorldItemDropVisualSystemService worldItemDropVisualSystemService(engine_->Services().primitive->GetPrimitiveRenderer());

		//ワールドアイテムドロップの描画クラスの初期化
		worldItemDropVisualSystem_.reset(new ::World::Runtime::WorldItemDropVisualSystem(worldItemDropVisualSystemService));

		//Game::World::Drop::WorldItemDropSystemServices worldItemDropSystemServices(
		//	*worldItemDropManager_,
		//	*worldItemDropVisualSystem_,
		//	*itemSystem->GetItemMasterManager()
		//);

		//ワールドに実際にアイテムなどをドロップする処理を持つクラス
		//worldItemDropSystem_.reset(new Game::World::Drop::WorldItemDropSystem(worldItemDropSystemServices));

		worldObjectSystem_.reset(new World::WorldObjectSystem());
		worldObjectDestroySystem_.reset(new World::WorldObjectDestroySystem(*engine_->Services().modelRenderer,*worldObjectSystem_));

		//倉庫クラスの初期化
		storageSystem_.reset(new Game::World::Storage::StorageSystem());

		buildingDatabase_.reset(new Build::BuildingDatabase());

		Build::BuildSystemServices buildSystemServices(*engine_->Services().modelRenderer, *engine_->Services().modelProvider, *engine_->Services().mouse, *engine_->Services().key,
			*engine_->Services().camera, *engine_->Services().displacementMapping, *engine_->Services().transform, *worldObjectSystem_,
			*engine_->Services().fbxInstance, *buildingDatabase_, *engine_->Services().collisionSystem);

		buildSystem_.reset(new Build::BuildSystem(buildSystemServices));

		//ゲームタイマーの初期化
		gameTimeSystem_.reset(new Game::Timer::GameTimeSystem(*engine_->Services().timeScaleManager));
		//農業システムの初期化
		agricultureSystem_.reset(new Agriculture::AgricultureSystem(Agriculture::AgricultureSystemServices(*engine_->Services().modelRenderer, objectAttributeManager_->GetObjectAttributeData(), *engine_->Services().transform)));

	}

	void GameplayScene::Initialize()
	{
		//オブジェクトの属性データの読み込み
		objectAttributeManager_->LoadObjectAttribute("ObjectData/ObjectAttributeData.csv");
		//コリジョン特性データの読み込み
		engine_->Services().collisionAttribute->LoadCollisionAttribute("CollisionData/CollisionData.csv");
		//攻撃用ツールデータ読み込み
		combatSystem_->LoadAttackTool("ToolData/AttackToolData.csv");



		//マスターアイテムデータ読み込み処理
		itemSystem->LoadMasterItemData("MasterItemData/MasterItemData.csv",
			Item::Master::LoadItemMasterDataService(
				combatSystem_->GetDamageData(),
				combatSystem_->GetAttackToolList()
			));

		//アイテムのドロップ率などセット
		//テーブルにアイテムドロップIDと対応するコリジョンのIDの情報を一つのテーブルにまとめる
		itemSystem->LoadItemDropData("DropItem/DropItem.csv",
			objectAttributeManager_->GetObjectType());

		//テーブルにアイテムドロップ関連の情報まとめる
		itemSystem->SetItemResolver(
			Item::Drop::Resolver::SetItemResolverData(
				objectAttributeManager_->GetAllData(),
				itemSystem->GetAllItemDropDataRegistry()
			));

		auto chId = characterSystem_->SpawnPlayer(L"主人公", L"Assets/man.fbx", 435, 113, 240);
		auto playerSlot = characterSystem_->GetSlot(chId);
		if (playerSlot) {
			if (auto animTarget = engine_->Services().fbxInstance->TryGetAnimTarget(L"Assets/man.fbx", playerSlot->fbxInstanceHandle.tid)) {
				auto clipID = animTarget->nameToClipId->at(L"Walk");
				//アニメーション再生開始用の初期呼び出し
				engine_->Services().animationController->Play(*animTarget->animState, clipID, true);

			}
		}
		//engine_->Services().fbxInstance
		//engine_->Services().animationController->Play()

		for (int i = 0; i < 6; i++) {
			auto npcID = characterSystem_->SpawnNpc(L"NPC" + std::to_wstring(i), L"Assets/man.fbx", 435 + 2 * float(i), 113, 240, Game::Character::Job::JobType::None);
			auto npcSlot = characterSystem_->GetSlot(npcID);
			if (auto animTarget = engine_->Services().fbxInstance->TryGetAnimTarget(L"Assets/man.fbx", npcSlot->fbxInstanceHandle.tid)) {
				auto clipID = animTarget->nameToClipId->at(L"Walk");
				//アニメーション再生開始用の初期呼び出し
				engine_->Services().animationController->Play(*animTarget->animState, clipID, true);

			}

		}

		//モデルパスとオブジェクトキーの対応表の初期化
		InitModelPathToObjectKey();


		//倉庫のモデル読み込みと配置
		{
			engine_->Services().modelLoader->LoadModel("Assets/Warehouse.fbx");
			engine_->Services().modelLoader->LoadModel("Assets/house1.fbx");
			engine_->Services().modelLoader->LoadModel("Assets/ConstractionModel.fbx");
			engine_->Services().modelLoader->LoadModel("Assets/field.fbx");
			engine_->Services().modelLoader->LoadModel("Assets/Carrot_4.fbx");

			Engine::ModelRenderDesc desc(
				L"Assets/Warehouse.fbx",
				L"Warehouse-test.fbx_0",
				430, 113, 235,
				0.0f, 0.0f, 0.0f,
				1.0f, 1.0f, 1.0f,
				true, false, EntityType::OBJECT, AnimationType::NONE
			);
			auto handle = engine_->Services().modelRenderer->Submit(desc);

			//畑描画テスト
			//{
			//	Engine::ModelRenderDesc descCons(
			//		L"Assets/field.fbx",
			//		L"Assets/field.fbx_0",
			//		420, 115, 235,
			//		0.0f, 0.0f, 0.0f,
			//		5.0f, 5.0f, 5.0f,
			//		true, false, EntityType::OBJECT, AnimationType::NONE
			//	);
			//	auto handleCons = engine_->Services().modelRenderer->Submit(descCons);

			//	auto fieldModel = engine_->Services().modelProvider->GetFromFbxName(L"Assets/field.fbx");

			//	DirectX::XMFLOAT3 cropsScale(3.0f, 3.0f, 3.0f);
			//	//指定されたグリッド数と配置幅をもとに畑に作物を配置していくここでは配置設定のみで描画はしない
			//	agricultureSystem_->RegisterFarm(L"carrot", handleCons.tid, fieldModel->GetVertSize(CollisionRegion::Full), 1.5f, 1.4f,0.3f, cropsScale);

			//}

			auto pos = DirectX::XMFLOAT3(desc.x, desc.y, desc.z);
			engine_->Services().modelRenderer->WrapFBXWithOBB(desc.fbxName, desc.keyDrawFbx, L"WarehouseCollisionTest", handle.tid, pos, 
				EColliderKind::None, CollisionRegion::Lower, DirectX::XMFLOAT3(desc.scaleX, desc.scaleY, desc.scaleZ));
			worldObjectSystem_->RegisterObject(handle.tid, desc.fbxName, desc.keyDrawFbx);

			//倉庫のインベントリを登録する
			auto id = storageSystem_->RegisterStorage(handle.tid,World::Storage::StorageRole::General);
		}

		buildingDatabase_->LoadBuildingData("BuildData/BuildObject.csv");
		buildingDatabase_->LoadBuildingRecipeData("BuildData/BuildRecipe.csv");

		//UIの初期化
		InitializeUI();

		//LODデータの読み込み
		engine_->Services().modelLodManager->LoadModelLodData("LodSetting/LodSetting.csv");

		auto allLodData = engine_->Services().modelLodManager->GetAllModelLodData();
		//engine_->Services().modelLoader->LoadModelLodSet(allLodData);

		//engine_->Services().lightManager->AddLight(DirectX::XMFLOAT4(425, 115, 235, 0.0f), DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 5.0f), DirectX::XMFLOAT4(20.0f, 0.0f, 0.0f, 0.0f));

		engine_->Services().lightManager->AddLight(DirectX::XMFLOAT4(438, 115, 235, 0.0f), DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 5.0f), DirectX::XMFLOAT4(10.0f, 0.0f, 0.0f, 0.0f));

		//ゲーム内タイマーの一日をリアルタイム基準でセットする
		gameTimeSystem_->SetRealMinutesPerGameDay(1);

	}

	void GameplayScene::InitHpBar()
	{


		//プリミティブマネージャにHPバーのレンダラーを登録する処理
		{
			auto primitiveRendererService = engine_->Services().primitive->GetRendererService();
			auto primitiveHeapResource = engine_->Services().primitive->GetHeapResource();
			auto renderInterfaceService = engine_->Services().primitive->GetRenderInterfaceService();
			auto primitiveInstanceService = engine_->Services().primitive->GetInstanceService();

			Primitive::Quad::HpBar::HpBarService hpbarService(*healthManager_);


			engine_->Services().primitive->RegisterRenderer(Primitive::PrimitiveUtility::PrimitiveKind::HP,
				std::make_unique<Primitive::Quad::HpBar::PrimitiveHpBar>(
					primitiveRendererService, primitiveHeapResource, renderInterfaceService, primitiveInstanceService, hpbarService, engine_->GetCommandContext(), engine_->GetViewScissor()));
		}


	}

	void GameplayScene::Update( Engine::FrameContext& frameContext)
	{
		//F1キー入力でデバッグモードの切り替え
		if (engine_->Services().key->TriggerKey(DIK_F1)) {
			engine_->GetDebugViewMode() = Engine::Render::DebugViewMode::Final;
		}
		else if (engine_->Services().key->TriggerKey(DIK_F2)) {
			engine_->GetDebugViewMode() = Engine::Render::DebugViewMode::WithoutUI;
		}
		else if (engine_->Services().key->TriggerKey(DIK_F3)) {
			engine_->GetDebugViewMode() = Engine::Render::DebugViewMode::Debug;
		}
		else if (engine_->Services().key->TriggerKey(DIK_F4)) {
			engine_->GetDebugViewMode() = Engine::Render::DebugViewMode::WithoutShadow;
		}
		else if (engine_->Services().key->TriggerKey(DIK_F5)) {
			engine_->GetDebugViewMode() = Engine::Render::DebugViewMode::WithoutBloom;
		}
		else if (engine_->Services().key->TriggerKey(DIK_F6)) {
			engine_->GetDebugViewMode() = Engine::Render::DebugViewMode::Divide6;
		}

		//ゲーム内時間経過処理
		gameTimeSystem_->Update(frameContext.time.deltaTime);


		//プレイヤーの座標を取得
		DirectX::XMFLOAT3 playerPos;
		followTargetPlayerId_ = 1;
		//キャラクターの更新処理
		{
			frameContext.simulationDeltaTime = gameTimeSystem_->GetSimulationDeltaTime();

			//カメラのターゲット更新
			bool allowPlayerInput = UpdateFollowTarget();

			//キャラクターの更新処理
			UpdateCharacter(frameContext, allowPlayerInput);



			characterSystem_->TryGetLocalPos(followTargetPlayerId_, playerPos);

			//攻撃処理の更新
			UpdateAttack(frameContext);
			//HP0になったオブジェクトの削除処理
			RemoveDeadObjects();

			//アイテムを拾って、インベントリ反映する処理
			//ここではプレイヤーの周囲に落ちているアイテムを拾う処理を入れている
			auto* inventory = characterSystem_->GetInventory(1);
			if (inventory) {
				auto pickUpdata = itemSystem->TryPickupNearest(*inventory, playerPos);
				if (pickUpdata&&pickUpdata->success) {
					printf("\nアイテムを拾ってインベントリに反映しました。");
					printf("\nPicked Up Item : %ls", pickUpdata->item.masterKey.c_str());
					worldItemDropVisualSystem_->EracePrimitive(pickUpdata->item.masterKey, pickUpdata->droppedItemId);

				}
			}

			//チャンクの更新処理
			auto UpdateResult = UpdateChunk(playerPos);
			//HPデータをオブジェクトにセットする処理
			SetHealthData(UpdateResult);



			//デバッグしっかり最後に選択したものが取得できているかどうか////
			auto sel = characterSystem_->GetLastSelection(1);
			if (sel) {
				//printf("\nmodelTargetName : %ls", sel->drawName.c_str());

				//auto chunkKey = engine_->Services().chunkManager->GetCenterChunkKey();
				//auto chunkData = engine_->Services().chunkManager->GetChunkDatas()[chunkKey];

				//printf("\ncenterChunkKey : mapId=%d, cx=%d, cz=%d", chunkKey.mapId, chunkKey.cx, chunkKey.cz);

				//for (auto& data : chunkData.live.drawNames) {
				//	for (auto& name : data.second) {
				//		printf("\nchunkData drawName : %ls", name.c_str());
				//	}
				//}

			}
			/////////////////////////


			//ディスプレイスメントマッピングの更新処理
			UpdateDisplacementMapping();

			if (engine_->Services().key->TriggerKey(DIK_J)) {
				auto mpos = engine_->Services().mouse->GetMousePos();
				printf("\nCLICK");
				printf("\nX :%f , Y :%f", mpos.X, mpos.Y);
			}

		}

		//建築システムの更新処理
		{
			const auto& uiManager = engine_->Services().uiManager;
			const auto& uiKey = uiManager->GetCurrentKey();

			const auto& viewportScissor = engine_->GetViewScissor();

			//カメラのメインカメラ名の取得
			const std::wstring& mainCameraName = engine_->Services().camera->GetMainCameraName();
			//ビルドシステムに渡すコンテキストの生成
			const auto& viewProjection = engine_->Services().camera->GetViewProjection(mainCameraName);

			//UIの更新
			if (Game::UI::ScreenKeys::GamePlay == uiKey) {
				auto* gamePlayScreen = uiManager->GetUIScreenAs<Game::UI::Screens::UIGamePlayScreen>(Game::UI::ScreenKeys::GamePlay);
				//建築常時更新処理
				auto compList = buildSystem_->Update();
				//建築中のオブジェクトにカーソルが重なったかの判定
				const auto& isHoveringUI = engine_->Services().uiManager->IsHoveringUI();
				//建築の進捗の詳細取得
				const auto& buildingProgress = buildSystem_->FindHoveredBuilding(Build::BuildSystemContext(viewportScissor, viewProjection, engine_->Services().gameTimer->GetDeltaTime(), isHoveringUI.isHit));

				//UIゲーム時間に関する更新処理
				{
					//ゲーム内時間をUIへ引き渡す
					gamePlayScreen->SetGametime(gameTimeSystem_->GetGameDateTime());
				}
				
				//建築に関する更新処理
				if (gamePlayScreen) {
					//建築の進捗を送る
					gamePlayScreen->GetBuildingInfo(buildingProgress);

					//UIから選択した建築物のパスを取得する
					const auto& selectedBuildPath = gamePlayScreen->GetSelectedBuildData();
					const auto& currentClickUnderBarButton = gamePlayScreen->GetCurrentClickUnderBarButton();
					const auto& lastClickedUnderBarButtonType = gamePlayScreen->GetLastClickedUnderBarButtonType();

					bool deselectedButton = gamePlayScreen->WasUnderBarButtonDeselected();

					if (currentClickUnderBarButton && currentClickUnderBarButton->buttonType == Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Build) {
						buildSystem_->StartPlacement(selectedBuildPath, playerPos, static_cast<float>(engine_->GetAspectRatio()));
						gamePlayScreen->ClearSelectedBuildData();
					}
					//建築モード以外のボタンをクリックしたとき、もしくは建築モードのボタンをもう一度クリックしたときに、建築モードを終了する
					else if (lastClickedUnderBarButtonType) {
						//建築モードのボタンをもう一度クリックしたときに、建築モードを終了する
						if (*lastClickedUnderBarButtonType == Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Build && deselectedButton) {
							//ビルドモードの終了
							buildSystem_->CancelPlacement();
						}
						//建築モード以外のボタンをクリックしたときに、建築モードを終了する
						else if (*lastClickedUnderBarButtonType != Game::UI::GamePlayScreen::Parts::UnderBarButtonType::Build) {
							//ビルドモードの終了
							buildSystem_->CancelPlacement();
						}
					}
				}

				//ここで、建築完了した物の中に建築物に畑が存在するか走査する
				for (const auto& data : compList) {
					switch (data.buildType) {
						case Game::Build::BuildType::Field:
						{
							const auto model = engine_->Services().modelProvider->GetFromFbxName(data.modelPath);
							if (!model) {
								ThrrowError("モデルが見つかりませんでした");
							}

							DirectX::XMFLOAT3 cropsScale(3.0f, 3.0f, 3.0f);
							agricultureSystem_->RegisterFarm(L"food", data.buildingTid, model->GetVertSize(CollisionRegion::Full), 1.5f, 1.4f, 0.3f, cropsScale);
							//畑オブジェクトには倉庫として作物を持つ（畑からー＞倉庫に運ぶための一時的な倉庫）
							auto storageId = storageSystem_->RegisterStorage(data.buildingTid, World::Storage::StorageRole::ProductionOutput);
							//倉庫割り当て
							agricultureSystem_->SetOutputStorageId(data.buildingTid,storageId);
							break;
						}
						case Game::Build::BuildType::Storage:
						{
							auto storageId = storageSystem_->RegisterStorage(data.buildingTid, World::Storage::StorageRole::General);
							break;
						}

						default:
						{
							break;
						}

					}
				}

			}

			//建築物の配置モードになっている場合、更新の処理を行う
			if (buildSystem_->GetIsPlacing()) {
				const auto& isHoveringUI = engine_->Services().uiManager->IsHoveringUI();
				//ビルドシステムの更新
				buildSystem_->UpdateInBuildMode(Build::BuildSystemContext(viewportScissor, viewProjection, engine_->Services().gameTimer->GetDeltaTime(), isHoveringUI.isHit));
				//更新結果に基づいて、チャンクの更新フラグを立てる
				const auto& currentData = buildSystem_->GetCurrentPlacementData();
				if (currentData.buildState == Game::Build::BuildState::UnderConstruction) {
					const auto& currentPlacementData = buildSystem_->GetCurrentPlacementData();
					const auto& chunkKey = engine_->Services().chunkManager->CalculateChunk(currentPlacementData.position);
					engine_->Services().chunkManager->SetChunkUpdateFlag(chunkKey, true);
				}
			}

		}

		//農業システムの更新処理
		{
			agricultureSystem_->Update(gameTimeSystem_->GetSimulationDeltaTime(),*storageSystem_,*itemSystem);
		}

		//畑の詳細UI関連の処理
		{
			bool click = engine_->Services().mouse->OnMouseDown(MouseButton::Left);
			auto mousePos = engine_->Services().mouse->GetMousePos();
			auto* gamePlayScreen = engine_->Services().uiManager->GetUIScreenAs<Game::UI::Screens::UIGamePlayScreen>(Game::UI::ScreenKeys::GamePlay);
			//カメラのメインカメラ名の取得
			const std::wstring& mainCameraName = engine_->Services().camera->GetMainCameraName();

			const auto& viewportScissor = engine_->GetViewScissor();
			const auto& viewProjection = engine_->Services().camera->GetViewProjection(mainCameraName);
			//レイキャストする
			const auto ray = engine_->Services().camera->ScreenToWorldPos(mousePos, viewportScissor, viewProjection);
			//もしレイと衝突するOBBがあるか探す
			const auto result = engine_->Services().collisionSystem->RaycastAgainstOBBs(ray);

			if (click && !fieldDetailInfo_) {

				if (result) {
					bool hasField = agricultureSystem_->HasFarm(result->tid);

					if (hasField && gamePlayScreen) {
						fieldDetailInfo_ = true;
						//畑の詳細を表示する対象のトランスフォームIDをセットする
						gamePlayScreen->SetTargetField(result->tid);
					}
				}
			}
			//一回畑の詳細パネルを登録してからもう一回クリックしたときに登録情報をクリアする
			else if (click && fieldDetailInfo_) {
				if (gamePlayScreen) {
					fieldDetailInfo_ = false;
					gamePlayScreen->ClearTargetField();
				}
			}

		}


		if (!buildSystem_->GetIsPlacing()) {
			//カメラ処理
			UpdateCameraTarget(playerPos);
		}

	}

	void GameplayScene::InitializeUI()
	{
		//UIヒープ、パイプラインをマネジャクラスから取得
		auto uiHeap = engine_->Services().uiManager->GetUIHeap();
		auto& uiHeapIdx = engine_->Services().uiManager->GetHiHeapIdx();
		auto uiPipeline = engine_->Services().uiManager->GetUIPipeline();
		auto viewportScissor = engine_->GetViewScissor();

		//UI基底クラスに渡すサービス群の生成
		UIService uiservice(*engine_->Services().textRenderer, *engine_->Services().mouse);
		UIContext uiContext(engine_->GetCommandContext(), uiHeap, uiHeapIdx, viewportScissor,uiPipeline);

		//アイコンプロバイダーの初期化
		itemMasterIconProvider_.reset(new Item::Master::ItemMasterIconProvider(itemSystem->GetItemTable()));

		auto uiGamePlayScreen = std::make_unique<Game::UI::Screens::UIGamePlayScreen>(
			uiservice,
			uiContext,
			uiPipeline,
			viewportScissor
		);

		//ここで、渡すインベントリは、倉庫とりあえず1を指定している
		uiGamePlayScreen->CreateParts(*itemMasterIconProvider_, *storageSystem_->GetInventory(1), *characterSystem_,
			*objectAttributeManager_, *buildingDatabase_,gameTimeSystem_->GetGameSpeedController(),*agricultureSystem_, *storageSystem_);

		//UI画面の登録
		//デバグとしてとりあえず、プレイヤーのインベントリクラスを渡している
		//今後、倉庫のインベントリを渡すようにする
		engine_->Services().uiManager->AddScreen(Game::UI::ScreenKeys::GamePlay, std::move(uiGamePlayScreen));
		//UI即切り替え
		engine_->Services().uiManager->ChangeScreen(Game::UI::ScreenKeys::GamePlay);

	}

	void GameplayScene::Draw()
	{

	}

	void GameplayScene::Finalize()
	{

	}

	Engine::FrameContext GameplayScene::BuildFrameContext(float deltaTime)
	{
		Engine::FrameContext frameContext{};
		frameContext.time.deltaTime = deltaTime;
		frameContext.viewScissor = engine_->GetViewScissor();
		frameContext.viewProjection =
			engine_->Services().camera->GetCameraData()
			[engine_->Services().camera->GetMainCameraName()].viewProj;
		frameContext.simulationDeltaTime = gameTimeSystem_->GetSimulationDeltaTime();
		return frameContext;
	}


	void GameplayScene::UpdateCharacter(const Engine::FrameContext& frameContext, bool allowPlayerInput)
	{
		//キャラクター更新で使うクラスをここで渡す
		characterEngineServices_ = Character::CharacterEngineServices(engine_->Services());

		Character::CharacterGameServices characterGameServices(*combatSystem_,*worldObjectSystem_,*itemSystem,itemSystem->GetWorldItemDropManager(),
			*worldItemDropVisualSystem_, *storageSystem_,buildSystem_->GetBuildProgressManager(), buildSystem_->GetBuildPlacementController(),*gameTimeSystem_,*agricultureSystem_,*objectAttributeManager_);


		Character::CharacterUpdateContext characterUpdateContext(frameContext, characterEngineServices_, characterGameServices, gameTimeSystem_->GetSimulationDeltaTime(), allowPlayerInput);


		//キャラクター更新
		characterSystem_->UpdateAll(characterUpdateContext);

	}

	ChunkUpdateResult GameplayScene::UpdateChunk(DirectX::XMFLOAT3 playerPos)
	{
		//チャンクマネージャクラス更新
		auto chunkUpdateResult = engine_->Services().chunkManager->Update(*engine_->Services().computeShaderManager, *engine_->Services().navigationSystem, playerPos,
			engine_->Services().displacementMapping->HeightDisplace());

		engine_->Services().chunkManager->UpdateObstacleMapDataAllChunk();

		//チャンクマネージャの更新結果に基づいて、ワールドオブジェクトシステムにオブジェクトを登録する
		for (const auto& obj : chunkUpdateResult.spawnedInstances)
		{
			worldObjectSystem_->RegisterObject(obj.tid, obj.modelPath, obj.drawName);
		}

		return chunkUpdateResult;
	}

	void GameplayScene::UpdateDisplacementMapping()
	{
		//ディスプレイスマッピングクラスにチャンクマネージャの中心チャンクの情報をセットする
		engine_->Services().displacementMapping->SetChunkSenter(*engine_->Services().chunkManager);

		auto allChunkObstacleData = engine_->Services().chunkManager->GetObstacleDataAllChunk().size();

		//障害物データがある場合はインスタンスバッファ作成して、データを登録する
		if (allChunkObstacleData != 0) {
			//リソース作成
			engine_->Services().displacementMapping->CreateDisplacementIstanceBuffer(L"obstacle", sizeof(UINT), allChunkObstacleData);
			//インスタンスバッファビューの作成
			engine_->Services().displacementMapping->CreateDisplacementIstanceBufferView(L"obstacle", sizeof(UINT), Displacement::DISPLACEMENT_NUM_ALL - 1);

			//インスタンスバッファに障害物情報を登録
			//displacementMapping->UpdateObstacleMapDataAllChunk(L"obstacle", chunkManager);
			engine_->Services().displacementMapping->UpdateDisplacementIstanceBuffer(L"obstacle", engine_->Services().chunkManager->GetObstacleDataAllChunk());
		}

	}

	bool GameplayScene::UpdateFollowTarget()
	{
		const auto& uiManager = engine_->Services().uiManager;
		auto* gamePlayScreen = uiManager->GetUIScreenAs<Game::UI::Screens::UIGamePlayScreen>(Game::UI::ScreenKeys::GamePlay);
		if (gamePlayScreen == nullptr) {
			return true;
		}
		//レシデントパネルで追従リクエストとして登録した物
		if (auto id = gamePlayScreen->ConsumeCameraFollowRequest())
		{
			//キャラクタースロットを取得
			auto characterData = characterSystem_->GetSlot(*id);
			if (characterData == nullptr) {
				return false;
			}
			followTargetId_ = *id;
			return false;

		}

		//追従解除、UI外の左クリック
		const bool click = engine_->Services().mouse->OnMouseDown(MouseButton::Left);
		const bool hoveringUI = engine_->Services().uiManager->IsHoveringUI().isHit;
		if (followTargetId_ != 0 && click && !hoveringUI) {
			followTargetId_ = 0;
			return false;
		}

		return true;
	}

	void GameplayScene::UpdateCameraTarget(const DirectX::XMFLOAT3& playerPos)
	{
		DirectX::XMFLOAT3 targetPos = playerPos;

		if (followTargetId_ != 0) {
			DirectX::XMFLOAT3 residentPos;
			//ターゲットIDをもとにキャラクターの座標を求めてカメラのターゲットにする
			if (characterSystem_->TryGetLocalPos(followTargetId_, residentPos)) {
				targetPos = residentPos;
			}
			else {
				//住民が消えていたらプレイヤーに戻す
				followTargetId_ = 0; 
			}
		}
		
		engine_->Services().camera->SetCameraTarget(targetPos);
	}


	void GameplayScene::SetHealthData(const ChunkUpdateResult& chunkUpdateResult)
	{

		const auto& chunkObjList = engine_->Services().chunkManager->GetObjectPlacementSettings();
		//チャンクの更新結果に基づいて、ワールドオブジェクトシステムに登録されたオブジェクトのTransformIdをもとに、ヘルスマネージャにHPデータをセットする
		for (auto& spawnData : chunkUpdateResult.spawnedInstances) {
			auto pos = engine_->Services().transform->GetLocal(spawnData.tid);
			auto objKey = worldObjectSystem_->FindObjectKeyByModelPath(spawnData.modelPath);
			auto objAttri = objectAttributeManager_->GetObjectAttributeByKey(*objKey);
			if (objKey && objAttri) {
				Health::HealthInit healthInit{};
				healthInit.fbxName = spawnData.modelPath;
				healthInit.keyDrawFbx = spawnData.drawName;
				healthInit.transformId = spawnData.tid;
				healthInit.pos = pos->pos.xyz;
				healthInit.maxHP = objAttri->maxHP;
				healthInit.hp = objAttri->maxHP;
				//healthInit.keyDrawFbx

				entityInitializer_->InitHealthData(
					*healthManager_,
					healthInit,
					*engine_->Services().collisionAttribute->GetCollisionAttributeData()
				);

			}

		}
	}

	void GameplayScene::UpdateAttack(const Engine::FrameContext& frameContext)
	{
		//キャラクターの戦闘状態を収集する
		combatSystem_->CollectCombatState(*characterSystem_, frameContext);
		//キューに積んだ攻撃リクエストを処理する
		combatSystem_->ProcessAttackRequests();
		//全キャラクターの戦闘状態を収集する
		const auto& chComb = combatSystem_->GetCharacterCombatStates();
		for (const auto& state : chComb) {

			//戦闘に入った瞬間にHPバーを新しくセットする
			if (state.combatFlags.justEnteredCombat){
				//HPバーの描画処理
				//新	しく描画するHPバーの情報をセットする
				hPBarRenderer_->HPBarRender(state.targetInfo.currentModelName, state.targetInfo.currentDrawName);
			}

			hpBarController_->EraceHpBar(Game::UI::HpBarDisplayState{ state.targetInfo.currentTid,state.targetInfo.previousTid,state.targetInfo.currentModelName, state.targetInfo.currentDrawName,state.targetInfo.previousModelName, state.targetInfo.previousDrawName});
		}
	}

	void GameplayScene::RemoveDeadObjects()
	{
		//HPが０になったオブジェクトのリストをヘルスマネージャからもらう
		const auto& list = healthManager_->DrainDeleteObject();
		for (const auto& delObject : list) {
			//ToDo : HP管理クラスの削除リストにTidも持たせるようにして、それをもとにワールドオブジェクトシステムからも削除するようにする
			Game::World::WorldObjectDestroyData destroyData(
				delObject.fbxName,
				delObject.keyDrwFbx,
				delObject.pos
			);
			//ワールドオブジェクトシステムの削除キューに削除リクエストを積む
			worldObjectSystem_->RequestDestroy(destroyData);

			//アイテムドロップキューに積む
			auto objKey = worldObjectSystem_->FindObjectKeyByModelPath(delObject.fbxName);
			if (objKey) {
				
				Item::Drop::Manager::DropRequest dropRequest(*objKey, delObject.pos,Item::Drop::Manager::DropRequestType::ObjectLoot);
				itemSystem->RequstItemDrop(dropRequest);
				
			}
			printf("\nHP0になったオブジェクトを削除キューに積みました。fbxName : %ls, keyDrawFbx : %ls", delObject.fbxName.c_str(), delObject.keyDrwFbx.c_str());
			//モデルの削除処理
			engine_->Services().modelRenderer->RemoveDrawFbx(delObject.fbxName, delObject.keyDrwFbx);
			auto tid = engine_->Services().transform->GetLocal(delObject.transform);
			auto key = engine_->Services().chunkManager->CalculateChunk(delObject.pos);
			//キーに対応するチャンクをアップデート処理する
			engine_->Services().chunkManager->SetChunkUpdateFlag(key, true);
		}

		//ワールドオブジェクトシステムの削除キューに積まれたオブジェクトをすべて破壊する
		worldObjectDestroySystem_->FlushDestroyQueue();

		std::vector<Item::Drop::Manager::DropRequest> dropList;

		//アイテムドロップキューに積まれたドロップ要求をすべて処理する
		itemSystem->FlushDropRequests(&dropList);

		for (auto& dropRequest : dropList) {
			if (!dropRequest.droppedItemId) {
				printf("\nError : droppedItemId is nullopt");
				continue;
			}
			
			//アイテムの描画設定
			Primitive::PrimitiveUtility::PrimitivePreDrawParam primitivePreDrawParam(DirectX::XMFLOAT4(dropRequest.dropPosition.x, dropRequest.dropPosition.y + 0.5f, dropRequest.dropPosition.z, 1.0f));
			primitivePreDrawParam.scale = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
			primitivePreDrawParam.flagBillboard = true;

			auto itemMasterData = itemSystem->GetItemMasterDataByKey(dropRequest.droppedItemMasterKey);
			if (itemMasterData) {
				worldItemDropVisualSystem_->PreDrawPrimitive(dropRequest.droppedItemMasterKey, *dropRequest.droppedItemId,Primitive::PrimitiveUtility::PrimitiveType::QUADPRIMITIVE, primitivePreDrawParam, *itemMasterData);
			}
		}

	}

	void GameplayScene::InitModelPathToObjectKey()
	{
		//配置されるオブジェクトのモデルパスとオブジェクトキーの対応表を初期化する処理
		{
			auto allObj = objectAttributeManager_->GetAllData();

			for (const auto& obj : allObj)
			{
				worldObjectSystem_->RegisterObjectKey(obj.value.modelPath, obj.value.objectKey);
			}
		}

	}


}