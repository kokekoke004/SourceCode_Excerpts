#pragma once

#include<fbxsdk.h>
#include<vector>
#include<array>
#include<string>
#include<map>
#include<set>
#include <sstream>

#include<d3d12.h>
#include"d3dx12.h"



#pragma comment(lib,"libfbxsdk-md.lib")
#pragma comment(lib,"libxml2-md.lib")
#pragma comment(lib,"zlib-md.lib")
#pragma comment(lib, "d3d12.lib")

#include"helper.h"
#include"GraphicsUtility.h"
#include"CreateResource.h"
#include"FbxPos.h"
#include"Camera.h"
#include"PointLight.h"
#include"loadTexture.h"
#include"DXAppUtillity.h"
#include"ColliderDescUtility.h"
#include"FbxAnimationData.h"

#include<vector>
#include <unordered_map>

//コンスタントバッファー
struct Cbuffer
{
	DirectX::XMMATRIX w;		//ワールド行列
	DirectX::XMMATRIX view;		//ビュー
	DirectX::XMMATRIX proj;		//プロジェクション
	DirectX::XMMATRIX invProj;		//逆プロジェクション

	DirectX::XMMATRIX lightCamera; //ライトからみたビュー
	DirectX::XMFLOAT4 lightDir;	//ライト方向
	DirectX::XMFLOAT4 lightColor;	//ライトカラー
	DirectX::XMFLOAT4 eye;		//視点位置

	//半球ライティングで使用--------------------------
	DirectX::XMFLOAT4 groundColor;	//地面の色
	DirectX::XMFLOAT4 skyColor;	//天球の色
	DirectX::XMFLOAT4 groundNormal; //地面の法線
	//------------------------------------------------
	DirectX::XMFLOAT4 ambient;	//環境光
	DirectX::XMFLOAT4 diffuse;	//拡散反射光
	DirectX::XMFLOAT4 specular;	//鏡面反射光

	DirectX::XMMATRIX bone[kMaxBones]; //ボーン

	//テクスチャのフラグ----------------
	//1=true , 0=false
	int texDiffuseFlag;
	DirectX::XMFLOAT3 pad1;	//パディング1
	int texNormalFlag;
	DirectX::XMFLOAT3 pad2;	//パディング2
	int texSpecularFlag;
	DirectX::XMFLOAT3 pad3;	//パディング3



	Cbuffer()
	{
		ZeroMemory(this, sizeof(Cbuffer));
		for (int i = 0; i < kMaxBones; i++) {
			bone[i] = (DirectX::XMMatrixIdentity());
		}
		texDiffuseFlag = 0;
		texNormalFlag = 0;
		texSpecularFlag = 0;
	}

};

//インスタンスバッファのサイズ
struct InstanceBufferData
{
	std::wstring keyName; //インスタンスバッファキーネーム
	size_t bufferSize;	//インスタンスバッファ全体のサイズ
	size_t dataSize;	//インスタンスバッファの１データのサイズ
	size_t instanceNum; //インスタンスバッファで描画する数

	bool isReCreate=false; //ビュー再生成フラグ(true=再生成 / false = 再生成しない)
};

namespace Engine::Components
{
	struct FbxInstanceHandle;
}


struct RootMeshName {
	std::wstring meshName;
	bool flagName = false;
};



class LoadTex;
class FbxModel;
class ColliderComponent;
class FbxAnimationData;
class FbxAnimationDataLoader;
class FbxAnimatedActor;
class FileManager;
class InstanceBufferManager;
class FrustumCuller;
class ComputeShaderManager;
class CollisionAttributeManager;

namespace Engine::Components
{
	class TransformSystem;
	//安定ハンドル（index + generation）
	struct TransformId;
	struct TransformIdHash;
}

namespace Engine::Assets
{
	class FbxInstanceRegistry;
}

namespace Engine::Model::Lod
{
	struct ModelLodInfo;
}

struct LodResolvedBatch
{
	uint32_t lodLevel = 0;
	std::wstring modelBasePath;
	std::wstring modelPath;
	std::vector<Engine::Components::TransformId> tids;
};

