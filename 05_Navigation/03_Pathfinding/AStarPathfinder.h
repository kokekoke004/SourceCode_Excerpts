#pragma once
#include <vector>
#include<d3d12.h>
#include"d3dx12.h"
#include<DirectXMath.h>
#include<unordered_map>
#include<unordered_set>
#include<string>
#include <memory>
#include<chrono>
#include<array>
#include<D3Dcompiler.h>
#include <queue>


#pragma comment(lib,"libfbxsdk-md.lib")
#pragma comment(lib,"libxml2-md.lib")
#pragma comment(lib,"zlib-md.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include"helper.h"
#include"ColliderDescUtility.h"
#include"NavigationSystemUtility.h"
#include"ChunkManager.h"
#include <map>


class AstarPathFinder
{
public:
	AstarPathFinder()
	{
		isClickObstacle = false;
	}

	//Aスターアルゴリズム計算
	//astarObstacle : Aスター用にマッピングされた配列
	//gridInfo : グリッド横幅 (ピクセル幅や１ピクセル何メートルかなどの情報を使う)
	//startPos : 現在位置
	//goalPos : 目標座標
	std::vector<DirectX::XMFLOAT3> AStarSearch(const std::vector<UINT>& astarObstacle,const GridInfo& gridInfo,DirectX::XMFLOAT3 startPos, DirectX::XMFLOAT3 goalPos);

	//Aスターアルゴリズム計算(チャンク毎)
	//chunkManager : チャンクマネージャクラス
	//gridInfo : グリッド横幅 (ピクセル幅や１ピクセル何メートルかなどの情報を使う)
	//startPos : 現在位置
	//goalPos : 目標座標
	std::vector<DirectX::XMFLOAT3> AStarSearchPerChunk(ChunkManager& chunkManager,DirectX::XMFLOAT3 startPos, const PathGoal& goalInfo, const PathQuery& pathQuery, bool isGoalObstacle);

	bool& GetIsClickObstacle()
	{
		return isClickObstacle;
	}

	const bool& GetIsClickObstacle() const
	{
		return isClickObstacle;
	}


private:
	const float interactR = 0.5f; //ゴール中心からどのくらいをサンプリングするか

	//openノード（コスト最小のノードを取り出す優先度つきキュー）
	struct Node {
		float f;  //Aスター評価値
		int idx;  //1次元配列で2次元配列を表すインデックス
	};

	struct Compare {
		bool operator()(const Node& a, const Node& b) const {
			return a.f > b.f;
		}
	};


	bool isClickObstacle; //障害物クリックフラグ

	//チャンク毎の障害物マップにアクセスする
	//chunkManager : チャンクマネージャクラス
	//gX : グリッド座標＿X
	//gY : グリッド座標＿Z
	//fallback : チャンクが見つからなかった時に返す値
	UINT SamplingObstacleMap(ChunkManager& chunkManager,int gX,int gZ,UINT fallback=1);

	//グリッド座標からチャンク座標を求める
	//pos : 座標
	//chunkSize : チャンクサイズ
	int FloorDiv(int pos, int chunkSize);

	//座標からグリッド番号を求める
	//pos : ワールド座標
	//scaleXZ : １グリッドの大きさ
	DirectX::XMINT2 WorldToGlobalGrid(const DirectX::XMFLOAT3& pos, float scaleXZ=1.0f);

	//Aスターで使うゴールからの指定された範囲内の座標のリスト作成
	// lX : ローカル格子座標＿X
	// lY : ローカル格子座標＿Y
	// width : 探索するチャンクの横幅
	// height : 探索するチャンクの縦幅
	// gMinX : 探索窓の左上セルのグローバル座用
	// gMinZ : 探索窓の左上セルのグローバル座用
	// scaleXZ : 1セル当たりのサイズ
	// range : ゴール中心からどのくらいをサンプリングするか
	// IsWalkable : 座標に対応する障害物情報が存在するかどうか
	std::vector<DirectX::XMFLOAT3> AstarRange(int lX, int lY, const int width,const int height, const int gMinX, const int gMinZ,const float scaleXZ, int range, const std::function<bool(int, int)>& IsWalkable);

	//ゴールからinteractRの範囲内ローカル格子インデックス格納
	//goalLX : ゴールローカル格子＿X
	//goalLY : ゴールローカル格子＿Y
	//interactR : ゴール中心からどのくらいをサンプリングするか
	// scaleXZ : 1セル当たりのサイズ
	// width : 探索するチャンクの横幅
	// height : 探索するチャンクの縦幅
	// IsWalkable : 座標に対応する障害物情報が存在するかどうか
	std::unordered_set<int> BuildGoalBandLocalIndices(int goalLX, int goalLY, float interactR, float scaleXZ,
		int width, int height, int gMinX, int gMinZ,const PathGoal& goalInfo, const std::function<bool(int, int)>& IsWalkable);

	float DistancePointToOBB_XZ(
		const DirectX::XMFLOAT3& point,
		const ColliderDesc& collider);

	//8近傍をチェックしオープンリストに追加
	//currentIdx : 現在のインデックス
	// width : 探索するチャンクの横幅
	//goalLX : ゴールローカル格子＿X
	//goalLY : ゴールローカル格子＿Y
	//closed : クローズノード
	//gScore : スコアリスト
	//parent : 親リスト
	//open : オープンノード
	void CheckDirEight(const int currentIdx, const int width, const DirectX::XMINT2& goalL, const std::vector<uint8_t>& closed,
		std::vector<float>& gScore, std::vector<int>& parent, std::priority_queue<Node, std::vector<Node>, Compare>& open
		, const std::function<bool(int, int)>& InBounds, const std::function<bool(int, int)>& IsWalkable, const std::function<int(int, int)>& ToIndex, const std::function<int(int x0, int y0, int x1, int y1)>& Heuristic);

	//経路探索をパスに追加していく
	//idx : インデックス
	//startIdx : スタートインデックス
	// width : 探索するチャンクの横幅
	// gMinX : 探索窓の左上セルのグローバル座用
	// gMinZ : 探索窓の左上セルのグローバル座用
	// scaleXZ : 1セル当たりのサイズ
	//parent : 親
	//path : 更新する経路用リスト
	void CreatePath(int idx, const int startIdx, const int width, const int gMinX, const int gMinZ, float scaleXZ, const std::vector<int>& parent, std::vector<DirectX::XMFLOAT3>& path);

};