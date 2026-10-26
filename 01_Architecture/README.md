処理の流れ

1. IGame / Engine  
   ゲーム側のライフサイクルをインターフェースで定義し、Engineが初期化・更新・描画を制御

2. EngineCore  
   描画・入力・UI・Navigation・Animationなど、エンジン側の各システムを生成・所有

3. EngineServices  
   EngineCoreが所有する機能への参照窓口を提供し、ゲーム側から必要な機能のみ利用

4. SceneManager / IScene  
   ゲームシーンを共通インターフェースで登録・生成・切り替え

5. Game / GameplayScene  
   IGame / ISceneを実装し、ゲーム固有システムを所有しながらEngineServicesを利用 