struct LodInstanceBinding
{
	bool isLodManaged = false;          //LOD管理対象か
	std::wstring baseModelPath;         //LODセットの基準モデルパス
	std::wstring activeModelPath;       //今描画中のモデル
	int activeLodLevel = -1;            //現在LOD
};

class FbxLoader
{
public:
	std::shared_ptr<CreateResource> createResource;
	std::shared_ptr<LoadTex> loadTexure;
	std::shared_ptr<ColliderComponent> colliderComponent;
	//コンストラクタ
	FbxLoader(FileManager& fileManager, CollisionAttributeManager& collisionAttributeManager, Engine::Components::TransformSystem& ts, Engine::Assets::FbxInstanceRegistry& fbxInstance, 
		ComPtr<ID3D12Device> device_,
		const char* fileName,
		UINT m_cBSize, UINT m_cBSizeInstance, ComPtr<ID3D12Resource> lightDepthBuffer,
		ComPtr<ID3D12DescriptorHeap> m_descHeap, ControlView* controlView);



	DirectX::XMMATRIX ConvertMatrix(const FbxMatrix& fbxmat);
	DirectX::XMMATRIX FbxAMatConvertToMatrix(const FbxAMatrix& fbxmat);
	void FindContent(FbxScene* pScene, std::vector<FbxNode*>& v);
	void FindContent(FbxNode* pNode, std::vector<FbxNode*>& v);
	char* GetMaterialTextureFilename(fbxsdk::FbxSurfaceMaterial* pMat);
	HRESULT ReadSkinInfo(FbxMesh* pFbxMesh, CustomVertex* pvVB);
	void LoadFbx(const char* fileName);

	//LODセットの読み込み
	void LoadModelLodSet(const std::vector<Engine::Model::Lod::ModelLodInfo> lodSet);

	//FBX描画用のバッファビュー作成
	//FBXのメッシュやマテリアル情報入った、m_mesh
	//FBXの名前
	//FBXの位置情報を持ってるcPos
	//一つの種類のFBX何回描画するか
	void makeFbxBufferView();


	//LODレベルに応じた描画バッチの作成処理
	std::vector<LodResolvedBatch> ResolveLodBatches(
		const std::vector<Engine::Model::Lod::ModelLodInfo>& lodInfos,
		const DirectX::XMFLOAT3& camPos);

	//LODレベルに応じた描画バッチの適用処理
	void ApplyLodBatches(
		const std::vector<LodResolvedBatch>& lodBatche);


	//FBXの描画位置インスタンス描画の更新
	void upDateInstance(std::shared_ptr<FrustumCuller>& frustumCuller, std::shared_ptr<ComputeShaderManager>& computeShaderManager,CameraInfo& camInfo, ShaderInfo& shaderInfo, const std::vector<Engine::Components::TransformId>& objDirtyList, const std::vector<Engine::Components::TransformId>& animDirtyList);

	//描画位置の初期化 モデル描画名や、トランスフォームIDなどを返却する
	//keyDrawFbx : 描画するFBXのキー
	//drawShadow : 影を描画するのか true=描画 , false=描画しない
	//drawType : 描画するものの属性
	//entityType : エンティティタイプ
	//collisionType : コリジョンタイプ
	//animationType : アニメーションタイプ
	Engine::Components::FbxInstanceHandle DrawFbx(const std::wstring& keyDrawFbx, bool drawShadow = true, DrawType drawType = NORMAL, EntityType entityType = OBJECT, 
		AnimationType animationType = NONE, float x = 0.0f, float y = 0.0f, float z = 0.0f, float rX = 0.0f, float rY = 0.0f, float rZ = 0.0f, float sX = 1.0f, float sY = 1.0f, float sZ = 1.0f);


	void AddCapsuleCollider(const std::wstring keyName, const Engine::Components::TransformId& tid,const DirectX::XMFLOAT3& p1,
		const DirectX::XMFLOAT3& p2,
		float radius, EColliderKind colliderKind);

	void AddOBB(const std::wstring& keyDrawFbx, const std::wstring& fbxName, const std::wstring& keyName, const Engine::Components::TransformId& tid, const CollisionType& collisionType,DirectX::XMFLOAT3 m_Pos, const std::array<DirectX::XMFLOAT3, 3>& m_NormaDirect, const std::array<float, 3>& m_fLength, EColliderKind colliderKind);

