#include"ChunkManager.h"
#include"ObjectPlacerManager.h"
#include"PerlinNoise.h"
#include"ComputeShaderManager.h"
#include"NavigationSystem.h"
#include"CreateResource.h"
#include"FbxLoader.h"
#include"NavigationSystemUtility.h"

using namespace std;

ChunkManager::ChunkManager(int mapId,int chunkSize)
	:mapId(mapId), chunkSize(chunkSize)
{
	objectPlacerManager.reset(new ObjectPlacerManager());
	perlin.reset(new PerlinNoise(1234));

}


//void ChunkManager::Update(std::shared_ptr<ComputeShaderManager>& computeShaderManager, std::shared_ptr<NavigationSystem>& navigationSystem, Game::MainSystem::GameSystem& gameSystem, const std::shared_ptr<ColliAttribute::CollisionAttributeData>&collisionAttributeData, const DirectX::XMFLOAT3& pos, HeightDisplacement& heightDisplacement)
//{
//	UpdateChunkDatas(gameSystem,collisionAttributeData,pos,heightDisplacement);
//	UpdateObstacleMapPerChunk(computeShaderManager, navigationSystem, heightDisplacement);
//}

//ChunkManagerの更新処理
ChunkUpdateResult ChunkManager::Update(ComputeShaderManager& computeShaderManager, Engine::Navigation::NavigationSystem& navigationSystem,const DirectX::XMFLOAT3& pos, HeightDisplacement& heightDisplacement)
{
	ChunkUpdateResult result;
	// 中心が変わったらキューに積むだけ（ここは軽い）
	EnqueueDiffIfMoved(pos);

	//毎フレーム少しずつ処理
	//生成・Spawn
	result.spawnedInstances = ProcessCreateQueue(heightDisplacement);
	//削除・RemoveDrawFbx
	ProcessRemoveQueue();
	//障害物マップ更新
	ProcessObstacleQueue(computeShaderManager, navigationSystem, heightDisplacement);

	//障害物の個数に変化があった場合障害物マッピング更新しなおす
	navigationSystem.SetObstacle();
	const auto& obData = navigationSystem.GetObstacleData();
	if ((int)obData.size() != lastObstacleCount_) {
		lastObstacleCount_ = (int)obData.size();
		RequestObstacleUpdateAllVisible();
	}
	//UpdateChunkDatas(pos, heightDisplacement);
	//UpdateObstacleMapPerChunk(computeShaderManager, navigationSystem, heightDisplacement);

	return result;
}

void ChunkManager::UpdateObstacleMapPerChunk(ComputeShaderManager& computeShaderManager, Engine::Navigation::NavigationSystem& navigationSystem, HeightDisplacement& heightDisplacement)
{
	//障害物情報の初期化
	navigationSystem.ClearObstacleData();
	//経路探索用障害物情報OBBをもとに登録
	navigationSystem.SetObstacle();

	for (int dZ = -chunkRange; dZ <= chunkRange; dZ++) {
		for (int dX = -chunkRange; dX <= chunkRange; dX++) {
			MapChunkKey key{ oldCenterChunkKey.mapId, oldCenterChunkKey.cx + dX, oldCenterChunkKey.cz + dZ };
			if (!FindKey(key, chunkDatas)) continue;
			if (!chunkDatas[key].isUpdateObstacle) continue;

			heightDisplacement.chunkSize = chunkSize;
			heightDisplacement.cX = key.cx;
			heightDisplacement.cZ = key.cz;

			vector<UINT> input;
			vector<UINT> heightMapObstacle;

			//コンピュートシェーダ実行処理下処理
			computeShaderManager.BeginCompute();
			//チャンクサイズをテクスチャサイズとして見立てる
			computeShaderManager.SetTexSize(L"HeightMapObstaclePerChunk", chunkSize, chunkSize, true);
			computeShaderManager.CreateResorceUAV(L"HeightMapObstaclePerChunk", L"obPerChunk", input, heightMapObstacle);
			auto obData = navigationSystem.GetObstacleData();
			heightDisplacement.obstacleCount = obData.size();
			computeShaderManager.CreateResorceUavAddMrg(L"HeightMapObstaclePerChunk", L"obPerChunk", obData, L"obData");
			computeShaderManager.CreateResorceUavAddMrg(L"HeightMapObstaclePerChunk", L"obPerChunk", heightDisplacement.h, L"heightDisplacement.h");
			computeShaderManager.CbData(L"HeightMapObstaclePerChunk", L"obPerChunk", heightDisplacement);
			//コンピュートシェーダ実行---------------------------------
			computeShaderManager.RunComputeShader(L"HeightMapObstaclePerChunk");

			chunkDatas[key].data.obstacleMap = heightMapObstacle;
			chunkDatas[key].isUpdateObstacle = false;
		}
	}

}

