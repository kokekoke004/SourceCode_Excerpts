## 処理の流れ

1. CharacterSystem / CharacterBase  
   各キャラクターの更新処理を実行   

2. NpcController  
   スタミナ、実行中の自由行動、仕事の有無に応じて、
   職業処理と自由行動を切り替える   
   仕事の中断状態を管理し、再開時に職業処理へ伝える   

3. JobSystem / JobBehaviorBase  
   キャラクターの職業に対応する行動を選択し、
   職業ごとの更新処理を実行する  

4. JobType / JobState  
   職業の種類と、職業FSMの現在の作業フェーズを管理する   

5. MinerJobなどの職業別Behavior  
   採掘師では、Idle → ターゲット選択 → 経路探索 → 移動 → 採掘の
   状態遷移を管理する
   木こり・採取職・農家・建築家なども、職業に応じた状態遷移と作業を実行する   

6. HaulerJob  
   建築資材の供給・生産物の運搬・ドロップアイテムの回収から、
   優先度と距離に基づいて作業を選択する   
   資源の予約 → 移動 → 引き出し → 運搬 → 納品などを行う  

7. FreeTimeBehaviorSystem / FreeTimeBehaviorBase  
   自由行動の開始条件と優先度に基づいて、実行する行動を選択する  
   選択した行動の開始・更新・終了を管理し、自由行動中のスタミナを回復する  

8. EatBehavior / WanderBehavior  
   EatBehaviorは、空腹状態と食料の有無を開始条件とし、
   食料の予約 → 経路探索 → 移動 → 引き出し → 食事の状態遷移を管理する   
   WanderBehaviorは、ランダムな目的地の選択 → 経路探索 → 移動 → 待機を管理する   

9. FreeTimeBehaviorUtility  
   自由行動の種類、作業フェーズ、更新結果、優先度などを定義する   

10. JobActionComponent / JobTarget  
    職業処理と自由行動で使用する、ターゲット選択・経路構築・移動などの
    共通処理とターゲット情報を管理する 