	//OBBでFBXを覆う
	//keyDrawFbx : FBXキー名
	//keyCollision : コリジョンキー名
	//collisionType : コリジョンのタイプ（オブジェクトを覆うのかなど）
	//m_Pos : コリジョンの位置座標
	//scale : コリジョンのスケール
	//colliderKind : コリジョンの種類
	void WrapFBXWithOBB(const std::wstring& keyDrawFbx, const std::wstring& keyCollision, const Engine::Components::TransformId& tid, const CollisionType& collisionType, DirectX::XMFLOAT3& m_Pos, const DirectX::XMFLOAT3& scale,
		EColliderKind colliderKind = EColliderKind::None, CollisionRegion region = CollisionRegion::Full);

	//カプセルコリジョンでFBXを覆う
	//keyDrawFbx : FBXキー名
	//m_Pos : コリジョンの位置座標
	//idxFbxPos : cPos(FbxPos)のインデックス
	//colliderKind : コリジョンの種類
	void WrapFBXWithCapsule(const std::wstring& keyDrawFbx, const std::wstring& fbxName, const std::wstring& keyCollision, const Engine::Components::TransformId& tid, DirectX::XMFLOAT3& m_Pos,
		EColliderKind colliderKind = EColliderKind::None, CollisionRegion region = CollisionRegion::Full);

	void SetTexture(const std::wstring& oldTexPath, const std::wstring& newTexPath, int handlePos);

	//セッターCbufferサイズ
	void setter_m_cBSize(UINT size)
	{
		this->m_cBSize = alignmentSize(size);
	}

	ControlView* setterContolView()
	{
		return controlView;
	}

	const std::wstring& GetFbxName() const
	{
		return fbxNameW;
	}

	//カプセルコリジョン更新処理
	//colliderKeyName : collidersのキー名
	//pos : 位置座標
	//newRadius : 新しい半径 0よりおおきければ更新
	void UpDateCapsule(const std::wstring& colliderKeyName, const DirectX::XMFLOAT3& pos, float newRadius=-1, CollisionRegion region = CollisionRegion::Full);




	const std::vector<DirectX::XMMATRIX>& GetFrameBoneMatrices(const std::wstring& keyDrawName) const;

	//キーを変更する
	//oldKey : 前のキー
	//newKey : 新しいキー
	void ReNameKey(const std::wstring& oldKey, const std::wstring& newKey);


	//SubMeshゲッター
	//fbxName : fbx名
	//idx : FBXインデックス
	const SubMesh& M_meshes(const std::wstring& fbxName, int idx) const;

	//SubMeshセッター
	//fbxName : fbx名
	//idx : FBXインデックス
	SubMesh& M_meshes(const std::wstring& fbxName, int idx);

	//M_mwshのVectorセッター
	//fbxName : fbx名
	std::vector<SubMesh>& M_meshes(const std::wstring& fbxName);

	//メッシュ総数
	//fbxName : FBX名
	std::size_t GetM_meshesCount(const std::wstring& fbxName) const noexcept;

	//サブメッシュの数
	//fbxName : FBX名
	//idx : インデックス
	std::size_t GetSubMeshesCount(const std::wstring& fbxName, int idx);

	//m_mesh内のサブメッシュゲッター
	//fbxName : FBX名
	//idx : メッシュインデックス
	//idx2 : サブメッシュインデックス
	const SubMesh& SubMeshes(const std::wstring& fbxName, int idx, int idx2) const;

	//m_mesh内のサブメッシュセッター
	//fbxName : FBX名
	//idx : メッシュインデックス
	//idx2 : サブメッシュインデックス
	SubMesh& SubMeshes(const std::wstring& fbxName, int idx, int idx2);

	//サブメッシュの数
	//fbxName : FBX名
	int CountSubMesh(std::wstring fbxName);

	//スキンメッシュアニメーションで使うボーン名ゲッター
	const std::vector<std::wstring>& BoneNameSkinMesh(const std::wstring& fbxName) const;

	//スキンメッシュアニメーションで使うボーン名セッター
	std::vector<std::wstring>& BoneNameSkinMesh(const std::wstring& fbxName);

	//ボーン名、配列のインデックスがボーンの番号ゲッター
	const std::vector<std::wstring>& BoneNamesIdx(const std::wstring& fbxName) const;

