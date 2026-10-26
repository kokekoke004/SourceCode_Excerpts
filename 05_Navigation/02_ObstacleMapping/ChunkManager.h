#pragma once

#include <vector>
#include <memory>
#include <string>
#include<unordered_map>
#include<DirectXMath.h>

#include"PerlinNoiseStrategy.h"
#include"ObjectInstantiator.h"
#include"helper.h"
#include"DXAppUtillity.h"
#include"CollisionType.h"
#include"CollisionAttributeData.h"
#include <deque>
#include <unordered_set>
#include"TransformSystem.h"

struct ObstacleData;

struct MapChunkKey {
    int mapId;   //ワールド / レイヤ ID
    int cx;      //チャンク X
    int cz;      //チャンク Z

};


enum class ObjectType
{
    ALL,
    Tree,
    Rock,
};

inline bool operator==(const MapChunkKey& a,
    const MapChunkKey& b) noexcept
{
    return a.mapId == b.mapId && a.cx == b.cx && a.cz == b.cz;
}

//MapChunkKeyこれをマップで使うためにハッシュ値にする
struct MapChunkKeyHash {
    size_t operator()(const MapChunkKey& k) const noexcept {
        // FNV1a簡易
        uint64_t h = 1469598103934665603ull;
        auto mix = [&](uint32_t v) { h ^= v; h *= 1099511628211ull; };
        mix((uint32_t)k.mapId);
        mix((uint32_t)k.cx);
        mix((uint32_t)k.cz);
        return (size_t)h;
    }
};

struct PerlinData
{
    std::vector<DirectX::XMFLOAT3> allPerlinPositions; //対応するチャンクのパーリンノイズ計算結果全情報
};

struct SpawnData
{
    std::vector<DirectX::XMFLOAT3> spawnPos;
};

struct MapData
{
    int chunkSize; //チャンクサイズ
    bool isUpdateMap; //チャンクマップデータを更新するかどうか（true=更新 / false= 更新しない）
    std::vector<UINT> occupied; //座標にオブジェクトが配置されているかどうか管理する（1==配置済み/ 0==配置されてない）
    std::vector<UINT> obstacleMap; //障害物マップ
    PerlinData perlinData;
    
    //キー：fbxName
    std::unordered_map<std::wstring, SpawnData> mapData; //マップデータ
};

struct ChunkLive {
    //FBXの描画リスト
    std::unordered_map<std::wstring, std::vector<std::wstring>> drawNames;
};

struct ChunkSlot {
    MapData data;
    ChunkLive live;
    bool isDelete;  //チャンクデータを削除するかどうか
    bool isUpdateObstacle = true; //障害物情報更新するかどうか
};

struct SpawnedInstanceInfo
{
    Engine::Components::TransformId tid;
    std::wstring modelPath;
    std::wstring drawName;
};

struct ChunkUpdateResult
{
    std::vector<SpawnedInstanceInfo> spawnedInstances;
};

namespace Engine::Navigation
{
    class NavigationSystem;

}

class ObjectPlacerManager;
class PerlinNoise;
class ComputeShaderManager;
class CreateResource;

class ChunkManager
{
public:
    //MapChunkKeyを受け取って、戻り値なしの関数という型を定義
    using OnChunkUpdated = std::function<void(const MapChunkKey&)>;


	ChunkManager(int mapId,int chunkSize);



    //チャンク毎のオブジェクト配置情報計算
    //fbxName : 対象FBXの名前
    //lowerBound : 配置境界下限 (これらは、パーリンソンノイズで使用)
    //upperBound : 配置境界上限
    //chunkRange : チャンクの範囲 例：(1=3x3,2=4x4)
    void SetChunkDatas(const HeightDisplacement& heightDisplacement, const std::wstring& fbxName, int lowerBound, int upperBound, int chunkSize);

    //ChunkManagerの更新処理
	//void Update(std::shared_ptr<ComputeShaderManager>& computeShaderManager, std::shared_ptr<NavigationSystem>& navigationSystem, Game::MainSystem::GameSystem& gameSystem, const std::shared_ptr<ColliAttribute::CollisionAttributeData>& collisionAttributeData,const DirectX::XMFLOAT3& pos, HeightDisplacement& heightDisplacement);

    //ChunkManagerの更新処理
    //更新結果を返却する
    ChunkUpdateResult Update(ComputeShaderManager& computeShaderManager, Engine::Navigation::NavigationSystem& navigationSystem, const DirectX::XMFLOAT3& pos, HeightDisplacement& heightDisplacement);


    //チャンクデータを削除する
    void RemoveChunkData(const std::wstring& fbxName);

    //パーリンノイズ計算更新
    PerlinData UpdatePerlinData(int chunkSize, int cX, int cZ, const HeightDisplacement& heightDisplacement);

