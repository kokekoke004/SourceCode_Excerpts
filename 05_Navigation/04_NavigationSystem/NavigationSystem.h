#pragma once
#include <vector>
#include <memory>
#include <DirectXMath.h>
#include <cstdint>
#include"NavigationSystemUtility.h"

namespace Engine
{ 
	class IModelProvider;

	namespace Assets
	{
		class FbxInstanceRegistry;
	}

}

struct GridInfo;
struct ObstacleData;

class AstarObstacleRegistry;
class AstarPathFinder;
class ChunkManager;

namespace Engine::Navigation
{

	//経路探索結果
	struct PathResult {
		bool success = false;
		std::vector<DirectX::XMFLOAT3> points;
	};

	class NavigationSystem
	{
	public:
		NavigationSystem(IModelProvider& iModelProvider, Assets::FbxInstanceRegistry& fbxInstance);

		//障害物情報登録
		//OBBの座標と幅で
		void SetObstacle();

		//経路探索処理
		PathResult FindPath(ChunkManager& chunkManager, DirectX::XMFLOAT3 start, const PathGoal& goalInfo , const PathQuery& pathQuery = {}, bool isGoalObstacle = false);


		//Aスターアルゴリズム計算
		//astarObstacle : Aスター用にマッピングされた配列
		//gridInfo : グリッド横幅 (ピクセル幅や１ピクセル何メートルかなどの情報を使う)
		//startPos : 現在位置
		//goalPos : 目標座標
		std::vector<DirectX::XMFLOAT3> AStarSearch(const std::vector<std::uint32_t>& astarObstacle, const GridInfo& gridInfo, DirectX::XMFLOAT3 startPos, DirectX::XMFLOAT3 goalPos);

		//Aスターアルゴリズム計算(チャンク毎)
		//chunkManager : チャンクマネージャクラス
		//gridInfo : グリッド横幅 (ピクセル幅や１ピクセル何メートルかなどの情報を使う)
		//startPos : 現在位置
		//goalPos : 目標座標
		std::vector<DirectX::XMFLOAT3> AStarSearchPerChunk(ChunkManager& chunkManager, DirectX::XMFLOAT3 startPos, const PathGoal& goalInfo, const PathQuery& pathQuery, bool isGoalObstacle);

		void FillObstacleData(std::vector<ObstacleData>& out);

		//障害物マッピングした情報を消す
		void ClearObstacleData();

		const std::vector<ObstacleData>& GetObstacleData() const;

		std::vector<std::uint32_t>& GetAstarObstacle();
		const std::vector<std::uint32_t>& GetAstarObstacle() const;

		bool GetIsClickObstacle();


	private:

		std::shared_ptr<AstarObstacleRegistry> astarObstacleRegistry;

		std::shared_ptr<AstarPathFinder> astarPathfinder;

		IModelProvider* iModelProvider_ = nullptr;
		Assets::FbxInstanceRegistry& fbxInstance_;
	};
}

