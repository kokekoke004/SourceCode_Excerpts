## 処理の流れ

1. **AstarObstacleRegistry** — [`AstarObstacleRegistry.h`](01_ObstacleCollection/AstarObstacleRegistry.h) / [`AstarObstacleRegistry.cpp`](01_ObstacleCollection/AstarObstacleRegistry.cpp)  
   モデルのOBBコリジョンから位置・サイズ・回転情報を収集

2. **ChunkManager / CSObstacleMapPerChunk.hlsl** — [`ChunkManager.h`](02_ObstacleMapping/ChunkManager.h) / [`ChunkManager.cpp`](02_ObstacleMapping/ChunkManager.cpp) / [`CSObstacleMapPerChunk.hlsl`](02_ObstacleMapping/CSObstacleMapPerChunk.hlsl)  
   OBB情報をGPUへ渡し、チャンクごとの通行可能・不可能マップを生成

3. **AStarPathfinder** — [`AStarPathfinder.h`](03_Pathfinding/AStarPathfinder.h) / [`AStarPathfinder.cpp`](03_Pathfinding/AStarPathfinder.cpp)  
   チャンクの障害物マップを参照し、8近傍A*で経路を探索

4. **NavigationSystem** — [`NavigationSystem.h`](04_NavigationSystem/NavigationSystem.h) / [`NavigationSystem.cpp`](04_NavigationSystem/NavigationSystem.cpp) / [`NavigationSystemUtility.h`](04_NavigationSystem/NavigationSystemUtility.h)  
   障害物登録と経路探索をまとめ、ゲーム側へFindPathとして提供 