void ChunkManager::UpdateObstacleMapPerChunk(ComputeShaderManager& computeShaderManager, HeightDisplacement& heightDisplacement, const std::vector<ObstacleData>& obstacleData)
{
	for (int dZ = -chunkRange; dZ <= chunkRange; dZ++) {
		for (int dX = -chunkRange; dX <= chunkRange; dX++) {
			MapChunkKey key{ oldCenterChunkKey.mapId, oldCenterChunkKey.cx + dX, oldCenterChunkKey.cz + dZ };
			if (!FindKey(key, chunkDatas)) continue;
			if (!chunkDatas[key].isUpdateObstacle) continue;

			heightDisplacement.chunkSize = chunkSize;
			heightDisplacement.cX = key.cx;
			heightDisplacement.cZ = key.cz;
			//障害物情報の初期化
			//navigationSystem->GetObstacleData().clear();
			//経路探索用障害物情報OBBをもとに登録
			//navigationSystem->SetObstacle();

			vector<UINT> input;
			vector<UINT> heightMapObstacle;

			//コンピュートシェーダ実行処理下処理
			computeShaderManager.BeginCompute();
			//チャンクサイズをテクスチャサイズとして見立てる
			computeShaderManager.SetTexSize(L"HeightMapObstaclePerChunk", chunkSize, chunkSize, true);
			computeShaderManager.CreateResorceUAV(L"HeightMapObstaclePerChunk", L"obPerChunk", input, heightMapObstacle);
			auto obData = obstacleData;
			heightDisplacement.obstacleCount = obData.size();
			computeShaderManager.CreateResorceUavAddMrg(L"HeightMapObstaclePerChunk", L"obPerChunk", obstacleData, L"obData");
			computeShaderManager.CreateResorceUavAddMrg(L"HeightMapObstaclePerChunk", L"obPerChunk", heightDisplacement.h, L"heightDisplacement.h");
			computeShaderManager.CbData(L"HeightMapObstaclePerChunk", L"obPerChunk", heightDisplacement);
			//コンピュートシェーダ実行---------------------------------
			computeShaderManager.RunComputeShader(L"HeightMapObstaclePerChunk");

			chunkDatas[key].data.obstacleMap = heightMapObstacle;
			chunkDatas[key].isUpdateObstacle = false;
		}
	}

}


MapChunkKey ChunkManager::CalculateChunk(const DirectX::XMFLOAT3& pos)
{
	int g = chunkSize;
	int cX= static_cast<int>(std::floor(pos.x / chunkSize));
	int cZ = static_cast<int>(std::floor(pos.z / chunkSize));

	return MapChunkKey{ mapId,cX,cZ };
}

void ChunkManager::SetChunkDatas(const HeightDisplacement& heightDisplacement, const std::wstring& fbxName, int lowerBound , int upperBound, int chunkRange)
{
	this->chunkRange = chunkRange;

	if (!FindKey(fbxName, objectPlacementSettings)) {
		objectPlacementSettings[fbxName].fbxName = fbxName;
		objectPlacementSettings[fbxName].colliderKind = EColliderKind::None;
		objectPlacementSettings[fbxName].lowerBound = lowerBound;
		objectPlacementSettings[fbxName].upperBound = upperBound;
		objectPlacementSettings[fbxName].chunkRange = chunkRange;
	}


	for (int dZ = -chunkRange; dZ <= chunkRange; dZ++) {
		for (int dX = -chunkRange; dX <= chunkRange; dX++) {
			MapChunkKey key{ centerChunkKey.mapId ,centerChunkKey.cx + dX, centerChunkKey.cz + dZ };
			std::vector<std::wstring> drawNames;
			ChunkLive chunkLive;
			//keyがchunkDatasに存在しなければオブジェクトの配置座標を決定する
			if (!FindKey(key, chunkDatas)) {
				MapData mapData;

				mapData.chunkSize = chunkSize;
				//初期生成時のみ、全て1で生成
				mapData.occupied = std::vector<UINT> (chunkSize * chunkSize, 1);
				//パーリンデータ更新
				mapData.perlinData = UpdatePerlinData(chunkSize, key.cx, key.cz, heightDisplacement);
				//計算結果のパーリンデータを使って
				mapData.mapData[fbxName].spawnPos = objectPlacerManager->GetStrategy()->GeneratePositions(mapData.perlinData.allPerlinPositions, mapData.occupied, lowerBound, upperBound, chunkSize, centerChunkKey.cx + dX, centerChunkKey.cz + dZ);
				chunkDatas[key].data = mapData;
				objectPlacerManager->SpawnFbxInstances(fbxName, chunkDatas[key].data.mapData[fbxName].spawnPos, drawNames, objectPlacementSettings[fbxName].colliderKind);
				chunkLive.drawNames[fbxName] = drawNames;
				chunkDatas[key].live = chunkLive;

			}
			//すでにチャンクキーが存在している場合
			//新しいFBXのmapdataをセットする
			else if(!FindKey(fbxName, objectPlacementSettings)){
				chunkDatas[key].data.mapData[fbxName].spawnPos = objectPlacerManager->GetStrategy()->GeneratePositions(chunkDatas[key].data.perlinData.allPerlinPositions, chunkDatas[key].data.occupied, lowerBound, upperBound, chunkSize, centerChunkKey.cx + dX, centerChunkKey.cz + dZ);
				objectPlacerManager->SpawnFbxInstances(fbxName, chunkDatas[key].data.mapData[fbxName].spawnPos, drawNames, objectPlacementSettings[fbxName].colliderKind);
				chunkDatas[key].live.drawNames[fbxName] = drawNames;
			}

		}
	}



}

