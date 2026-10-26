#pragma once
#include <vector>
#include<d3d12.h>
#include"d3dx12.h"
#include<DirectXMath.h>
#include<unordered_map>
#include<string>
#include <memory>
#include<chrono>
#include<array>
#include<D3Dcompiler.h>


#pragma comment(lib,"libfbxsdk-md.lib")
#pragma comment(lib,"libxml2-md.lib")
#pragma comment(lib,"zlib-md.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

#include"helper.h"
#include"ColliderDescUtility.h"
#include"NavigationSystemUtility.h"
#include <map>

namespace Engine
{
    class IModelProvider;

    namespace Assets
    {
        class FbxInstanceRegistry;
    }

}



class AstarObstacleRegistry {
public:

    // コンストラクタ
    AstarObstacleRegistry(Engine::IModelProvider& iModelProvider, Engine::Assets::FbxInstanceRegistry& fbxInstance);


    //障害物情報をセットする
    void SetObstacle();

    std::vector<ObstacleData>& GetObstacleData()
    {
        return obData;
    }

    const std::vector<ObstacleData>& GetObstacleData() const
    {
        return obData;
    }

    std::vector<UINT>& GetAstarObstacle()
    {
        return astarObstacle;
    }

    const std::vector<UINT>& GetAstarObstacle() const
    {
        return astarObstacle;
    }


private:
    int width_, height_;
    float cellSize_;
    std::vector<bool> blockedCells_;
    //Aスターアルゴリズム用、侵入可能・不可マッピング済み配列
    std::vector<UINT> astarObstacle;
    std::vector<ColliderDesc> colliders;    //障害物候補FBX
    std::vector<ObstacleData> obData;

    Engine::IModelProvider* iModelProvider_ = nullptr;

    Engine::Assets::FbxInstanceRegistry& fbxInstance_;

    // 障害物を登録（OBB登録情報から）
    //colliders : コリジョン形状情報
    void AddObstacle(const std::vector<ColliderDesc>& colliders);

};
