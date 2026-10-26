処理の流れ

1. AstarObstacleRegistry  
   モデルのOBBコリジョンから位置・サイズ・回転情報を収集

2. ChunkManager / CSObstacleMapPerChunk.hlsl  
   OBB情報をGPUへ渡し、チャンクごとの通行可能・不可能マップを生成

3. AStarPathfinder  
   チャンクの障害物マップを参照し、8近傍A*で経路を探索

4. NavigationSystem  
   障害物登録と経路探索をまとめ、ゲーム側へFindPathとして提供 