	//ボーン名、配列のインデックスがボーンの番号セッター
	std::vector<std::wstring>& BoneNamesIdx(const std::wstring& fbxName);

	//ボーンノードテーブルゲッター
	//fbxName : FBXの名前
	//boneNam :ボーンの名前
	const BoneInfo& BoneNodeTable(const std::wstring& fbxName, std::wstring boneName) const;

	//ボーンノードテーブルセッター
	//fbxName : FBXの名前
	//boneNam :ボーンの名前
	BoneInfo& BoneNodeTable(const std::wstring& fbxName, std::wstring boneName);

	//ボーン名、配列のインデックスがボーンの番号でインデックス指定して名前を取得ゲッター
	//fbxname : FBX名
	//idx : インデックス（ボーンの番号）
	const std::wstring& BoneNamesIdx(const std::wstring& fbxName, size_t idx) const;
	
	//ボーン名、配列のインデックスがボーンの番号でインデックス指定して名前をセットセッター
	//fbxname : FBX名
	//idx : インデックス（ボーンの番号）
	std::wstring& BoneNamesIdx(const std::wstring& fbxName, size_t idx);

	//ボーンテーブルに子要素追加
	//fbxName : FBX名
	//boneNameParent : ボーン親名
	//boneNameChildren : ボーン子名
	void BoneNodeTableAddChild(const std::wstring& fbxName, const std::wstring& boneNameParent, const std::wstring& boneNameChildren);

	//ボーンテーブル子要素に親要素追加
	//fbxName : FBX名
	//boneNameParent : ボーン親名
	//boneNameChildren : ボーン子名
	void BoneNodeTableAddParent(const std::wstring& fbxName, const std::wstring& boneNameParent, const std::wstring& boneNameChildren);

	//BoneNodeTableのボーン名まで
	//fbxName : FBXの名前
	//boneNam :ボーンの名前
	const std::map <std::wstring, BoneInfo>& BoneNodeTable(const std::wstring& fbxName) const;

	//BoneNodeTableのボーン名まで
	//fbxName : FBXの名前
	//boneNam :ボーンの名前
	std::map <std::wstring, BoneInfo>& BoneNodeTable(const std::wstring& fbxName);

	//std::map<std::wstring, std::map <std::wstring, BoneInfo>> _boneNodeTable丸ごと
	const std::map<std::wstring, std::map <std::wstring, BoneInfo>>& BoneNodeTableAll(const std::wstring& fbxName) const;

	//std::map<std::wstring, std::map <std::wstring, BoneInfo>> _boneNodeTable丸ごと
	std::map<std::wstring, std::map <std::wstring, BoneInfo>>& BoneNodeTableAll(const std::wstring& fbxName);

	//ボーンの数
	//fbxName : FBXの名前
	const int& M_numBones(const std::wstring& fbxName)const;

	//ボーンの数
	//fbxName : FBXの名前
	int& M_numBones(const std::wstring& fbxName);

	//Fbxのクラスター
	//fbxName : FBXの名前
	const std::vector<FbxCluster*>& M_clusters(const std::wstring& fbxName) const;

	//Fbxのクラスター
	//fbxName : FBXの名前
	std::vector<FbxCluster*>& M_clusters(const std::wstring& fbxName);

	//Fbxのクラスター
	//fbxName : FBXの名前
	//idx : インデックス
	const FbxCluster* M_clusters(const std::wstring& fbxName, size_t idx) const;

	//Fbxのクラスター
	//fbxName : FBXの名前
	//idx : インデックス
	FbxCluster*& M_clusters(const std::wstring& fbxName, size_t idx);

	//ボーン生成
	//fbxName : FBX名
	const std::vector<Bone>& M_bones(const std::wstring& fbxName)  const;

	//ボーン生成
	//fbxName : FBX名
	std::vector<Bone>& M_bones(const std::wstring& fbxName);

	//Boneゲッター
	//fbxName : FBX名
	//idx : インデックス
	const Bone& M_bones(const std::wstring& fbxName, size_t idx) const;


	//Bone参照
	//fbxName : FBX名
	//idx : インデックス
	Bone& M_bones(const std::wstring& fbxName, size_t idx);

