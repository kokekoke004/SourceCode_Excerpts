## 処理の流れ

1. **CharacterSystem / CharacterBase** — [`CharacterSystem.h`](01_NPCControl/CharacterSystem.h) / [`CharacterSystem.cpp`](01_NPCControl/CharacterSystem.cpp) / [`CharacterBase.h`](01_NPCControl/CharacterBase.h) / [`CharacterBase.cpp`](01_NPCControl/CharacterBase.cpp) / [`ICharacter.h`](01_NPCControl/ICharacter.h)  
   各キャラクターの更新処理を実行   

2. **NpcController** — [`NpcController.h`](01_NPCControl/NpcController.h) / [`NpcController.cpp`](01_NPCControl/NpcController.cpp)  
   スタミナ、実行中の自由行動、仕事の有無に応じて、
   職業処理と自由行動を切り替える   
   仕事の中断状態を管理し、再開時に職業処理へ伝える   

3. **JobSystem / JobBehaviorBase** — [`JobSystem.h`](02_JobFramework/JobSystem.h) / [`JobSystem.cpp`](02_JobFramework/JobSystem.cpp) / [`JobBehaviorBase.h`](02_JobFramework/JobBehaviorBase.h)  
   キャラクターの職業に対応する行動を選択し、
   職業ごとの更新処理を実行する  

4. **JobType / JobState** — [`JobType.h`](02_JobFramework/JobType.h)  
   職業の種類と、職業FSMの現在の作業フェーズを管理する   

5. **MinerJobなどの職業別Behavior** — [`MinerJob.h`](03_JobFSM/MinerJob.h) / [`MinerJob.cpp`](03_JobFSM/MinerJob.cpp) / [`WoodcutterJob.h`](03_JobFSM/WoodcutterJob.h) / [`WoodcutterJob.cpp`](03_JobFSM/WoodcutterJob.cpp) / [`Gatherer.h`](03_JobFSM/Gatherer.h) / [`Gatherer.cpp`](03_JobFSM/Gatherer.cpp) / [`FarmerJob.h`](03_JobFSM/FarmerJob.h) / [`FarmerJob.cpp`](03_JobFSM/FarmerJob.cpp) / [`BuilderJob.h`](03_JobFSM/BuilderJob.h) / [`BuilderJob.cpp`](03_JobFSM/BuilderJob.cpp)  
   採掘師では、Idle → ターゲット選択 → 経路探索 → 移動 → 採掘の
   状態遷移を管理する
   木こり・採取職・農家・建築家なども、職業に応じた状態遷移と作業を実行する   

6. **HaulerJob** — [`HaulerJob.h`](03_JobFSM/HaulerJob.h) / [`HaulerJob.cpp`](03_JobFSM/HaulerJob.cpp)  
   建築資材の供給・生産物の運搬・ドロップアイテムの回収から、
   優先度と距離に基づいて作業を選択する   
   資源の予約 → 移動 → 引き出し → 運搬 → 納品などを行う  

7. **FreeTimeBehaviorSystem / FreeTimeBehaviorBase** — [`FreeTimeBehaviorSystem.h`](05_FreeBehavior/FreeTimeBehaviorSystem.h) / [`FreeTimeBehaviorSystem.cpp`](05_FreeBehavior/FreeTimeBehaviorSystem.cpp) / [`FreeTimeBehaviorBase.h`](05_FreeBehavior/FreeTimeBehaviorBase.h)  
   自由行動の開始条件と優先度に基づいて、実行する行動を選択する  
   選択した行動の開始・更新・終了を管理し、自由行動中のスタミナを回復する  

8. **EatBehavior / WanderBehavior** — [`EatBehavior.h`](05_FreeBehavior/EatBehavior.h) / [`EatBehavior.cpp`](05_FreeBehavior/EatBehavior.cpp) / [`WanderBehavior.h`](05_FreeBehavior/WanderBehavior.h) / [`WanderBehavior.cpp`](05_FreeBehavior/WanderBehavior.cpp)  
   EatBehaviorは、空腹状態と食料の有無を開始条件とし、
   食料の予約 → 経路探索 → 移動 → 引き出し → 食事の状態遷移を管理する   
   WanderBehaviorは、ランダムな目的地の選択 → 経路探索 → 移動 → 待機を管理する   

9. **FreeTimeBehaviorUtility** — [`FreeTimeBehaviorUtility.h`](05_FreeBehavior/FreeTimeBehaviorUtility.h)  
   自由行動の種類、作業フェーズ、更新結果、優先度などを定義する   

10. **JobActionComponent / JobTarget** — [`JobActionComponent.h`](04_JobAction/JobActionComponent.h) / [`JobActionComponent.cpp`](04_JobAction/JobActionComponent.cpp) / [`JobTarget.h`](04_JobAction/JobTarget.h)  
    職業処理と自由行動で使用する、ターゲット選択・経路構築・移動などの
    共通処理とターゲット情報を管理する 