    //外部から関数を登録するための窓口
    void SetOnChunkUpdatedCallback(OnChunkUpdated callback)
    {
        onChunkUpdated_ = callback; //登録された関数を保管
    }

    std::shared_ptr<ObjectPlacerManager>& GetObjectPlacerManager()
    {
        return objectPlacerManager;
    }

    const std::shared_ptr<ObjectPlacerManager>& GetObjectPlacerManager() const
    {
        return objectPlacerManager;
    }

    int& GetChunkSize()
    {
        return chunkSize;
    }

    const int& GetChunkSize() const
    {
        return chunkSize;
    }

    std::unordered_map<MapChunkKey, ChunkSlot, MapChunkKeyHash>& GetChunkDatas()
    {
        return chunkDatas;
    }

    const std::unordered_map<MapChunkKey, ChunkSlot, MapChunkKeyHash>& GetChunkDatas() const
    {
        return chunkDatas;
    }

    int& GetChunkRange()
    {
        return chunkRange;
    }

    const int& GetChunkRange() const
    {
        return chunkRange;
    }

    MapChunkKey& GetCenterChunkKey()
    {
        return centerChunkKey;
    }

    const MapChunkKey& GetCenterChunkKey() const
    {
        return centerChunkKey;
    }

    MapChunkKey CalculateChunk(const DirectX::XMFLOAT3& pos);

    //チャンク毎の障害物マップを一つにまとめて、インスタンシングバッファに渡しアプロードまでする(この処理は上手くいっていない　：　気が向いたら修正)
    //createResource : リソース作成クラス
    //commandContext : commandContext
    //gpuInstanceBuffer  : 更新対象とするインスタンシングバッファリソース
    void UpdateObstacleMapDataAllChunk(std::shared_ptr<CreateResource>& createResource,CommandContext& commandContext, ComPtr<ID3D12Resource>& gpuInstanceBuffer);

	//チャンク毎の障害物マップを一つにまとめる
	//この関数は、障害物マップを一つにまとめるだけで、インスタンシングバッファへのアップロードは行わない
    void UpdateObstacleMapDataAllChunk();

    std::vector<UINT>& GetObstacleDataAllChunk()
    {
        return obstacleDataAllChunk;
    }

    const std::vector<UINT>& GetObstacleDataAllChunk() const
    {
        return obstacleDataAllChunk;
    }

    //キーに対応するチャンクをアップデートするフラグ立てる
    //mapKey : キー
    //updateFlag : アップデートフラグ
    void SetChunkUpdateFlag(const MapChunkKey& mapKey, bool updateFlag);

    //障害物情報すべて更新するフラグを立てる
    void RequestObstacleUpdateAllVisible();

    struct PlacementSetting
    {
        std::wstring fbxName;
        EColliderKind colliderKind; //コリジョンの種類
        int lowerBound = 0; //lowerBound : 配置境界下限 (これらは、パーリンソンノイズで使用)
        int upperBound = 0; //upperBound : 配置境界上限
        int chunkRange = 0; //チャンク範囲
    };

	//チャンクごとに配置するオブジェクトの設定を保持するマップ
    const std::unordered_map<std::wstring, PlacementSetting>& GetObjectPlacementSettings() const 
    {
        return objectPlacementSettings;
    }

private:
    std::deque<MapChunkKey> createQueue_;
    std::deque<MapChunkKey> removeQueue_;
    std::deque<MapChunkKey> obstacleQueue_;

    //重複 enqueue 防止
    std::unordered_set<MapChunkKey, MapChunkKeyHash> queuedCreate_;
    std::unordered_set<MapChunkKey, MapChunkKeyHash> queuedRemove_;
    std::unordered_set<MapChunkKey, MapChunkKeyHash> queuedObstacle_;

    //1フレームで処理する上限
    int createBudgetPerFrame_ = 2;
    int removeBudgetPerFrame_ = 2;
    int obstacleBudgetPerFrame_ = 2;

    //必ず初期生成を正しく行わせるために使う
    bool isInitialized_ = false;

    //前フレームの障害物の個数
    int lastObstacleCount_ = -1;

    //移動に合わせて、キューに積んだりする
    void EnqueueDiffIfMoved(const DirectX::XMFLOAT3& pos);
    //作るチャンクをキューに積む
    void EnqueueCreate(const MapChunkKey& key);
    //消すチャンクをキューに積む
    void EnqueueRemove(const MapChunkKey& key);
    //障害物情報をキューに積む
    void EnqueueObstacle(const MapChunkKey& key);

    //X軸方向にプレイヤーが移動したらZ軸のチャンクをまとめて作成するキューに積む
    void EnqueueAddColumn(int cx, int czCenter);