//void ChunkManager::UpdateCalculateChunkDatas(Game::MainSystem::GameSystem& gameSystem, const std::shared_ptr<ColliAttribute::CollisionAttributeData>& collisionAttributeData,const HeightDisplacement& heightDisplacement)
//{
//	for (int dZ = -chunkRange; dZ <= chunkRange; dZ++) {
//		for (int dX = -chunkRange; dX <= chunkRange; dX++) {
//			MapChunkKey key{ centerChunkKey.mapId ,centerChunkKey.cx + dX, centerChunkKey.cz + dZ };
//			std::unordered_map<std::wstring,std::vector<std::wstring>> drawNames;
//			ChunkLive chunkLive;
//			//keyがchunkDatasに存在しなければオブジェクトの配置座標を決定する
//			if (!FindKey(key, chunkDatas)) {
//				MapData mapData;
//
//				mapData.chunkSize = chunkSize;
//				//初期生成時のみ、全て1で生成
//				mapData.occupied = std::vector<UINT>(chunkSize * chunkSize, 1);
//				//パーリンデータ更新
//				mapData.perlinData = UpdatePerlinData(chunkSize, key.cx, key.cz, heightDisplacement);
//				chunkDatas[key].data.perlinData.allPerlinPositions = mapData.perlinData.allPerlinPositions;
//				chunkDatas[key].data.occupied = mapData.occupied;
//
//				for (auto& objectPlacementSetting : objectPlacementSettings) {
//					chunkDatas[key].data.mapData[objectPlacementSetting.second.fbxName].spawnPos = objectPlacerManager->GetStrategy()->GeneratePositions(chunkDatas[key].data.perlinData.allPerlinPositions, chunkDatas[key].data.occupied, objectPlacementSetting.second.lowerBound, objectPlacementSetting.second.upperBound, chunkSize, centerChunkKey.cx + dX, centerChunkKey.cz + dZ);
//					objectPlacerManager->SpawnFbxInstances(objectPlacementSetting.second.fbxName, chunkDatas[key].data.mapData[objectPlacementSetting.second.fbxName].spawnPos, drawNames[objectPlacementSetting.second.fbxName], objectPlacementSettings[objectPlacementSetting.second.fbxName].colliderKind);
//					chunkDatas[key].live.drawNames[objectPlacementSetting.second.fbxName] = drawNames[objectPlacementSetting.second.fbxName];
//
//					//HPをオブジェクトにセットする
//					//const auto& fbx = objectPlacerManager->GetFbxLoader(objectPlacementSetting.second.fbxName);
//					//const auto& collision = fbx->GetcolliderComponent();
//					//gameSystem.InitEntity(collision, collisionAttributeData);
//					
//					
//				}
//			}
//		}
//	}
//
//}

void ChunkManager::UpdateCalculateChunkDatas(const HeightDisplacement& heightDisplacement)
{
	for (int dZ = -chunkRange; dZ <= chunkRange; dZ++) {
		for (int dX = -chunkRange; dX <= chunkRange; dX++) {
			MapChunkKey key{ centerChunkKey.mapId ,centerChunkKey.cx + dX, centerChunkKey.cz + dZ };
			std::unordered_map<std::wstring, std::vector<std::wstring>> drawNames;
			ChunkLive chunkLive;
			//keyがchunkDatasに存在しなければオブジェクトの配置座標を決定する
			if (!FindKey(key, chunkDatas)) {
				MapData mapData;
				printf("centerChunkKey: mapId=%d, dX=%d, dZ=%d\n", centerChunkKey.mapId, dX, dZ);

				mapData.chunkSize = chunkSize;
				//初期生成時のみ、全て1で生成
				mapData.occupied = std::vector<UINT>(chunkSize * chunkSize, 1);
				//パーリンデータ更新
				mapData.perlinData = UpdatePerlinData(chunkSize, key.cx, key.cz, heightDisplacement);
				chunkDatas[key].data.perlinData.allPerlinPositions = mapData.perlinData.allPerlinPositions;
				chunkDatas[key].data.occupied = mapData.occupied;

				for (auto& objectPlacementSetting : objectPlacementSettings) {
					chunkDatas[key].data.mapData[objectPlacementSetting.second.fbxName].spawnPos = objectPlacerManager->GetStrategy()->GeneratePositions(chunkDatas[key].data.perlinData.allPerlinPositions, chunkDatas[key].data.occupied, objectPlacementSetting.second.lowerBound, objectPlacementSetting.second.upperBound, chunkSize, centerChunkKey.cx + dX, centerChunkKey.cz + dZ);
					objectPlacerManager->SpawnFbxInstances(objectPlacementSetting.second.fbxName, chunkDatas[key].data.mapData[objectPlacementSetting.second.fbxName].spawnPos, drawNames[objectPlacementSetting.second.fbxName], objectPlacementSettings[objectPlacementSetting.second.fbxName].colliderKind);
					chunkDatas[key].live.drawNames[objectPlacementSetting.second.fbxName] = drawNames[objectPlacementSetting.second.fbxName];

					////HPをオブジェクトにセットする
					//const auto& fbx = objectPlacerManager->GetFbxLoader(objectPlacementSetting.second.fbxName);
					//const auto& collision = fbx->GetcolliderComponent();
					//gameSystem.InitEntity(collision, collisionAttributeData);


				}
			}
		}
	}

}


