## 処理の流れ

1. **IGame / Engine** — [`IGame.h`](01_EngineInterface/IGame.h) / [`Engine.h`](01_EngineInterface/Engine.h) / [`Engine.cpp`](01_EngineInterface/Engine.cpp)  
   ゲーム側のライフサイクルをインターフェースで定義し、Engineが初期化・更新・描画を制御

2. **EngineCore** — [`EngineCore.h`](02_EngineCore/EngineCore.h) / [`EngineCore.cpp`](02_EngineCore/EngineCore.cpp)  
   描画・入力・UI・Navigation・Animationなど、エンジン側の各システムを生成・所有

3. **EngineServices** — [`EngineServices.h`](02_EngineCore/EngineServices.h)  
   EngineCoreが所有する機能への参照窓口を提供し、ゲーム側から必要な機能のみ利用

4. **SceneManager / IScene** — [`SceneManager.h`](03_SceneFramework/SceneManager.h) / [`IScene.h`](03_SceneFramework/IScene.h)  
   ゲームシーンを共通インターフェースで登録・生成・切り替え

5. **Game / GamePlayScene** — [`Game.h`](04_GameLayer/Game.h) / [`Game.cpp`](04_GameLayer/Game.cpp) / [`GamePlayScene.h`](04_GameLayer/GamePlayScene.h) / [`GamePlayScene.cpp`](04_GameLayer/GamePlayScene.cpp)  
   IGame / ISceneを実装し、ゲーム固有システムを所有しながらEngineServicesを利用
