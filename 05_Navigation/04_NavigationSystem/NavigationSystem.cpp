#include"NavigationSystem.h"
#include"AstarObstacleRegistry.h"
#include"AstarPathFinder.h"
#include"ChunkManager.h"
#include"Engine/IModelProvider.h"
#include"Engine/Model/FbxInstanceRegistry.h"

namespace Engine::Navigation 
{
    NavigationSystem::NavigationSystem(IModelProvider& iModelProvider, Assets::FbxInstanceRegistry& fbxInstance)
        :iModelProvider_(&iModelProvider),fbxInstance_(fbxInstance)
    {
        astarObstacleRegistry.reset(new AstarObstacleRegistry(iModelProvider,fbxInstance_));
        astarPathfinder.reset(new AstarPathFinder());

    }

    void NavigationSystem::SetObstacle()
    {
        astarObstacleRegistry->SetObstacle();
    }

    PathResult NavigationSystem::FindPath(ChunkManager& chunkManager, DirectX::XMFLOAT3 start, const PathGoal& goalInfo, const PathQuery& pathQuery, bool isGoalObstacle)
    {
        PathResult path;
        switch (pathQuery.algo)
        {
            case PathAlgo::AStar:
            {
                //AÉXÉ^Å[Ç…ÇÊÇÈåoòHíTçı
                path.points = AStarSearchPerChunk(chunkManager, start, goalInfo, pathQuery, isGoalObstacle);
                if (!path.points.empty()) {
                    path.success = true;
                }
                else {
                    path.success = false;

                }
                return path;

            }
        default:
            break;
        }

        return path;
    }

    void NavigationSystem::FillObstacleData(std::vector<ObstacleData>& out)
    {
        astarObstacleRegistry->GetObstacleData() = out;
    }

    void NavigationSystem::ClearObstacleData()
    {
        astarObstacleRegistry->GetObstacleData().clear();
    }


    const std::vector<ObstacleData>& NavigationSystem::GetObstacleData() const
    {
        return astarObstacleRegistry->GetObstacleData();
    }

    std::vector<std::uint32_t>& NavigationSystem::GetAstarObstacle()
    {
        return astarObstacleRegistry->GetAstarObstacle();
    }

    const std::vector<std::uint32_t>& NavigationSystem::GetAstarObstacle() const
    {
        return astarObstacleRegistry->GetAstarObstacle();
    }

    std::vector<DirectX::XMFLOAT3> NavigationSystem::AStarSearch(const std::vector<std::uint32_t>& astarObstacle, const GridInfo& gridInfo, DirectX::XMFLOAT3 startPos, DirectX::XMFLOAT3 goalPos)
    {
        return astarPathfinder->AStarSearch(astarObstacle, gridInfo, startPos, goalPos);
    }

    std::vector<DirectX::XMFLOAT3> NavigationSystem::AStarSearchPerChunk(ChunkManager& chunkManager, DirectX::XMFLOAT3 startPos, const PathGoal& goalInfo, const PathQuery& pathQuery, bool isGoalObstacle)
    {
        return astarPathfinder->AStarSearchPerChunk(chunkManager, startPos, goalInfo, pathQuery, isGoalObstacle);
    }

    bool NavigationSystem::GetIsClickObstacle()
    {
        return astarPathfinder->GetIsClickObstacle();
    }
}

