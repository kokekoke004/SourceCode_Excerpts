#pragma once
#include<fbxsdk.h>
#include<vector>
#include<array>
#include<string>
#include<map>
#include<set>
#include <sstream>
#include<iostream>
#include<fstream>
#include<filesystem>

#include<d3d12.h>
#include"d3dx12.h"



#pragma comment(lib,"libfbxsdk-md.lib")
#pragma comment(lib,"libxml2-md.lib")
#pragma comment(lib,"zlib-md.lib")
#pragma comment(lib, "d3d12.lib")

#include"helper.h"
#include"FbxAnimationData.h"
#include"FbxPos.h"

class FbxModel;
class BoneMover;
class FileManager;

namespace Engine::Assets
{
	class FbxInstanceRegistry;
}


class SkinmeshAnimation
{
public:
	SkinmeshAnimation(std::shared_ptr<FbxModel>& fbxModel, std::shared_ptr<FbxAnimationData>& fbxAnimationData, FileManager& fileManager, std::shared_ptr<BoneMover>& boneMover, Engine::Assets::FbxInstanceRegistry& fbxInstanceRegistry);
	//スキンメッシュアニメーションの更新処理
	void upDateAnimationSK(const std::wstring& drawFbxName, const std::wstring& animationStackName);

	//スキンメッシュアニメーションのどのアニメーションか指定する
	void setSkinMeshAnim(std::wstring animName);

	void motionUpdate(const std::wstring& drawFbxName, const std::wstring& animationStackName);

	//スキンメッシュアニメーションクラスのメソッドを使って、アニメーションデータをファイルへ書き込む
	//fbxName : FBXのファイル名
	void SaveAnimationData(const std::wstring& filePath);

	//アニメーションリストからアニメーションをロードする
	//fbxName : アニメーションを読み込むFBX
	void LoadAnimationData(const std::wstring& fbxName );

	//アニメーションを指定して再生する
	//fbxName : ロード時のFBX名
	//animationStackName : アニメーションスタック名
	void PlayAnimation(const std::wstring& fbxName, const std::wstring& keyDrawFbx, const std::wstring& animationStackName);

	//CSVファイルに、平行移動量、回転、スケールをそれぞれ書き出す
	void SaveAnimationToPosRoteSCale(const std::wstring& drawFbxName);

	void LoadAnimationDataPosRoteScale(const std::wstring& drawFbxName);

	std::filesystem::path SkinMeshAnimPass(const std::wstring& fbxName);

	//ファイルパスを指定してアニメーションを読み込む
	//fbxName : アニメーションを読み込む対象fbxName
	//animName : アニメーション名
	//animationPass : アニメーション保存パス
	void LoadAnimationFromPath(const std::wstring fbxName, const std::wstring animName, const std::wstring& animationPass);

	std::map<std::wstring, FbxAnimStack*>& GetSkinMeshAnimStack()
	{
		return skinMeshAnim;
	}

	const std::map<std::wstring, FbxAnimStack*>& GetSkinMeshAnimStack() const
	{
		return skinMeshAnim;
	}

private:

	//ベジェ補完を求める
	float getYFromXOnBezier(float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n);

	//アニメーション開始する
	void StartAnimationTime(const std::wstring& animationStackName);

	Engine::Assets::FbxInstanceRegistry& fbxInstanceRegistry_;

	DirectX::XMMATRIX ConvertMatrix(const FbxMatrix& fbxmat);

	std::map<std::wstring, int> frameCountSK;
	std::map<std::wstring, FbxAnimStack*> skinMeshAnim;
	std::shared_ptr<FbxModel>& fbxModel;
	std::shared_ptr<FbxAnimationData>& fbxAnimationData;
	std::shared_ptr<BoneMover>& boneMover;
	FileManager& fileManager;
	AnimationData& GetAnimationData(const std::wstring& stackName, const std::wstring& boneNameW, UINT frameNum);

	const AnimationData& GetAnimationData(const std::wstring& stackName, const std::wstring& boneNameW, UINT frameNum) const;

};