	//FbxBone
	//fbxname : FBX名
	const std::vector<BoneInfo>& FbxBones(const std::wstring& fbxName) const;

	//FbxBone
	//fbxname : FBX名
	std::vector<BoneInfo>& FbxBones(const std::wstring& fbxName);
	//FbxBone
	//fbxname : FBX名
	//idx : インデックス
	const BoneInfo& FbxBones(const std::wstring& fbxName, size_t idx) const;

	//FbxBone
	//fbxname : FBX名
	//idx : インデックス
	BoneInfo& FbxBones(const std::wstring& fbxName, size_t idx);

	//インスタンシング座標更新リソースとビューの作成
	void MakeInstanceUpdatePos(int upDateInsCount);
	//インスタンシング座標更新を更新しGPUへ渡す
	void UpdateInstanceUpdatePos(std::shared_ptr<FrustumCuller>& frustumCuller);

	void MakeInstanceDrawIdxGPU(int drawCount);

	//コリジョンクラスを渡す
	ColliderComponent& ColliderComponentClass()
	{
		return *colliderComponent;
	}

	//コリジョンインデックス取得
	//keyName : コリジョンキーネーム
	int ColliderIdx(const std::wstring& keyName)const;

	//モデルの回転行列作成
	//keyDrawFbx : FBXキー名
	//pitch : X軸回転　度
	//yaw : Y軸回転
	//roll : Z軸回転
	void RotateModel(const std::wstring& keyDrawFbx, float pitch, float yaw, float roll);

	//回転クォータニオン
	//keyDrawFbx : FBXキー名
	//pitch : X軸回転　度
	//yaw : Y軸回転
	//roll : Z軸回転
	void QuaternionRotateModel(const std::wstring& keyDrawFbx, float pitch, float yaw, float roll);



	void UploadInstanceAnimationData(const std::wstring& fbxName);

	//インスタンシングリソースビューを更新する
	void UpDateInstanceResourceView(const std::wstring& fbxName);

	const std::unordered_map <std::wstring, std::unordered_map<std::wstring, std::vector<std::vector<DirectX::XMMATRIX>>>>& GetFrameBoneMatricesAll() const;

	//配置済みのFBXを移動する
	//fbxName : FBXの名前
	//keyDrawFbx : FBXの描画キー
	//x : 移動座標＿X
	//y : 移動座標＿Y
	//z : 移動座標＿Z
	void MoveFbx(const std::wstring& keyDrawFbx, float x, float y, float z);

	//描画する数のセッター
	void setterDrawNum(int num)
	{
		this->drawNum = num;
	}

	void SetFlagCreateConstantRes()
	{
		flagCreateConstantRes = true;
	}

	const bool& GetFlagCreateConstantRes() const
	{
		return flagCreateConstantRes;
	}


	//バッファーサイズセッター
	void  getter_bufferSize(size_t size)
	{
		bufferSize = size;
	}


	//FBXのマテリアルなどの情報を持つリソースのセッター
	ComPtr<ID3D12Resource>& setter_m_constantBuffer()
	{
		return m_constantBuffer;
	}

	//m_cBSizeInstanceのゲッター
	UINT getter_m_cBSizeInstance()
	{
		return this->m_cBSizeInstance;
	}


	//ゲッターCbufferサイズ
	UINT getter_m_cBSize()
	{
		return this->m_cBSize;
	}

	//ゲッターコンストバッファーFBXの色や頂点情報(m_constantBuffer)
	ComPtr<ID3D12Resource> getter_m_constantBuffer()
	{
		return this->m_constantBuffer;
	}


	//アニメーションデータ関係

	//----------------------------------

	//ファイル名をwstrで取得
	std::wstring getterFileName()
	{
		return fileName;
	}

	//アニメーションが存在するかどうか
	bool& GetExsistAnim()
	{
		return exsistAnim[fbxNameW];
	}

	const bool& GetExsistAnim() const
	{
		return exsistAnim.at(fbxNameW);
	}


	std::map<std::wstring, ComPtr<ID3D12Resource>> getter_texResource()
	{
		return this->texResource;
	}

	//バッファーサイズゲッター
	size_t  getter_bufferSize()
	{
		return bufferSize;
	}



