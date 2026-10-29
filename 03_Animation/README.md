## 処理の流れ

1. **SkinmeshAnimation** — [`SkinmeshAnimation.h`](01_AnimationPrecompute/SkinmeshAnimation.h) / [`SkinmeshAnimation.cpp`](01_AnimationPrecompute/SkinmeshAnimation.cpp)  
   全フレーム × 全ボーンのスキニング行列をCPU側で事前計算

2. **FbxLoader** — [`FbxLoader.h`](02_GPUUpload/FbxLoader.h) / [`FbxLoader.cpp`](02_GPUUpload/FbxLoader.cpp)  
   ボーン行列・アニメーション情報をGPUへまとめて転送

3. **AnimationController / GpuClipPlayer** — [`AnimationController.h`](03_Playback/AnimationController.h) / [`AnimationController.cpp`](03_Playback/AnimationController.cpp) / [`GpuClipPlayer.h`](03_Playback/GpuClipPlayer.h) / [`GpuClipPlayer.cpp`](03_Playback/GpuClipPlayer.cpp)  
   各インスタンスのアニメーションID・再生フレームを更新

4. **Skin.hlsl** — [`Skin.hlsl`](04_GPUSkinning/Skin.hlsl) / [`peraHeader.hlsli`](04_GPUSkinning/peraHeader.hlsli)  
   対応するボーン行列を参照し、4ウェイトでGPUスキニング 
