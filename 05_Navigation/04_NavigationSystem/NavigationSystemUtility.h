#pragma once

#include<DirectXMath.h>
#include"ColliderDescUtility.h"

class ColliderComponent;

//経路探索アルゴリズムの種類
enum class PathAlgo {
    AStar
};

struct PathQuery {
    PathAlgo algo = PathAlgo::AStar;
    float maxCost = 1e9f;
};

enum class PathGoalType
{
    GroundPoint,    //地面上の1点へ移動
    InteractPoint,  //指定位置の周囲へ移動
    InteractObject  //OBBなどを持つオブジェクトの周囲へ移動
};

struct PathGoal
{
    PathGoalType type = PathGoalType::GroundPoint;

    //地面クリック、または対象オブジェクトの代表位置
    DirectX::XMFLOAT3 goalPosition{};

    //オブジェクトに近づく距離
    float interactRange = 1.5f;

    //対象コリジョンをもとに経路探索を行う場合だけセット
    const ColliderDesc* collision = nullptr;
};


//コンピュートシェーダで送る障害物情報
struct ObstacleData {
    DirectX::XMFLOAT3 position;
    float padding;
    DirectX::XMFLOAT3 halfExtents;
    float padding2;
    DirectX::XMFLOAT4 rotQuat; //クォータニオン

    //DirectX::XMMATRIX rote=DirectX::XMMatrixIdentity();

    ObstacleData()
    {

    }

    ObstacleData(DirectX::XMFLOAT3 pos, DirectX::XMFLOAT3 halfExt) :position(pos), halfExtents(halfExt)
    {

    }

};

struct GridInfo {
    int   width;    // 横ピクセル数
    int   height;   // 縦ピクセル数
    float scaleXZ;  // 1ピクセルあたりのワールド長さ（メートル）
};