//void ChunkManager::UpdateChunkDatas(Game::MainSystem::GameSystem& gameSystem, const std::shared_ptr<ColliAttribute::CollisionAttributeData>& collisionAttributeData, const DirectX::XMFLOAT3& pos, const HeightDisplacement& heightDisplacement)
//{
//	centerChunkKey = CalculateChunk(pos);
//	for (auto& objectPlacementSetting : objectPlacementSettings) {
//		//キーchunkKeyこれが見つからなければ生成し直し
//		UpdateCalculateChunkDatas(gameSystem, collisionAttributeData, heightDisplacement);
//
//		RemoveChunkData(objectPlacementSetting.second.fbxName);
//	}
//}

void ChunkManager::UpdateChunkDatas(const DirectX::XMFLOAT3& pos, const HeightDisplacement& heightDisplacement)
{
	centerChunkKey = CalculateChunk(pos);
	for (auto& objectPlacementSetting : objectPlacementSettings) {
		//キーchunkKeyこれが見つからなければ生成し直し
		UpdateCalculateChunkDatas(heightDisplacement);

		RemoveChunkData(objectPlacementSetting.second.fbxName);
	}
}


void ChunkManager::UpdateChunkData()
{

	for (int dZ = -chunkRange; dZ <= chunkRange; dZ++) {
		for (int dX = -chunkRange; dX <= chunkRange; dX++) {
			MapChunkKey key{ centerChunkKey.mapId,centerChunkKey.cx + dX, centerChunkKey.cz + dZ };
			std::vector<std::wstring> drawNames;
			ChunkLive chunkLive;
			if (!FindKey(key, chunkDatas)) {
				chunkDatas[key].isDelete = false;
			}

		}
	}

}

void ChunkManager::RemoveChunkData(const std::wstring& fbxName)
{
	if (centerChunkKey.cx != oldCenterChunkKey.cx || centerChunkKey.cz != oldCenterChunkKey.cz
		|| centerChunkKey.mapId != oldCenterChunkKey.mapId) {
		//古いチャンク全て削除フラグ立てる
		for (int dZ = -chunkRange; dZ <= chunkRange; dZ++) {
			for (int dX = -chunkRange; dX <= chunkRange; dX++) {
				MapChunkKey key{ oldCenterChunkKey.mapId, oldCenterChunkKey.cx + dX, oldCenterChunkKey.cz + dZ };
				if (FindKey(key, chunkDatas)) {
					chunkDatas[key].isDelete = true;
				}

			}
		}

		//新しいチャンク削除フラグを消す
		for (int dZ = -chunkRange; dZ <= chunkRange; dZ++) {
			for (int dX = -chunkRange; dX <= chunkRange; dX++) {
				MapChunkKey key{ centerChunkKey.mapId, centerChunkKey.cx + dX, centerChunkKey.cz + dZ };
				if (FindKey(key, chunkDatas)) {
					chunkDatas[key].isDelete = false;
				}
			}
		}

		std::vector<MapChunkKey> toErase;
		toErase.reserve(chunkDatas.size());

		for (auto& [key,chunk] : chunkDatas) {
			if (chunk.isDelete) {
				for (const auto& [fbxNameInChunk, keyDrawList] : chunk.live.drawNames) {
					for (const auto& keyFbxName : keyDrawList) {
						objectPlacerManager->RemoveDrawFbx(fbxNameInChunk, keyFbxName);
					}
				}
				toErase.emplace_back(key);
			}
		}

		for (const auto& key : toErase) {
			chunkDatas.erase(key);
		}
	}

	oldCenterChunkKey = centerChunkKey;
}

PerlinData ChunkManager::UpdatePerlinData(int chunkSize, int cX, int cZ, const HeightDisplacement& heightDisplacement)
{
	PerlinData perlinData;
	//キーが存在しなければ、パーリンノイズ計算する
	perlinData.allPerlinPositions = (perlin->CalculatePerlinNoise(chunkSize, cX, cZ, heightDisplacement));

	return perlinData;
}