	ComPtr<ID3D12Resource> getter_lightDepthBuffer()
	{
		return lightDepthBuffer;
	}

	//FBXヒープインデックス
	//fbxNameW
	int& FbxDrawCount()
	{
		return fbxDrawCount;
	}

	int& GetFbxDrawOldCount()
	{
		return fbxDrawOldCount;
	}

	const int& GetFbxDrawOldCount() const
	{
		return fbxDrawOldCount;
	}


	const ColliderDesc& Collider(int idx) const;

	ColliderDesc& Collider(int idx);

	const ColliderDesc& Collider(const std::wstring& keyName) const;

	ColliderDesc& Collider(const std::wstring& keyName);

	const std::vector<ColliderDesc>& Collider() const;

	std::vector<ColliderDesc>& Collider();

	//OBBのpos更新
	//keyName : コリジョンの名前
	//m_Pos : OBBの位置座標
	void UpDateObbPos(const std::wstring& keyName, DirectX::XMFLOAT3& m_Pos);

	//コリジョン回転をクォータニオンで更新
	//Collision : コリジョン情報
	//dq : クォータニオン
	void RotateObbByDeltaQuat(ColliderDesc& Collision, const DirectX::XMMATRIX& dq);

	//見つからなければ-1を返す
	int& GetfbxKeysIndex(const std::wstring& keyDrawFbx)
	{
		if (!FindKey(keyDrawFbx, fbxKeyIndex))
		{
			printf("\n関数：GetfbxKeysIndex：キーが見つかりませんでした");
			int noneModel = -1;
			return noneModel;
		}
		return fbxKeyIndex[keyDrawFbx];
	}

	const int& GetfbxKeysIndex(const std::wstring& keyDrawFbx) const
	{
		if (!FindKey(keyDrawFbx, fbxKeyIndex))
		{
			ThrrowError("関数：GetfbxKeysIndex：キーが見つかりませんでした");
		}

		return fbxKeyIndex.at(keyDrawFbx);
	}

	std::unordered_map<std::wstring, int>& GetfbxKeysIndex()
	{
		return fbxKeyIndex;
	}

	const std::unordered_map<std::wstring, int>& GetfbxKeysIndex() const
	{
		return fbxKeyIndex;
	}

	

	std::map<std::wstring, std::vector<DirectX::XMMATRIX>>& GetBoneMatrices();
	const std::map<std::wstring, std::vector<DirectX::XMMATRIX>>& GetBoneMatrices() const;
	const std::vector<DirectX::XMMATRIX>& GetBoneMatrices(const std::wstring& keyDrawName) const;


	void UpDateAnim(const std::wstring& fbxName, const std::wstring& keyDrawName, const std::wstring& animationStackName);
	const std::vector <std::vector<DirectX::XMMATRIX>>& GetFrame(const std::wstring& keyDrawName,const std::wstring& animStack) const;
	std::unordered_map <std::wstring, std::unordered_map<std::wstring, std::vector<std::vector<DirectX::XMMATRIX>>>>& GetFrameBoneMatricesAll();
	std::vector<std::wstring>& GetAnimName();

	void CloneAnimationData(const std::shared_ptr<FbxLoader>& source);

	std::unordered_map<std::wstring, std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>>& GetInstanceSrvGpuHandles()
	{
		return instanceSrvCpuHandles;
	}

	const std::unordered_map<std::wstring, std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>>& GetInstanceSrvGpuHandles() const
	{
		return instanceSrvCpuHandles;
	}

	void SetFbxResourceViewCreated(const std::wstring& fbxName)
	{
		fbxResourceViewCreated[fbxName] = true;
	}

	bool GetFbxResourceViewCreated(const std::wstring fbxName)
	{
		if (FindKey(fbxName, fbxResourceViewCreated)) {
			return fbxResourceViewCreated[fbxName];
		}
		return false;
	}

	int& GetCountDraw()
	{
		return countDraw;
	}

	const int& GetCountDraw() const
	{
		return countDraw;
	}

	std::unordered_map<std::wstring, std::wstring>& GetKeyDrawFbxUpdateMap()
	{
		return keyDrawFbxUpdateMap;
	}