    //X軸方向にプレイヤーが移動したらZ軸のチャンクをまとめて削除するキューに積む
    void EnqueueRemoveColumn(int cx, int czCenter);

    //Z軸方向にプレイヤーが移動したらX軸のチャンクをまとめて作成するキューに積む
    void EnqueueAddRow(int cz, int cxCenter);

    //Z軸方向にプレイヤーが移動したらX軸のチャンクをまとめて削除するキューに積む
    void EnqueueRemoveRow(int cz, int cxCenter);

    //ここで指定されたチャンクを生成処理
    std::vector<SpawnedInstanceInfo> ProcessCreateQueue(const HeightDisplacement& hd);
    //ここで指定されたチャンクを削除処理
    void ProcessRemoveQueue();
    //ここで指定されたチャンクの障害物情報を計算する
    void ProcessObstacleQueue(ComputeShaderManager& cs, Engine::Navigation::NavigationSystem& nav, HeightDisplacement& hd);

    //1チャンク生成
    std::vector<SpawnedInstanceInfo> CreateOneChunk(const MapChunkKey& key, const HeightDisplacement& hd);

    //1チャンク削除処理
    void RemoveOneChunk(const MapChunkKey& key);

    //チャンクが管理範囲内にあるかどうか見てる
    bool IsInsideWindow(const MapChunkKey& key, const MapChunkKey& center) const;
private:
	OnChunkUpdated onChunkUpdated_; //チャンク更新されたときに呼び出されるコールバック関数

    std::shared_ptr<ObjectPlacerManager> objectPlacerManager;
    std::shared_ptr<PerlinNoise> perlin;
    std::vector<UINT> obstacleDataAllChunk;
    int mapId;   //ワールド / レイヤ ID
    int chunkSize; //チャンクサイズ
    int chunkRange; //チャンクの幅（1=3x3, 2=4x4）
    MapChunkKey centerChunkKey; //中心とするチャンクキー
    MapChunkKey oldCenterChunkKey; //中心とするチャンクキー


    //描画するFBXの名前を保持する
	//キー：fbxName
    std::unordered_map<std::wstring, PlacementSetting> objectPlacementSettings;

    //チャンクごとのデータを保持する
    //MapChunkKey : キー
    //ChunkSlot : データ
    //MapChunkKeyHash : MapChunkKeyこれからハッシュ値計算
    std::unordered_map<MapChunkKey, ChunkSlot,MapChunkKeyHash> chunkDatas;




    //チャンク毎のオブジェクト配置情報計算
    //gameSystem : gameSystemクラスをセットする
    //pos : チャンクを計算するのに使うプレイヤーの位置座標
    //chunkSize : チャンクサイズ
    //void UpdateChunkDatas(Game::MainSystem::GameSystem& gameSystem, const std::shared_ptr<ColliAttribute::CollisionAttributeData>& collisionAttributeData, const DirectX::XMFLOAT3& pos, const HeightDisplacement& heightDisplacement);

    //チャンク毎のオブジェクト配置情報計算
    //gameSystem : gameSystemクラスをセットする
    //pos : チャンクを計算するのに使うプレイヤーの位置座標
    //chunkSize : チャンクサイズ
    void UpdateChunkDatas(const DirectX::XMFLOAT3& pos, const HeightDisplacement& heightDisplacement);


    //チャンクデータアップデート
    //void UpdateCalculateChunkDatas(Game::MainSystem::GameSystem& gameSystem, const std::shared_ptr<ColliAttribute::CollisionAttributeData>& collisionAttributeData, const HeightDisplacement& heightDisplacement);

    //チャンクデータアップデート
    void UpdateCalculateChunkDatas(const HeightDisplacement& heightDisplacement);


    void UpdateChunkData();

    //チャンクごとの障害物情報マップ更新
    //computeShaderManager : コンピュートシェーダクラス
    //navigationSystem : ナビゲーションシステムクラス
    //heightDisplacement : ディスプレイスメント情報
    void UpdateObstacleMapPerChunk(ComputeShaderManager& computeShaderManager, Engine::Navigation::NavigationSystem& navigationSystem, HeightDisplacement& heightDisplacement);

    //チャンクごとの障害物情報マップ更新
//computeShaderManager : コンピュートシェーダクラス
//heightDisplacement : ディスプレイスメント情報
    void UpdateObstacleMapPerChunk(ComputeShaderManager& computeShaderManager, HeightDisplacement& heightDisplacement, const std::vector<ObstacleData>& obstacleData);


    //チャンクごとの障害物マッピングを一つにまとめる
    void UpdateObstacleAtlas();

    static inline UINT64 ElemOffset(UINT x, UINT y, UINT atlasW) {
        return (UINT64(y) * atlasW + x) * sizeof(uint32_t);
    }
};