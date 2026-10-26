処理の流れ

1. SkinmeshAnimation  
   全フレーム × 全ボーンのスキニング行列をCPU側で事前計算

2. FbxLoader  
   ボーン行列・アニメーション情報をGPUへまとめて転送

3. AnimationController / GpuClipPlayer  
   各インスタンスのアニメーションID・再生フレームを更新

4. Skin.hlsl  
   対応するボーン行列を参照し、4ウェイトでGPUスキニング 