	const std::unordered_map<std::wstring, std::wstring>& GetKeyDrawFbxUpdateMap() const
	{
		return keyDrawFbxUpdateMap;
	}

	std::shared_ptr<FbxAnimatedActor>& GetFbxAnimatedActor()
	{
		return fbxAnimatedActor;
	}

	const std::shared_ptr<FbxAnimatedActor>& GetFbxAnimatedActor() const
	{
		return fbxAnimatedActor;
	}

	bool& GetIsUpdateInstance()
	{
		return isUpdateInstance;
	}

	const bool& GetIsUpdateInstance() const
	{
		return isUpdateInstance;
	}



	std::vector<InstanceBufferData>& GetSizeOfInstance()
	{
		return sizeOfInstance;
	}

	const std::vector<InstanceBufferData>& GetSizeOfInstance() const
	{
		return sizeOfInstance;
	}

	const ColliderComponent& GetcolliderComponent() const {
		return *colliderComponent;
	}

	//アニメーション名からアニメーションインデックスを取得する
	//name : アニメーション名
	//outClipId : 出力するアニメーションインデックス
	bool TryGetAnimClipId(const std::wstring& animName, uint32_t& outClipId) const;

	/**
	* @brief モデルの縦幅、横幅、高さを取得する
	*
	* @return モデルの縦幅、横幅、高さ情報
	*/
	const VertexInfo& GetVertSize(CollisionRegion region) const
	{
		return vertSize.at(region);
	}


private:
	bool AddMaterialTexture(FbxSurfaceMaterial* material, const char* propertyName, TypeTexture typeTexture, MyMaterial& outMaterial);

	//TransformIdこれと、cPosのインデックス対応表
	//キーは、対応するｃPosのIDX
	std::unordered_map<Engine::Components::TransformId, int, Engine::Components::TransformIdHash> transformToInstanceIdx_;
	Engine::Components::TransformSystem& transformSystem_;
	FileManager& fileManager;
	CollisionAttributeManager& collisionAttributeManager;
	//trueになればインスタンスを更新する処理を必ず実行する
	bool isUpdateInstance = false;
	//配置したFBXのワールド座標更新リスト
	std::unordered_map<std::wstring, std::wstring> keyDrawFbxUpdateMap;

	//FBXのリソースビュー作成済みか確認する(true=作成済み / false=未作成)
	//wstring : fbxName
	std::unordered_map<std::wstring, bool> fbxResourceViewCreated;
	//インスタンスバッファのヒープ領域保持しておく
	//wstring : fbxName
	std::unordered_map<std::wstring, std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>> instanceSrvCpuHandles;

	//GPUに渡したアニメーションを更新するかどうか(true=更新する, false 更新しない)
	//wstring : fbxNameW
	std::unordered_map<std::wstring, bool> flagUpDateAnimDataForGPU;

	std::vector<InstanceBufferData> sizeOfInstance;

	std::shared_ptr<FbxAnimatedActor> fbxAnimatedActor;
	std::shared_ptr<FbxAnimationData> fbxAnimationData;
	
	std::shared_ptr <InstanceBufferManager> instanceBufferManager;

	Engine::Assets::FbxInstanceRegistry& fbxInstance_;


	//wstring : FBXキー名
	//int : キー名に対応するインデックス
	std::unordered_map<std::wstring, int> fbxKeyIndex;



	//fbxModelのクラス宣言
	std::unordered_map<std::wstring, std::shared_ptr<FbxModel>> fbxModel;

	std::unordered_map<Engine::Components::TransformId, LodInstanceBinding, Engine::Components::TransformIdHash> lodInstanceBinding_;

	void CalcLocalPos();
	//FBXの描画総数
	int fbxDrawCount=0;
	//FBXの前ループ描画数
	int fbxDrawOldCount=0;


	std::shared_ptr<FbxAnimationDataLoader> fbxAnimationDataLoader;
	


	std::wstring fbxNameW; //FBXの名前

	std::unordered_map<CollisionRegion,VertexInfo> vertSize; //頂点データの最大値最小値



	//インスタンス描画のカウント（カリング範囲内の描画オブジェクト数）
	int countDraw=0;