void ChunkManager::UpdateObstacleMapDataAllChunk()
{
	const int chunksPerSide = 2 * chunkRange + 1;
	const int tilesPerChunk = chunkSize * chunkSize;
	const size_t totalElems = size_t(chunksPerSide) * chunksPerSide * tilesPerChunk;

	//全体を通行可=1で初期化
	obstacleDataAllChunk.assign(totalElems, 1u);

	size_t writeBase = 0;
	for (int dZ = -chunkRange; dZ <= chunkRange; ++dZ) {
		for (int dX = -chunkRange; dX <= chunkRange; ++dX) {

			MapChunkKey key{ centerChunkKey.mapId, centerChunkKey.cx + dX, centerChunkKey.cz + dZ };
			auto it = chunkDatas.find(key);

			if (it != chunkDatas.end()) {
				const auto& src = it->second.data.obstacleMap; //std::vector<UINT> (0/1)
				if (src.size() == size_t(tilesPerChunk)) {
					//一括コピー（要素ループ不要）
					std::memcpy(&obstacleDataAllChunk[writeBase],
						src.data(),
						tilesPerChunk * sizeof(UINT));
				}
			}

			writeBase += tilesPerChunk;
		}
	}
}

void ChunkManager::UpdateObstacleMapDataAllChunk(std::shared_ptr<CreateResource>& createResource, CommandContext& commandContext, ComPtr<ID3D12Resource>& gpuInstanceBuffer)
{
	const UINT stride = chunkSize;
	//列バイト
	const UINT srcRowBytes = chunkSize * sizeof(uint32_t);
	//１チャンク毎のサイズ
	const UINT64 oneChunkBytes = UINT64(srcRowBytes) * chunkSize;
	//全チャンクでの合計バイト数
	const UINT64 totalBytes = oneChunkBytes * (2*chunkRange+1)*(2*chunkRange+1);

	ComPtr<ID3D12Resource> upload;
	upload = createResource->CreateUploadBuffer(totalBytes);


	uint8_t* base = nullptr;
	upload->Map(0, nullptr, (void**)&base);
	UINT64 baseOff = 0;

	for (int dZ = -chunkRange; dZ <= chunkRange; dZ++) {
		for (int dX = -chunkRange; dX <= chunkRange; dX++) {
			MapChunkKey key{ centerChunkKey.mapId, centerChunkKey.cx + dX, centerChunkKey.cz + dZ };
			if (FindKey(key, chunkDatas) && chunkDatas[key].data.obstacleMap.size()!=0) {
				memcpy(base + baseOff, chunkDatas[key].data.obstacleMap.data(), size_t(oneChunkBytes));
				baseOff += oneChunkBytes;

			}
			else {
				//チャンクが見つからなかった場合通行可能として1を渡す
				std::fill_n(reinterpret_cast<uint32_t*>(base + baseOff),
					size_t(chunkSize) * chunkSize, 1u);

				baseOff += oneChunkBytes;

			}
		}
	}

	upload->Unmap(0, nullptr);

	auto toCopy = CD3DX12_RESOURCE_BARRIER::Transition(
		gpuInstanceBuffer.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
	GraphicsListFrom(commandContext)->ResourceBarrier(1, &toCopy);

	//チャンク座標での左上
	int minCX = centerChunkKey.cx - chunkRange;
	int minCZ = centerChunkKey.cz - chunkRange;

	//横一列のチャンク大きさ
	int chunkWidth = (2 * chunkRange + 1) * stride;
	int chunkHeight = chunkWidth;


	//チャンク毎に行コピーする
	UINT64 srcBase = 0;

	for (int dZ = -chunkRange; dZ <= chunkRange; dZ++) {
		for (int dX = -chunkRange; dX <= chunkRange; dX++) {
			MapChunkKey key{ centerChunkKey.mapId, centerChunkKey.cx + dX, centerChunkKey.cz + dZ };

			const UINT tileX = UINT(key.cx - minCX);
			const UINT tileZ = UINT(key.cz - minCZ);
			const UINT dstX = tileX * stride;
			const UINT dstY = tileZ * stride;


			if (dstX + chunkSize > chunkWidth || dstY + chunkSize > chunkHeight) {
				srcBase += oneChunkBytes;
				continue;
			}

			//uploadInstanceBufferに横一列のチャンクコピー
			//横16個のデータを16回繰り返してコピーすることで、一チャンクのデータをコピーしてる
			for (UINT y = 0; y < chunkSize; ++y) {
				const UINT64 srcOffset = srcBase + UINT64(y) * srcRowBytes;           //Upload側
				const UINT64 dstOffset = ElemOffset(dstX, dstY + y, chunkWidth);      //インスタンスバッファ側

				//GPUに値を渡してる
				GraphicsListFrom(commandContext)->CopyBufferRegion(gpuInstanceBuffer.Get(), dstOffset,
					upload.Get(), srcOffset,
					srcRowBytes);
			}
			srcBase += oneChunkBytes;

		}
	}

	auto toRead = CD3DX12_RESOURCE_BARRIER::Transition(
		gpuInstanceBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
	GraphicsListFrom(commandContext)->ResourceBarrier(1, &toRead);

}

void ChunkManager::SetChunkUpdateFlag(const MapChunkKey& mapKey,bool updateFlag)
{
	//キーが存在すればtrue
	if (!chunkDatas.count(mapKey)) return;
	chunkDatas[mapKey].isUpdateObstacle = updateFlag;
	if (updateFlag) {
		EnqueueObstacle(mapKey);  //キューにも積む
	}
}

bool ChunkManager::IsInsideWindow(const MapChunkKey& key, const MapChunkKey& center) const
{
	if (key.mapId != center.mapId) return false;
	//チャンクがチャンクレンジ内におさまっているかどうか
	return (std::abs(key.cx - center.cx) <= chunkRange) &&
		(std::abs(key.cz - center.cz) <= chunkRange);
}

void ChunkManager::EnqueueCreate(const MapChunkKey& key)
{
	//second == trueの時は初めて値が入ることになる
	//second == falseの時はすでに値が入っている
	if (queuedCreate_.insert(key).second) {
		createQueue_.push_back(key);
	}
}
void ChunkManager::EnqueueRemove(const MapChunkKey& key)
{
	//second == trueの時は初めて値が入ることになる
	//second == falseの時はすでに値が入っている
	if (queuedRemove_.insert(key).second) {
		removeQueue_.push_back(key);
	}
}
void ChunkManager::EnqueueObstacle(const MapChunkKey& key)
{
	//second == trueの時は初めて値が入ることになる
	//second == falseの時はすでに値が入っている
	if (queuedObstacle_.insert(key).second) {
		obstacleQueue_.push_back(key);
	}
}

void ChunkManager::EnqueueAddColumn(int cx, int czCenter)
{
	//X軸方向にプレイヤーが移動したらZ軸のチャンクをまとめて作成するキューに積む
	for (int dz = -chunkRange; dz <= chunkRange; ++dz) {
		EnqueueCreate(MapChunkKey{ mapId, cx, czCenter + dz });
	}
}
void ChunkManager::EnqueueRemoveColumn(int cx, int czCenter)
{
	//X軸方向にプレイヤーが移動したらZ軸のチャンクをまとめて削除するキューに積む
	for (int dz = -chunkRange; dz <= chunkRange; ++dz) {
		EnqueueRemove(MapChunkKey{ mapId, cx, czCenter + dz });
	}
}
void ChunkManager::EnqueueAddRow(int cz, int cxCenter)
{
	//Z軸方向にプレイヤーが移動したらX軸のチャンクをまとめて作成するキューに積む
	for (int dx = -chunkRange; dx <= chunkRange; ++dx) {
		EnqueueCreate(MapChunkKey{ mapId, cxCenter + dx, cz });
	}
}
void ChunkManager::EnqueueRemoveRow(int cz, int cxCenter)
{
	//Z軸方向にプレイヤーが移動したらX軸のチャンクをまとめて削除するキューに積む
	for (int dx = -chunkRange; dx <= chunkRange; ++dx) {
		EnqueueRemove(MapChunkKey{ mapId, cxCenter + dx, cz });
	}
}

void ChunkManager::EnqueueDiffIfMoved(const DirectX::XMFLOAT3& pos)
{
	MapChunkKey newCenter = CalculateChunk(pos);

	//初回（まだcenterが未設定なら）や、mapIdが変わるケースもここで拾う
	if (isInitialized_ && newCenter.mapId == centerChunkKey.mapId &&
		newCenter.cx == centerChunkKey.cx &&
		newCenter.cz == centerChunkKey.cz)
	{
		//変化なし
		return; 
	}

	isInitialized_ = true;

	const MapChunkKey oldCenter = centerChunkKey;
	centerChunkKey = newCenter;

	//oldCenter が未初期化っぽい場合（必要なら条件を追加）
	//ここではワープ扱いにして新範囲を全部Createに積む
	const int dcx = newCenter.cx - oldCenter.cx;
	const int dcz = newCenter.cz - oldCenter.cz;

	//1チャンク移動かどうかを判定する
	const bool stepMove = (std::abs(dcx) <= 1) && (std::abs(dcz) <= 1) && !(dcx == 0 && dcz == 0);

	if (stepMove && oldCenter.mapId == newCenter.mapId) {
		//+X / -X 移動：列を追加/削除
		if (dcx > 0) {
			EnqueueAddColumn(newCenter.cx + chunkRange, newCenter.cz);
			EnqueueRemoveColumn(oldCenter.cx - chunkRange, oldCenter.cz);
		}
		else if (dcx < 0) {
			EnqueueAddColumn(newCenter.cx - chunkRange, newCenter.cz);
			EnqueueRemoveColumn(oldCenter.cx + chunkRange, oldCenter.cz);
		}

		//+Z / -Z 移動：行を追加/削除
		if (dcz > 0) {
			EnqueueAddRow(newCenter.cz + chunkRange, newCenter.cx);
			EnqueueRemoveRow(oldCenter.cz - chunkRange, oldCenter.cx);
		}
		else if (dcz < 0) {
			EnqueueAddRow(newCenter.cz - chunkRange, newCenter.cx);
			EnqueueRemoveRow(oldCenter.cz + chunkRange, oldCenter.cx);
		}
		//斜め移動の場合も列+行が積まれる（角が重複するが seunordered_setが重複防止で防ぐ）
	}
	else {
		//ワープ/初回、新ウィンドウを全部Create、古いものはウィンドウ外だけRemove
		for (int dz = -chunkRange; dz <= chunkRange; ++dz) {
			for (int dx = -chunkRange; dx <= chunkRange; ++dx) {
				EnqueueCreate(MapChunkKey{ mapId, newCenter.cx + dx, newCenter.cz + dz });
			}
		}
		//既存chunkDatasのうち、newWindow外だけ消す
		for (const auto& [k, v] : chunkDatas) {
			if (!IsInsideWindow(k, newCenter)) {
				EnqueueRemove(k);
			}
		}
	}
}

std::vector<SpawnedInstanceInfo> ChunkManager::CreateOneChunk(const MapChunkKey& key, const HeightDisplacement& hd)
{
	// 既にあるなら何もしない
	if (chunkDatas.find(key) != chunkDatas.end())
		return {};

	MapData mapData;
	mapData.chunkSize = chunkSize;
	mapData.occupied = std::vector<UINT>(size_t(chunkSize) * chunkSize, 1u);
	mapData.perlinData = UpdatePerlinData(chunkSize, key.cx, key.cz, hd);

	ChunkLive live;

	std::vector<SpawnedInstanceInfo> spawnedInstances;

	//fbx種類ごとに配置->Spawn
	for (auto& [name, setting] : objectPlacementSettings) {
		std::vector<std::wstring> drawNames;

		auto& spawnPos = mapData.mapData[setting.fbxName].spawnPos;
		spawnPos = objectPlacerManager->GetStrategy()->GeneratePositions(
			mapData.perlinData.allPerlinPositions,
			mapData.occupied,
			setting.lowerBound,
			setting.upperBound,
			chunkSize,
			key.cx,
			key.cz
		);

		for (auto& sp : spawnPos) {
			float expectedMinX = key.cx * chunkSize;
			float expectedMaxX = (key.cx + 1) * chunkSize;
			float expectedMinZ = key.cz * chunkSize;
			float expectedMaxZ = (key.cz + 1) * chunkSize;
			if (sp.x < expectedMinX || sp.x >= expectedMaxX ||
				sp.z < expectedMinZ || sp.z >= expectedMaxZ) {
				printf("[MISMATCH] key=(%d,%d) but spawnPos=(%.1f, %.1f)\n",
					key.cx, key.cz, sp.x, sp.z);
			}
		}

		auto spawnData = objectPlacerManager->SpawnFbxInstances(setting.fbxName, spawnPos, drawNames, setting.colliderKind);
		live.drawNames[setting.fbxName] = std::move(drawNames);
		
		for (auto data : spawnData) {
			SpawnedInstanceInfo spawnedInfo;
			spawnedInfo.modelPath = data.modelPath;
			spawnedInfo.drawName = data.drawName;
			spawnedInfo.tid = data.tid;
			spawnedInstances.push_back(std::move(spawnedInfo));

		}
		//spawnedInfo.drawName = 
	}

	//chunkDatasに登録
	auto& dst = chunkDatas[key];
	dst.data = std::move(mapData);
	dst.live = std::move(live);
	dst.isDelete = false;
	dst.isUpdateObstacle = true; //障害物更新が必要なら

	//コールバック関数がセットされていれば呼び出す
	if (onChunkUpdated_) {
		onChunkUpdated_(key);  //ここで外部に通知
	}

	return spawnedInstances;

}

void ChunkManager::RemoveOneChunk(const MapChunkKey& key)
{
	auto it = chunkDatas.find(key);
	if (it == chunkDatas.end())
		return;

	auto& chunk = it->second;

	//描画インスタンス削除
	for (const auto& [fbxNameInChunk, keyDrawList] : chunk.live.drawNames) {
		for (const auto& keyFbxName : keyDrawList) {
			objectPlacerManager->RemoveDrawFbx(fbxNameInChunk, keyFbxName);
		}
	}

	chunkDatas.erase(it);
}

std::vector<SpawnedInstanceInfo> ChunkManager::ProcessCreateQueue(const HeightDisplacement& hd)
{
	int budget = createBudgetPerFrame_;

	std::vector<SpawnedInstanceInfo> spawnedInstances;

	while (budget-- > 0 && !createQueue_.empty()) {
		//要素を先頭から取り出す
		MapChunkKey key = createQueue_.front();
		//取り出したらキューから削除する
		createQueue_.pop_front();
		//重複管理は、要素をすでに取り出したため、ここから対象のキー削除する
		queuedCreate_.erase(key);

		//もう不要（移動で範囲外）ならスキップ
		if (!IsInsideWindow(key, centerChunkKey))
			continue;

		auto spawnedTmp = CreateOneChunk(key, hd);

		//spawnedTmpをspawnedInstancesに移動挿入する
		spawnedInstances.insert(
			spawnedInstances.end(),
			std::make_move_iterator(spawnedTmp.begin()),
			std::make_move_iterator(spawnedTmp.end())
		);


		//障害物更新キューへ
		//キーが存在すれば1==true
		if (chunkDatas.count(key)) {
			chunkDatas[key].isUpdateObstacle = true;  // 強制的にフラグを立て直す
			EnqueueObstacle(key);
		}
	}

	return spawnedInstances;
}

void ChunkManager::ProcessRemoveQueue()
{
	int budget = removeBudgetPerFrame_;

	while (budget-- > 0 && !removeQueue_.empty()) {
		const MapChunkKey key = removeQueue_.front();
		//要素を先頭から取り出す
		removeQueue_.pop_front();
		//重複防止セットから削除する
		queuedRemove_.erase(key);

		//今の中心範囲内に戻ってきたなら消さない
		if (IsInsideWindow(key, centerChunkKey))
			continue;

		//実際の削除（RemoveDrawFbx -> erase）を1チャンク分だけ
		RemoveOneChunk(key);
	}
}

void ChunkManager::ProcessObstacleQueue(ComputeShaderManager& cs,
	Engine::Navigation::NavigationSystem& nav,
	HeightDisplacement& hd)
{
	if (obstacleQueue_.empty())
		return;

	//障害物(OBB)の収集は1回だけ
	nav.ClearObstacleData();
	nav.SetObstacle();
	const auto obData = nav.GetObstacleData();

	const size_t tiles = size_t(chunkSize) * chunkSize;

	//使い回し（毎チャンクnewしない）
	std::vector<UINT> input;
	std::vector<UINT> heightMapObstacle; //出力（CreateResorceUAV側が埋める）

	int budget = obstacleBudgetPerFrame_;

	while (budget-- > 0 && !obstacleQueue_.empty()) {
		const MapChunkKey key = obstacleQueue_.front();
		obstacleQueue_.pop_front();
		queuedObstacle_.erase(key); //重複防止セットから外す

		//すでに消えている/範囲外ならスキップ
		auto it = chunkDatas.find(key);
		if (it == chunkDatas.end())
			continue;

		if (!IsInsideWindow(key, centerChunkKey))
			continue;

		if (!it->second.isUpdateObstacle)
			continue;

		//障害物が1つも無いなら、全部通行可(1)にしてCSを回さない
		if (obData.empty()) {
			it->second.data.obstacleMap.assign(tiles, 1u);
			it->second.isUpdateObstacle = false;
			continue;
		}

		//1チャンク分のパラメータ設定
		hd.chunkSize = chunkSize;
		hd.cX = key.cx;
		hd.cZ = key.cz;
		hd.obstacleCount = (UINT)obData.size();

		//使い回し前提でクリア
		input.clear();
		heightMapObstacle.clear();

		// ---- Compute実行 ----
		cs.BeginCompute();

		// サイズが固定なら SetTexSize はループ外に出してもOK（ComputeShaderManagerの仕様次第）
		cs.SetTexSize(L"HeightMapObstaclePerChunk", chunkSize, chunkSize, true);
		cs.CreateResorceUAV(L"HeightMapObstaclePerChunk", L"obPerChunk", input, heightMapObstacle);
		cs.CreateResorceUavAddMrg(L"HeightMapObstaclePerChunk", L"obPerChunk", obData, L"obData");
		cs.CreateResorceUavAddMrg(L"HeightMapObstaclePerChunk", L"obPerChunk", hd.h, L"heightDisplacement.h");
		cs.CbData(L"HeightMapObstaclePerChunk", L"obPerChunk", hd);

		cs.RunComputeShader(L"HeightMapObstaclePerChunk");
		// -----------------------------------------------

		//出力をチャンクに保存（swapでコピー回避、capacityも再利用できる）
		it->second.data.obstacleMap.swap(heightMapObstacle);
		it->second.isUpdateObstacle = false;
	}
}

void ChunkManager::RequestObstacleUpdateAllVisible()
{
	for (int dz = -chunkRange; dz <= chunkRange; ++dz) {
		for (int dx = -chunkRange; dx <= chunkRange; ++dx) {
			MapChunkKey key{ mapId, centerChunkKey.cx + dx, centerChunkKey.cz + dz };
			SetChunkUpdateFlag(key, true);
		}
	}
}