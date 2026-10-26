処理の流れ

1. JobSystem / JobBehaviorBase  
   キャラクターの職業に対応するBehaviorを選択し、職業ごとの更新処理を実行

2. JobType / JobState  
   職業の種類とFSMで使用する現在の作業フェーズを管理

3. MinerJob  
   Idle → Target選択 → 経路探索 → 移動 → 採掘の状態遷移を管理
   その他職業も職業に応じた、状態遷移していく

4. HaulerJob    
   建築資材の供給・ドロップアイテムの回収・生産物の運搬から、優先度と距離に基づいて作業を選択   
   アイテムの予約 → 倉庫への移動 → アイテムの取り出し → 運搬 → 納品などの状態遷移を管理

5. JobActionComponent / JobTarget  
   ターゲット探索・経路構築・移動・攻撃など、各状態で使用する共通処理を実行