	//バッファービュー作成時のヒープのどこに何が確保されているかをcPosに渡す
	//ループ数
	//確保するヒープのサイズ
	//ヒープの確保を開始する位置
	//ヒープの確保を終了する位置
	int updateManageAddress(int loopNum, int size, int& numHeap, int& endHeap);

	//バッファービュー作成時のヒープのどこに何が確保されているかをcPosに渡す
	//ループ数
	//確保するヒープのサイズ
	//ヒープの確保を開始する位置
	//ヒープの確保を終了する位置
	int manageAddress(int loopNum, int size, int numHeap, int endHeap);


	//バッファーサイズ
	size_t  bufferSize;

	//実際に描画する数
	int drawNum;

	//インスタンス描画で使う構造体
	//b2と対応
	struct InstanceParam
	{
		std::vector<InstanceDraw> data;
	};


	//ファイルの名前
	std::wstring fileName;




	ControlView* controlView;
	//FBXの頂点情報や色情報などを管理しているリソース
	ComPtr<ID3D12Resource> m_constantBuffer;
	ComPtr<ID3D12DescriptorHeap> m_descHeap;
	//FBXのインスタンス描画で使用する(true= 作成済み / false = 未作成)
	bool flagCreateConstantRes = false;
	//ホワイトテクスチャ用リソース
	ComPtr<ID3D12Resource> white;
	//ブラックテクスチャ用リソース
	ComPtr<ID3D12Resource> black;


	std::wstring particleName;



	std::map<std::wstring, bool> exsistAnim;
	std::map<std::wstring, bool> exsistBone;

	std::map<std::wstring, RootMeshName> rootMeshName;

	//FBXに含まれるテクスチャパスの取得
	const char* GetMaterialTextureFileName(fbxsdk::FbxSurfaceMaterial* material, const char* textureType);

	void ExtractMaterialTextures(fbxsdk::FbxSurfaceMaterial* material, SubMesh* pMesh, MyMaterial* pMat);

	//テクスチャ名
	std::vector<std::wstring> texName;
	std::map<std::wstring, ComPtr<ID3D12Resource>> texResource;

	//SK-----------------------------------------------------


	//--------------------------------------------------------
	//FK-------------


		//-------------------------------------------------------


	std::string AttributeTypeToString(FbxNodeAttribute::EType type);

	void PrintNodeAttributes(FbxNode* node);
	void searchBone(FbxNode* node);

	void relateBoneName();


	void boneTreeWrite(int depth);

	void boneTreeReWrite(BoneInfo* node, int depth);


	void boneSetPoint(std::wstring boneName, float rote);







	//接ベクトルと従ベクトルを求める
	void BuildTangentAndBiNormal(SubMesh& subMesh);
	//外積計算
	Vector3 Cross(const Vector3 v0, const Vector3 v1);
	//正規化処理
	void Normalize(Vector3& tangent);


	std::vector<float> animBonePos(FbxAnimCurve* curve, AnimationData* animData,
		std::vector<fbxsdk::FbxTime>* frameTime, std::vector<int>* frameNum);



	std::vector <FbxInfo> fbxInfo;

	int numFbx;
	int numTex;

	bool exBone = false;

	bool exAnim = false;

	//bool unExsistBone = false;

	//FBXファイル名
	std::vector <std::string>fbxName;


	ComPtr<ID3D12Device> m_device;
	std::vector<ComPtr<ID3D12PipelineState>> m_pipeline;
	ComPtr<ID3D12RootSignature> m_rootsignature;
	//Cbufferのサイズ
	UINT m_cBSize;
	//InstanceDrawのサイズ
	UINT m_cBSizeInstance;

	ComPtr<ID3D12Resource> lightDepthBuffer; //ライトデプスバッファ

	std::vector<InstanceDraw> instanceData;	//これをシェーダーに渡す


	//ボーンの長さを取得する
	void calcBoneLength(BoneInfo* node);

	//コリジョンのサイズをFBXの高さの割合で求める
	//高さ0.3にしたら、0.3までの幅でコリジョンを適用する
	//nameW : FBXの名前
	//baseVertSize : FBXの頂点の最大値と最小値の差
	//heightRatio : FBXの高さに対する割合
	VertexInfo CalcColliderVertSizeByHeightRatio(const std::wstring& nameW, const VertexSize& baseVertSize, float heightRatio = 0.3);
};