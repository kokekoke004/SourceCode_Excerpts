//普通に子ノードまでボーンをたどって数えるとスキンメッシュで用いているボーンでないボーンもカウントするからそういうボーンは除外する必要がある

//ボーン動かすとずれる理由は、ボーンの位置が描画しているＦＢＸの位置とずれているため
//しっかりFBX出力する時にAポーズにしてトランスフォームを適応する
//FBXはFBXの足元にFBXの原点を設定してから出力する

//Blenderで作成したFBX出力する時、コントロール＋Aで全トランスフォーム適応してから出力する
//BlenderでFBXを出力する時、テクスチャマップ、ノーマルマップ、スペキュラーマップをそれぞれマテリアルにこの順番で追加する
//前方ーZ
//上Y　　この設定で出力
//ーーーーーーーーー
//手前ーZ
//奥＋Z
//上＋Y
//下ーY

//このコードは今のところ３枚のテクスチャを一度に描画できる

#include<optional>
#include"FbxLoader.h"
#include"IKSolver.h"
#include"ColliderComponent.h"
#include"FbxModel.h"
#include"FbxAnimationDataLoader.h"
#include"FbxAnimationData.h"
#include"FbxAnimatedActor.h"
#include"FileManager.h"
#include"InstanceBufferManager.h"
#include"FrustumCuller.h"
#include"ComputeShaderManager.h"
#include"CollisionAttributeManager.h"
#include"TransformSystem.h"
#include <algorithm>
#include"Engine/Model/FbxInstanceRegistry.h"
#include"ModelLodData.h"

using namespace std;
using namespace DirectX;

//コンストラクタ
FbxLoader::FbxLoader(FileManager& fileManager, CollisionAttributeManager& collisionAttributeManager,Engine::Components::TransformSystem& ts, Engine::Assets::FbxInstanceRegistry& fbxInstance,
	ComPtr<ID3D12Device> device_,
	const char* fileName,
	UINT m_cBSize, UINT m_cBSizeInstance, ComPtr<ID3D12Resource> lightDepthBuffer,
	ComPtr<ID3D12DescriptorHeap> m_descHeap, ControlView* controlView)
	:fileManager(fileManager), transformSystem_(ts), fbxInstance_(fbxInstance),collisionAttributeManager(collisionAttributeManager)
{
	//コリジョン管理クラスインスタンス化
	colliderComponent.reset(new ColliderComponent(collisionAttributeManager));
	//受け取ったm_cBSizeのサイズをセットする
	this->m_cBSize = m_cBSize;
	//受け取ったm_cBSizeInstanceのサイズをセットする
	this->m_cBSizeInstance = m_cBSizeInstance;
	//FBXの名前をセットする
	std::string name = fileName;
	this->fileName = UTF8_to_UTF16(name);
	this->fbxNameW = this->fileName;
	//ディスクリプタヒープのセット
	this->m_descHeap = m_descHeap;
	this->m_device = device_;
	//ＦＢＸの基本的なライティングなどに関するコンスタントバッファを受け取る
	this->lightDepthBuffer = lightDepthBuffer;
	//インスタンス情報の含まれたコンスタントバッファを受け取る
	//this->m_BufferInstance = m_constantBufferInstance;
	//Viewの領域管理に関する構造体の参照渡し
	this->controlView = controlView;
	//リソース生成クラスのコンストラクタ
	createResource.reset(new CreateResource(this->m_device, this->m_descHeap));

	//インスタンスバッファマネージャクラスインスタンス化
	instanceBufferManager.reset(new InstanceBufferManager(createResource));

	loadTexure.reset(new LoadTex(this->m_device));



	//ＦＢＸのロード処理
	LoadFbx(fileName);
	//アニメーションのための時間のカウントの開始
	//playAnimation();
	//描画する数をセットする
	setterDrawNum(0);

	FbxDrawCount() = 0;
}

void FbxLoader::LoadModelLodSet(const std::vector<Engine::Model::Lod::ModelLodInfo> lodSet)
{
	for (const auto& level : lodSet)
	{
		LoadFbx(level.modelLodPath.c_str());
	}
}

std::vector<LodResolvedBatch> FbxLoader::ResolveLodBatches(
	const std::vector<Engine::Model::Lod::ModelLodInfo>& lodInfos,
	const DirectX::XMFLOAT3& camPos)
{
	std::vector<LodResolvedBatch> result;

	//LODバッチを先に作る
	int lodSize = 0;
	for (const auto& lodInfo : lodInfos) {
		if (fbxNameW != lodInfo.modelBasePath) {
			//LODセットの中に、fbxNameWと同じモデルがない場合はcontinueする
			continue;
		}
		LodResolvedBatch batch;
		batch.modelBasePath = lodInfo.modelBasePath;
		batch.lodLevel = lodInfo.lodLevel;
		batch.modelPath = UTF8_to_UTF16(lodInfo.modelLodPath);
		result.push_back(std::move(batch));
		lodSize++;
	}

	if(lodSize<=0){
		return result;
	}

	auto& batches = fbxInstance_.GetBatches();
	auto it = batches.find(fbxNameW);
	if (it == batches.end()) {
		return result;
	}

	auto& srcBatch = it->second;

	for (const auto& inst : srcBatch.instances) {
		DirectX::XMFLOAT3 pos = transformSystem_.GetLocal(inst.transformId)->pos.xyz;

		float dx = pos.x - camPos.x;
		float dy = pos.y - camPos.y;
		float dz = pos.z - camPos.z;
		float distSq = dx * dx + dy * dy + dz * dz;

		int selectedLodIndex = lodSize - 1; //最遠LODをデフォルト

		for (int i = 0; i < lodSize; ++i) {
			float thresholdSq = lodInfos[i].distanceThreshold * lodInfos[i].distanceThreshold;
			if (distSq < thresholdSq) {
				selectedLodIndex = i;
				break;
			}
		}

		result[selectedLodIndex].tids.push_back(inst.transformId);
	}

	return result;
}

void FbxLoader::ApplyLodBatches(
	const std::vector<LodResolvedBatch>& lodBatches)
{
	if (lodBatches.size() <= 0) {
		return;
	}

	auto& batches = fbxInstance_.GetBatches();
	lodInstanceBinding_.clear();
	//元のベースモデルのバッチを取得
	auto srcIt = batches.find(fbxNameW);
	if (srcIt == batches.end()) {
		return;
	}

	auto& srcBatch = srcIt->second;

	//tidから元インスタンスを引けるようにしておく
	std::unordered_map<Engine::Components::TransformId, decltype(srcBatch.instances)::value_type, Engine::Components::TransformIdHash> srcInstMap;
	srcInstMap.reserve(srcBatch.instances.size());

	for (const auto& inst : srcBatch.instances) {
		srcInstMap.emplace(inst.transformId, inst);
	}

	//いったん各LOD先バッチを空にする
	//base以外だけクリア
	for (const auto& lodBatch : lodBatches) {
		if (lodBatch.modelBasePath == fbxNameW) {
			continue;
		}
		auto& dstBatch = batches[lodBatch.modelPath];
		dstBatch.instances.clear();
	}

	//LODごとに振り分けてコピー
	for (const auto& lodBatch : lodBatches) {

		for (auto tid : lodBatch.tids) {
			auto srcFind = srcInstMap.find(tid);
			if (srcFind == srcInstMap.end()) {
				continue;
			}

			LodInstanceBinding lodInstanceBinding;
			lodInstanceBinding.baseModelPath = lodBatch.modelBasePath;
			lodInstanceBinding.activeLodLevel = lodBatch.lodLevel;
			lodInstanceBinding.activeModelPath = lodBatch.modelPath;
			lodInstanceBinding.isLodManaged = true;
			lodInstanceBinding_[tid] = lodInstanceBinding;

			auto lodInst = srcFind->second;

			//描画キーをLODモデル側に差し替える
			lodInst.keyDrawFbx = lodBatch.modelPath;
			auto& dstBatch = batches[lodBatch.modelPath];

			dstBatch.instances.push_back(std::move(lodInst));

		}
	}
}

//FbxMatrixの行列をXMMATRIX型に変換
DirectX::XMMATRIX FbxLoader::ConvertMatrix(const FbxMatrix& fbxmat)
{
	XMMATRIX mat(
		(float)fbxmat[0][0], (float)fbxmat[0][1], (float)fbxmat[0][2], (float)fbxmat[0][3],
		(float)fbxmat[1][0], (float)fbxmat[1][1], (float)fbxmat[1][2], (float)fbxmat[1][3],
		(float)fbxmat[2][0], (float)fbxmat[2][1], (float)fbxmat[2][2], (float)fbxmat[2][3],
		(float)fbxmat[3][0], (float)fbxmat[3][1], (float)fbxmat[3][2], (float)fbxmat[3][3]
	);
	return mat;
}


//FbxMatrixの行列をXMMATRIX型に変換(GPT推奨版)
DirectX::XMMATRIX FbxLoader::FbxAMatConvertToMatrix(const FbxAMatrix& fbxmat)
{
	return XMMATRIX(
		(float)fbxmat.Get(0, 0), (float)fbxmat.Get(0, 1), (float)fbxmat.Get(0, 2), (float)fbxmat.Get(0, 3),
		(float)fbxmat.Get(1, 0), (float)fbxmat.Get(1, 1), (float)fbxmat.Get(1, 2), (float)fbxmat.Get(1, 3),
		(float)fbxmat.Get(2, 0), (float)fbxmat.Get(2, 1), (float)fbxmat.Get(2, 2), (float)fbxmat.Get(2, 3),
		(float)fbxmat.Get(3, 0), (float)fbxmat.Get(3, 1), (float)fbxmat.Get(3, 2), (float)fbxmat.Get(3, 3)
	);
}



void FbxLoader::FindContent(FbxScene* pScene, std::vector<FbxNode*>& v)
{
	int i;
	FbxNode* lNode = pScene->GetRootNode();

	if (lNode) {
		for (i = 0; i < lNode->GetChildCount(); i++) {
			FindContent(lNode->GetChild(i), v);
		}
	}
}

void FbxLoader::FindContent(FbxNode* pNode, std::vector<FbxNode*>& v)
{
	FbxNodeAttribute::EType lAttributeType;
	int i;

	if (pNode->GetNodeAttribute() == NULL) {
		//ノードがNULL

	}
	else {
		lAttributeType = (pNode->GetNodeAttribute()->GetAttributeType());
		switch (lAttributeType) {
		default:
			break;

		case FbxNodeAttribute::eMesh:
			v.push_back(pNode);
			break;
		}
	}

	for (i = 0; i < pNode->GetChildCount(); i++) {
		FindContent(pNode->GetChild(i), v);
	}
}

char* FbxLoader::GetMaterialTextureFilename(fbxsdk::FbxSurfaceMaterial* pMat)
{
	FbxProperty prop = pMat->FindProperty(fbxsdk::FbxSurfaceMaterial::sDiffuse);
	int layeredTextureCount = prop.GetSrcObjectCount<FbxLayeredTexture>();
	if (layeredTextureCount > 0) {
		//1サーフェースに複数のテクスチャーを貼りテクスチャーブレンドしている場合はこっち
		for (int j = 0; j < layeredTextureCount; j++) {
			FbxLayeredTexture* layered_texture = FbxCast<FbxLayeredTexture>(prop.GetSrcObject<FbxLayeredTexture>(j));
			int lcount = layered_texture->GetSrcObjectCount<FbxTexture>();

			for (int k = 0; k < lcount; k++) {
				FbxTexture* texture = FbxCast<FbxTexture>(layered_texture->GetSrcObject<FbxTexture>(k));
				const char* textureName = texture->GetName();
				FbxFileTexture* textureFile = FbxCast<FbxFileTexture>(prop.GetSrcObject<FbxFileTexture>(j));
				const char* txetureFileName = textureFile->GetRelativeFileName();
			}
		}
	}
	//1サーフェイスに1テクスチャーの場合こっち（FBXデータ内にテクスチャが一枚ってこと）
	else {
		int textureCount = prop.GetSrcObjectCount<FbxTexture>();
		for (int j = 0; j < textureCount; j++) {
			FbxTexture* texture = FbxCast<FbxTexture>(prop.GetSrcObject<FbxTexture>(j));
			const char* textureName = texture->GetName();
			FbxFileTexture* textureFile = FbxCast<FbxFileTexture>(prop.GetSrcObject<FbxFileTexture>(j));
			const char* textureFileName = textureFile->GetRelativeFileName();
			return (char*)textureFileName;
		}
	}
	return NULL;
}

HRESULT FbxLoader::ReadSkinInfo(FbxMesh* pFbxMesh, CustomVertex* pvVB)
{
	//ここで一回全てのボーン初期化しないとおかしな値が入ってしまう事があるから初期化する
	for (int i = 0; i < pFbxMesh->GetControlPointsCount(); i++) {
		pvVB[i].boneIndex[0] = 0;
		pvVB[i].boneIndex[1] = 0;
		pvVB[i].boneIndex[2] = 0;
		pvVB[i].boneIndex[3] = 0;
		pvVB[i].boneWeights[0] = 0.0f;
		pvVB[i].boneWeights[1] = 0.0f;
		pvVB[i].boneWeights[2] = 0.0f;
		pvVB[i].boneWeights[3] = 0.0f;
	}

	int i, k;
	int iNumVertex = pFbxMesh->GetControlPointsCount();	//頂点数
	int* piReadCount = new int[iNumVertex];	//インデックスと重みは最大4つ
	ZeroMemory(piReadCount, sizeof(int) * iNumVertex);

	//デフォーマーを得る
	int numDeformers = pFbxMesh->GetDeformerCount();
	FbxSkin* pSkinInfo = nullptr;

	for (int j = 0; j < numDeformers; j++) {
		FbxDeformer* pDeformer = pFbxMesh->GetDeformer(j);
		if (pDeformer && pDeformer->GetDeformerType() == FbxDeformer::eSkin) {
			pSkinInfo = static_cast<FbxSkin*>(pDeformer);
			break;
		}

	}
	if (!pSkinInfo) {
		// デフォーマーがスキンでない場合
		return E_FAIL;
	}
	std::vector<FbxCluster*> validClusters;
	validClusters.reserve(pSkinInfo->GetClusterCount());

	for (int ci = 0; ci < pSkinInfo->GetClusterCount(); ++ci) {
		auto* cluster = pSkinInfo->GetCluster(ci);
		std::wstring name = UTF8_to_UTF16(cluster->GetName());

		// 名前が "_end" で終わるものはスキップ
		const std::wstring suffix = L"_end";
		if (name.size() >= suffix.size() &&
			name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
		{
			continue;
		}

		validClusters.push_back(cluster);
	}

	//validClusters.size() をボーン数としてセット
	int boneNum = static_cast<int>(validClusters.size());
	M_numBones(fbxNameW) = boneNum;
	M_clusters(fbxNameW).resize(boneNum);

	ThrowMessageWstr(L"\n---------スキンメッシュで使うボーン一覧---------");

	//validClusters を参照してメッセージ／配列を埋める
	for (int i = 0; i < boneNum; ++i) {
		auto* cluster = validClusters[i];
		std::wstring name = UTF8_to_UTF16(cluster->GetName());

		ThrowMessageWstr(L"\nスキンメッシュで使うボーン:" + name);
		BoneNameSkinMesh(fbxNameW).emplace_back(name);
		M_clusters(fbxNameW, i) = cluster;
	}

	ThrowMessageWstr(L"\n総数：" + to_wstring(boneNum));
	ThrowMessageWstr(L"\n----------------------------------------");
	//それぞれのボーンに影響を受ける頂点を調べる　そこから逆に。頂点ベースでボーンインデックス・重みを整頓する
	for (i = 0; i < M_numBones(fbxNameW); i++)
	{
		int iNumIndex = M_clusters(fbxNameW,i)->GetControlPointIndicesCount();	//このボーンで影響を受ける頂点数
		int* piIndex = M_clusters(fbxNameW,i)->GetControlPointIndices();
		double* pdWeight = M_clusters(fbxNameW,i)->GetControlPointWeights();
		//頂点側からインデックスをたどって、頂点サイドで整理する
		for (k = 0; k < iNumIndex; k++) {
			for (int m = 0; m < 4; m++) {
				if (pdWeight[k] > pvVB[piIndex[k]].boneWeights[m]) {
					pvVB[piIndex[k]].boneIndex[m] = i;
					pvVB[piIndex[k]].boneWeights[m] = pdWeight[k];
					break;
				}
			}
			if (piReadCount[piIndex[k]] < 3) {
				piReadCount[piIndex[k]]++;
			}
		}
	}

	//ボーンを生成cPOs
	M_bones(fbxNameW).resize(M_numBones(fbxNameW));
	for (i = 0; i < M_numBones(fbxNameW); i++) {
		FbxAMatrix mat;
		M_clusters(fbxNameW,i)->GetTransformLinkMatrix(mat);
		for (int x = 0; x < 4; x++) {
			for (int y = 0; y < 4; y++) {
				M_bones(fbxNameW,i).bindMatrix.r[y].m128_f32[x] = mat.Get(y, x);
			}
		}

		FbxNode* pBoneNode = M_clusters(fbxNameW,i)->GetLink();
		if (pBoneNode) {
			//ローカル座標取得
			FbxAMatrix local = pBoneNode->EvaluateLocalTransform();

		}
	}
	delete[] piReadCount;

	return S_OK;
}

void FbxLoader::LoadFbx(const char* fileName)
{
	FbxManager* pManager;
	vector<FbxNode*> nodes;
	wstring key = UTF8_to_UTF16(fileName);

	if (fbxModel.find(UTF8_to_UTF16(fileName)) != fbxModel.end()) {
		//すでに読み込んでいるFBXだったら何もしない
		return;
	}

	//FbxModelクラスインスタンス化
	auto fm = std::make_shared<FbxModel>();
	fbxModel[UTF8_to_UTF16(fileName)] = fm;

	const auto& fbxIns = fbxInstance_.GetInstanceRecords(fbxNameW);

	fbxAnimationData.reset(new FbxAnimationData());
	fbxAnimationDataLoader.reset(new FbxAnimationDataLoader(fbxModel[UTF8_to_UTF16(fileName)], fbxAnimationData));
	fbxAnimatedActor.reset(new FbxAnimatedActor(fbxModel[key], fbxAnimationData, fileManager, transformSystem_, fbxInstance_));

	//全てに先立ちFBXマネージャーを生成
	pManager = FbxManager::Create();
	if (!pManager) {
		MessageBoxA(0, "FBXマネージャーを生成できません", "", MB_OK);
		return;
	}

	//FbxOIOSettingsの生成
	FbxIOSettings* ios = FbxIOSettings::Create(pManager, IOSROOT);
	pManager->SetIOSettings(ios);

	FbxString lPath = FbxGetApplicationDirectory();
	pManager->LoadPluginsDirectory(lPath.Buffer());

	//シーン（オブジェクト、カメラ、ライト…etcを含む）の生成
	FbxScene* scene = FbxScene::Create(pManager, "My Scene");
	fbxAnimationData->SetScene(scene);
	auto* pScene = fbxAnimationData->GetScene();

	if (!pScene) {
		MessageBoxA(0, "FBXマネージャーを生成できません", "", MB_OK);
		return;
	}

	string name = fileName;
	wstring nameW = UTF8_to_UTF16(name);
	ThrowMessageWstr(L"\nロードするFBX名 : " + nameW);
	fbxModel[nameW]->GetFbxName() = nameW;
	//アニメーション情報取得オブジェクト
	fbxAnimationData->GetAnimEvaluator()[nameW] = pScene->GetAnimationEvaluator();


	FbxImporter* lImporter = FbxImporter::Create(pManager, "");
	//インポーターを初期化
	const bool lImportStatus = lImporter->Initialize(fileName, -1, ios);

	//本プログラムのFBXバージョン（SDKバージョン）と読み込もうとしているファイルFBXバージョン
	int lSDKMajaor, lSDKMinor, lSDKRevision;
	int lFileMajaor, lFileMinor, lFileRevision;
	FbxManager::GetFileFormatVersion(lSDKMajaor, lSDKMinor, lSDKRevision);
	lImporter->GetFileVersion(lFileMajaor, lFileMinor, lFileRevision);
	char str[512] = { 0 };
	ThrowMessage("本プログラムのFBX ver :" + lSDKMajaor + '.' + lSDKMinor + '.' + lSDKRevision + '\n');
	ThrowMessage("読み込むファイルのFBX ver :" + lFileMajaor + '.' + lFileMinor + '.' + lFileRevision);
	//インポート時のオプション指定(変更したい場合はtrue、falseを適宜変える)
	//読み込む際のＦＢＸの詳細設定
	ios->SetBoolProp(IMP_FBX_MATERIAL, true);
	ios->SetBoolProp(IMP_FBX_TEXTURE, true);
	ios->SetBoolProp(IMP_FBX_LINK, true);
	ios->SetBoolProp(IMP_FBX_SHAPE, true);
	ios->SetBoolProp(IMP_FBX_GOBO, true);
	ios->SetBoolProp(IMP_FBX_ANIMATION, true);
	ios->SetBoolProp(IMP_FBX_GLOBAL_SETTINGS, true);

	//インポート実行
	bool result = lImporter->Import(pScene);




	FindContent(pScene, nodes);
	//三角形のポリゴンに変換して読込
	FbxGeometryConverter cv(pManager);
	cv.Triangulate(pScene, false, false);
	int numNode = nodes.size();
	FbxNode* root_node = pScene->GetRootNode();
	fbxInfo.emplace_back();
	fbxInfo[0].boneNum = -1;
	//ボーンが存在するかどうか
	searchBone(root_node);



	//1つのノードに1つのDXメッシュ（そのDXメッシュの中にさらにマテリアル毎に区分けした複数のサブメッシュ）
	for (int k = 0; k < numNode; k++) {

		SubMesh* pMesh = new SubMesh;
		//FbxNode型からFbxMesh型に変換
		FbxMesh* pFbxMesh = (FbxMesh*)nodes.at(k)->GetNodeAttribute();


		const char* meshName = nodes.at(k)->GetName();
		string meshNameSt = meshName;

		wstring meshNameWs = UTF8_to_UTF16(meshNameSt);
		FbxDouble3 localOrigin = nodes.at(k)->LclTranslation.Get();
		wprintf(L"\n%ls : ", meshNameWs.c_str());
		//FBXの原点コンソール出力
		printf("Local Origin: X=%f, Y=%f, Z=%f",
			localOrigin[0], localOrigin[1], localOrigin[2]);

		pMesh->pMyNode = nodes.at(k);
		pMesh->numTriangles = pFbxMesh->GetPolygonCount();
		int numTri = pFbxMesh->GetPolygonCount();
		pMesh->meshName = meshNameWs;
		//マテリアル
		int materialCount = nodes.at(k)->GetMaterialCount();

		vector<FbxSurfaceMaterial*> materialVector;
		//まずマテリアル情報の配列作成
		for (int i = 0; i < materialCount; i++) {	//1つのマテリアルに1つのDXメッシュ内サブセット
			FbxSurfaceMaterial* pMat = nodes.at(k)->GetMaterial(i);
			materialVector.push_back(pMat);
		}

		//作ったマテリアル情報の配列をもとにしてノードに対応するマテリアル情報にアクセス
		for (int i = 0; i < materialVector.size(); i++) {
			MyMaterial* pMat = new MyMaterial;
			//ambient, diffuse, specularなど
			//おそらくFindProperty(FbxSurfaceMaterial::sAmbient).Get<FbxDouble3>()これでFbxDouble3型のsAmbientをFbxDouble3 ambientに渡している下も同様
			FbxDouble3 ambient = materialVector[i]->FindProperty(FbxSurfaceMaterial::sAmbient).Get<FbxDouble3>();
			pMat->ambient = XMFLOAT4(ambient[0], ambient[1], ambient[2], 1);
			FbxDouble3 diffuse = materialVector[i]->FindProperty(FbxSurfaceMaterial::sDiffuse).Get<FbxDouble3>();
			pMat->diffuse = XMFLOAT4(diffuse[0], diffuse[1], diffuse[2], 1);
			FbxDouble3 specular = materialVector[i]->FindProperty(FbxSurfaceMaterial::sSpecular).Get<FbxDouble3>();
			pMat->specular = XMFLOAT4(specular[0], specular[1], specular[2], 1);

			int textureCount = materialVector[i]->GetSrcObjectCount<FbxTexture>();
			//テクスチャ名取得
			ExtractMaterialTextures(materialVector[i], pMesh, pMat);

			pMesh->materials.push_back(*pMat);
		}
		//このメッシュ内のポリゴンそれぞれがどのマテリアルを貼られているか　マテリアルインデックス
		FbxLayerElementArrayTemplate<int>* pLockable;
		pFbxMesh->GetMaterialIndices(&pLockable);
		if (pLockable != NULL) {
			for (int i = 0; i < numTri; i++) {
				int index = pLockable->GetAt(i);
				MyTriangle* pTri = new MyTriangle;
				pTri->materialIndex = index;
				pMesh->triangles.push_back(*pTri);
			}

		}
		else {
			for (int i = 0; i < numTri; i++) {
				MyTriangle* pTri = new MyTriangle;
				pTri->materialIndex = -1;  // 無効な値として -1 を設定
				pMesh->triangles.push_back(*pTri);
			}
		}
		//m_meshesにＦＢＸの主な情報を渡している
		M_meshes(nameW).push_back(*pMesh);
	}

	VertexSize vertexSizeFull;

	//マテリアル単位でサブセットを作りＤＸメッシュを作成する
	int numMesh = M_meshes(nameW).size();
	for (int k = 0; k < numMesh; k++) {
		//AttributeNodeをFbxMesh型で受け取る
		FbxMesh* pFbxMesh = (FbxMesh*)nodes.at(k)->GetNodeAttribute();

		SubMesh* pRootMesh = &M_meshes(nameW, k);
		//スキン情報（ボーン番号・ウェイト）読込
		//頂点の数だけpvの配列の領域確保
		CustomVertex* pv = new CustomVertex[pFbxMesh->GetControlPointsCount()];
		if (exBone) {
			ReadSkinInfo(pFbxMesh, pv);
		}
		else {
			for (int i = 0; i < pFbxMesh->GetControlPointsCount(); i++) {
				pv[i].boneIndex[0] = 0;
				pv[i].boneIndex[1] = 0;
				pv[i].boneIndex[2] = 0;
				pv[i].boneIndex[3] = 0;
				pv[i].boneWeights[0] = 1.0f;
				pv[i].boneWeights[1] = 0.0f;
				pv[i].boneWeights[2] = 0.0f;
				pv[i].boneWeights[3] = 0.0f;
			}
		}

		//ノードに含まれているマテリアルの数だけ実行
		int numMaterial = nodes.at(k)->GetMaterialCount();
		SubMesh subMesh;
		MyMaterial* pMat = new MyMaterial;


		if (nodes.at(k)->GetMaterialCount() != 0) {
			for (int i = 0; i < nodes.at(k)->GetMaterialCount(); i++) {
				SubMesh subMesh;
				//1マテリアルに1サブメッシュを作る
				//SubMesh subMesh;
				//MyMaterial* pMat = new MyMaterial;
				subMesh.materials.push_back(pRootMesh->materials.at(i));
				char uvSetNme;
				FbxStringList uvSetNames;
				pFbxMesh->GetUVSetNames(uvSetNames);

				for (int j = 0; j < pRootMesh->triangles.size(); j++) {
					if (pRootMesh->triangles.at(j).materialIndex == i) {
						//このポリゴンのマテリアルがこのループ回のもの
						//SubMeshにデータをためていく

						//triangle
						MyTriangle* pTri = new MyTriangle;
						subMesh.triangles.push_back(*pTri);
						subMesh.triangles.at(subMesh.numTriangles) = pRootMesh->triangles.at(j);
						//Vertex x3 Triangleしているので1triangle=3verticesが保障されている
						for (int m = 0; m < 3; m++) {
							//position
							int indexVert = pFbxMesh->GetPolygonVertex(j, m);
							auto* node = pFbxMesh->GetNode();

							//ジオメトリ・オフセット行列
							FbxAMatrix geom(
								node->GetGeometricTranslation(FbxNode::eSourcePivot),
								node->GetGeometricRotation(FbxNode::eSourcePivot),
								node->GetGeometricScaling(FbxNode::eSourcePivot));

							//グローバル行列（Pre/PostRotation 込み）
							FbxAMatrix global = node->EvaluateGlobalTransform();

							//掛け合わせて最終行列
							FbxAMatrix final = global * geom;

							//頂点をシーン空間に
							FbxVector4 v = pFbxMesh->GetControlPointAt(indexVert);
							v = final.MultT(v);   //FBX SDK の行列・頂点積

							CustomVertex* pVert = new CustomVertex;
							pVert->Position.X = v[0];
							pVert->Position.Y = v[1];
							pVert->Position.Z = v[2];

							//各軸の頂点の最大値最小値を求める
							if (v[0] > vertexSizeFull.xMax) {
								vertexSizeFull.xMax = v[0];
							}
							if (v[0] < vertexSizeFull.xMin) {
								vertexSizeFull.xMin = v[0];
							}

							if (v[1] > vertexSizeFull.yMax) {
								vertexSizeFull.yMax = v[1];
							}
							if (v[1] < vertexSizeFull.yMin) {
								vertexSizeFull.yMin = v[1];
							}

							if (v[2] > vertexSizeFull.zMax) {
								vertexSizeFull.zMax = v[2];
							}
							if (v[2] < vertexSizeFull.zMin) {
								vertexSizeFull.zMin = v[2];
							}
							///////////////////////////////////


							//normal
							FbxVector4 normal;
							//normal情報を取得しnormalへ格納
							pFbxMesh->GetPolygonVertexNormal(j, m, normal);
							pVert->Normal.X = normal[0];
							pVert->Normal.Y = normal[1];
							pVert->Normal.Z = normal[2];
							//uv
							bool unmapped = false;

							//これをuv座標に掛けることで拡大できる
							//float scaleU = 0.2f; // 横方向に0.2倍
							//float scaleV = 0.2f; // 縦方向に0.2倍

							//これを足すとテクスチャの描画位置ずらせる
							//float offsetU = 0; // 横方向に+0.1移動
							//float offsetV = 0; // 縦方向に-0.2移動


							FbxVector2 uv;
							//指定したポリゴンjに対する頂点mのUV座標取得
							//uvSetNames.GetStringAt(0)は使用するUVセットの名前取得
							pFbxMesh->GetPolygonVertexUV(j, m, uvSetNames.GetStringAt(0), uv, unmapped);

							//blenderとDirectX側のUV座標が逆だからここでUV修正
							pVert->uv.X = static_cast<float>(uv[0]);
							pVert->uv.Y = 1.0f - static_cast<float>(uv[1]);
							//bone
							pVert->boneIndex[0] = pv[indexVert].boneIndex[0];
							pVert->boneIndex[1] = pv[indexVert].boneIndex[1];
							pVert->boneIndex[2] = pv[indexVert].boneIndex[2];
							pVert->boneIndex[3] = pv[indexVert].boneIndex[3];

							//boneWeight
							pVert->boneWeights[0] = pv[indexVert].boneWeights[0];
							pVert->boneWeights[1] = pv[indexVert].boneWeights[1];
							pVert->boneWeights[2] = pv[indexVert].boneWeights[2];
							pVert->boneWeights[3] = pv[indexVert].boneWeights[3];

							subMesh.vertices.push_back(*pVert);

							//Indices
							int index = subMesh.vertices.size() - 1;
							subMesh.indices.push_back(index);

						}
						subMesh.numTriangles += 1;
					}
				}//SubMesh完了

				//従ベクトルと接ベクトルを求める
				BuildTangentAndBiNormal(subMesh);

				pRootMesh->subMeshes.push_back(subMesh);
			}

		}



	}
	//材料が全てそろった
	for (int i = 0; i < GetM_meshesCount(nameW); i++) {
		int numSubMesh = GetSubMeshesCount(nameW, i);
		for (int j = 0; j < numSubMesh; j++) {
			SubMesh* pMesh = &SubMeshes(nameW, i, j);

			//バーテックスバッファー作成
			int num = pMesh->vertices.size();
			if (num == 0) {
				continue;
			}
			const UINT vertexbufferSize = sizeof(CustomVertex) * pMesh->vertices.size();

			auto vertexheapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto vertexResDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexbufferSize);
			ThrowIfFailed(m_device->CreateCommittedResource(
				&vertexheapProp,
				D3D12_HEAP_FLAG_NONE,
				&vertexResDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(pMesh->pVertexBuffer.ReleaseAndGetAddressOf())));

			UINT8* pVertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);
			ThrowIfFailed(pMesh->pVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, pMesh->vertices.data(), vertexbufferSize);	//ここでuv座標もシェーダーに渡してる
			pMesh->pVertexBuffer->Unmap(0, NULL);
			//バーテックスバッファービュー
			pMesh->vertexBufferView.BufferLocation = pMesh->pVertexBuffer->GetGPUVirtualAddress();
			pMesh->vertexBufferView.StrideInBytes = sizeof(CustomVertex);
			pMesh->vertexBufferView.SizeInBytes = vertexbufferSize;

			//インデックスバッファー生成
			const UINT indexBufferSize = sizeof(int) * pMesh->triangles.size() * 3;
			vertexheapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			vertexResDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
			//頂点バッファーの生成
			ThrowIfFailed(m_device->CreateCommittedResource(
				&vertexheapProp,
				D3D12_HEAP_FLAG_NONE,
				&vertexResDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(pMesh->pIndexBuffer.ReleaseAndGetAddressOf())));

			//インデックスバッファーにインデックスデータを詰め込む
			UINT8* pIndexDataBegin;
			CD3DX12_RANGE readRange2(0, 0);
			pMesh->pIndexBuffer->Map(0, &readRange2, (void**)&pIndexDataBegin);
			memcpy(pIndexDataBegin, pMesh->indices.data(), indexBufferSize);
			pMesh->pIndexBuffer->Unmap(0, nullptr);
			//インデックスバッファビューの生成
			pMesh->indexBufferView.BufferLocation = pMesh->pIndexBuffer->GetGPUVirtualAddress();
			pMesh->indexBufferView.Format = DXGI_FORMAT_R32_UINT;
			pMesh->indexBufferView.SizeInBytes = indexBufferSize;

			//テクスチャ（ここに来る


		}
		//マテリアを持ってないとき
		if (numSubMesh == 0) {
			SubMesh* pMesh = &SubMeshes(nameW, i, 0);

			//バーテックスバッファー作成
			int num = pMesh->vertices.size();
			const UINT vertexbufferSize = sizeof(CustomVertex) * pMesh->vertices.size();

			auto vertexheapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			auto vertexResDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexbufferSize);
			ThrowIfFailed(m_device->CreateCommittedResource(
				&vertexheapProp,
				D3D12_HEAP_FLAG_NONE,
				&vertexResDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(pMesh->pVertexBuffer.ReleaseAndGetAddressOf())));

			UINT8* pVertexDataBegin;
			CD3DX12_RANGE readRange(0, 0);
			ThrowIfFailed(pMesh->pVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, pMesh->vertices.data(), vertexbufferSize);
			pMesh->pVertexBuffer->Unmap(0, NULL);
			//バーテックスバッファービュー
			pMesh->vertexBufferView.BufferLocation = pMesh->pVertexBuffer->GetGPUVirtualAddress();
			pMesh->vertexBufferView.StrideInBytes = sizeof(CustomVertex);
			pMesh->vertexBufferView.SizeInBytes = vertexbufferSize;

			//インデックスバッファー生成
			const UINT indexBufferSize = sizeof(int) * pMesh->triangles.size() * 3;
			vertexheapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			vertexResDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
			//頂点バッファーの生成
			ThrowIfFailed(m_device->CreateCommittedResource(
				&vertexheapProp,
				D3D12_HEAP_FLAG_NONE,
				&vertexResDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(pMesh->pIndexBuffer.ReleaseAndGetAddressOf())));

			//インデックスバッファーにインデックスデータを詰め込む
			UINT8* pIndexDataBegin;
			CD3DX12_RANGE readRange2(0, 0);
			pMesh->pIndexBuffer->Map(0, &readRange2, (void**)&pIndexDataBegin);
			memcpy(pIndexDataBegin, pMesh->indices.data(), indexBufferSize);
			pMesh->pIndexBuffer->Unmap(0, nullptr);
			//インデックスバッファビューの生成
			pMesh->indexBufferView.BufferLocation = pMesh->pIndexBuffer->GetGPUVirtualAddress();
			pMesh->indexBufferView.Format = DXGI_FORMAT_R32_UINT;
			pMesh->indexBufferView.SizeInBytes = indexBufferSize;

			//テクスチャ（ここに来る）

		}

	}
	//ボーン名の表示と、ボーンのparentNoなどの設定
	PrintNodeAttributes(root_node);
	if (exBone) {
		exsistBone[nameW] = true;
		exBone = false;
	}
	//アニメーション関係
	exsistAnim[nameW] = fbxAnimationDataLoader->loadAnimData(pScene);


	//////////////////////////////
	//ボーンの名前とデータを括りつける
	relateBoneName();
	boneTreeWrite(0);
	//アニメーションデータ受け渡し
	if (exsistAnim[nameW]) {
		fbxAnimatedActor->ResiveMotionData(nameW);

	}


	//ホワイトテクスチャの生成
	white = loadTexure->CreateWhiteTexture();

	//ブラックテクスチャの生成
	black = loadTexure->CreateBlackTexture();


	//ローカル座標求める
	CalcLocalPos();

	VertexInfo vertexInfo;

	vertexInfo.width = fabs(vertexSizeFull.xMax - vertexSizeFull.xMin);
	vertexInfo.height = fabs(vertexSizeFull.yMax - vertexSizeFull.yMin);
	vertexInfo.depth = fabs(vertexSizeFull.zMax - vertexSizeFull.zMin);

	vertSize[CollisionRegion::Full] = vertexInfo;

	//コリジョンの割合からサイズを求める
	auto colliderLowerVertSize = CalcColliderVertSizeByHeightRatio(fbxNameW, vertexSizeFull,0.1f);
	vertSize[CollisionRegion::Lower] = colliderLowerVertSize;

	//アニメーションデータをファイルへ書き出す
	fbxAnimatedActor->SaveAnimationData(nameW);

	//fbxAnimatedActor->LoadAnimationData(nameW);

	fbxAnimatedActor->SaveAnimationToPosRoteSCale(nameW);

	fbxAnimatedActor->LoadAnimationDataPosRoteScale(nameW);

	//ボーン総数表示
	ThrowMessage("\nMeshNum:" + to_string(fbxInfo[0].meshNum) + '\n');
	ThrowMessage("BoneNum:" + to_string(fbxInfo[0].boneNum) + '\n');

}

VertexInfo FbxLoader::CalcColliderVertSizeByHeightRatio(const std::wstring& nameW, const VertexSize& baseVertSize, float heightRatio)
{
	VertexSize colliderVertSize;

	colliderVertSize.xMin = FLT_MAX;
	colliderVertSize.yMin = FLT_MAX;
	colliderVertSize.zMin = FLT_MAX;

	colliderVertSize.xMax = -FLT_MAX;
	colliderVertSize.yMax = -FLT_MAX;
	colliderVertSize.zMax = -FLT_MAX;

	//求めた頂点データの最大値最小値をもとにして、コリジョンを適用するサイズを掛けてコリジョンのサイズを決める
	float colliderYMax = baseVertSize.yMin + (baseVertSize.yMax - baseVertSize.yMin) * heightRatio;

	for (int i = 0; i < GetM_meshesCount(nameW); i++) {
		int numSubMesh = GetSubMeshesCount(nameW, i);

		for (int j = 0; j < numSubMesh; j++) {
			SubMesh* pMesh = &SubMeshes(nameW, i, j);

			for (const auto& v : pMesh->vertices) {
				float x = v.Position.X;
				float y = v.Position.Y;
				float z = v.Position.Z;

				if (y < baseVertSize.yMin || y > colliderYMax) {
					continue;
				}

				if (x > colliderVertSize.xMax) {
					colliderVertSize.xMax = x;
				}
				if (x < colliderVertSize.xMin) {
					colliderVertSize.xMin = x;
				}

				if (y > colliderVertSize.yMax) {
					colliderVertSize.yMax = y;
				}	
				if (y < colliderVertSize.yMin) {
					colliderVertSize.yMin = y;
				}

				if (z > colliderVertSize.zMax) {
					colliderVertSize.zMax = z;
				}
				if (z < colliderVertSize.zMin) {
					colliderVertSize.zMin = z;
				}
			}
		}
	}
	VertexInfo vertexInfo;
	vertexInfo.width = fabs(colliderVertSize.xMax - colliderVertSize.xMin);
	//コリジョンの高さは、元のモデルの高さで適用する
	vertexInfo.height = fabs(baseVertSize.yMax - baseVertSize.yMin);
	//vertexInfo.height = fabs(colliderVertSize.yMax - colliderVertSize.yMin);
	vertexInfo.depth = fabs(colliderVertSize.zMax - colliderVertSize.zMin);

	return vertexInfo;

}

void FbxLoader::CalcLocalPos()
{
	//ボーンを生成cPOs
	for (int i = 0; i < M_numBones(fbxNameW); i++) {
		FbxNode* pBoneNode = M_clusters(fbxNameW,i)->GetLink();
		if (pBoneNode) {
			//ローカル座標取得
			FbxAMatrix local = pBoneNode->EvaluateLocalTransform();
			XMMATRIX localMatrix = FbxAMatConvertToMatrix(local);
			FbxBones(fbxNameW,i).localPosMatrix = localMatrix;
		}
	}

}

//接ベクトルと従ベクトルを求める
void FbxLoader::BuildTangentAndBiNormal(SubMesh& subMesh)
{
	//従ベクトルと接ベクトルを求める
	for (int i = 0; i < subMesh.indices.size(); i += 3) {
		int idx0 = subMesh.indices[i];
		int idx1 = subMesh.indices[i + 1];
		int idx2 = subMesh.indices[i + 2];

		CustomVertex& v0 = subMesh.vertices[idx0];
		CustomVertex& v1 = subMesh.vertices[idx1];
		CustomVertex& v2 = subMesh.vertices[idx2];

		Vector3 cp0[] = {
			{v0.Position.X,v0.uv.X,v0.uv.Y},
			{v0.Position.Y,v0.uv.X,v0.uv.Y},
			{v0.Position.Z,v0.uv.X,v0.uv.Y},

		};

		Vector3 cp1[] = {
			{v1.Position.X,v1.uv.X,v1.uv.Y},
			{v1.Position.Y,v1.uv.X,v1.uv.Y},
			{v1.Position.Z,v1.uv.X,v1.uv.Y},

		};

		Vector3 cp2[] = {
			{v2.Position.X,v2.uv.X,v2.uv.Y},
			{v2.Position.Y,v2.uv.X,v2.uv.Y},
			{v2.Position.Z,v2.uv.X,v2.uv.Y},

		};

		//平面パラメータからUV軸座標算出する
		Vector3 tangent, binormal;
		for (int i = 0; i < 3; ++i) {
			auto V1 = cp1[i] - cp0[i];
			auto V2 = cp2[i] - cp1[i];
			auto ABC = Cross(V1, V2);

			if (ABC.X == 0.0f) {
				tangent.v[i] = 0.0f;
				binormal.v[i] = 0.0f;
			}
			else {
				tangent.v[i] = -ABC.Y / ABC.X;
				binormal.v[i] = -ABC.Z / ABC.X;

			}
		}

		Normalize(tangent);		//ここでtangent正規化している
		Normalize(binormal);	//ここでbinormal正規化している

		v0.tangent += tangent;
		v1.tangent += tangent;
		v2.tangent += tangent;

		v0.binormal += binormal;
		v1.binormal += binormal;
		v2.binormal += binormal;
	}

	//ここでもう一度正規化し、ベクトルを平均化する
	for (auto& vert : subMesh.vertices) {
		Normalize(vert.tangent);
		Normalize(vert.binormal);
	}

}

//外積計算
Vector3 FbxLoader::Cross(const Vector3 v0, const Vector3 v1)
{
	XMVECTOR xmv0 = XMVectorSet(v0.X, v0.Y, v0.Z, 0.0f);
	XMVECTOR xmv1 = XMVectorSet(v1.X, v1.Y, v1.Z, 0.0f);
	XMVECTOR xmvr = XMVector3Cross(xmv0, xmv1);
	XMFLOAT3 vec;
	XMStoreFloat3(&vec, xmvr);
	Vector3 vec3(vec.x, vec.y, vec.z);

	return vec3;
}

//正規化処理
void FbxLoader::Normalize(Vector3& tangent)
{
	XMVECTOR xmv = XMVectorSet(tangent.X, tangent.Y, tangent.Z, 0.0f);
	xmv = XMVector3Normalize(xmv);
	XMFLOAT3 vec;
	XMStoreFloat3(&vec, xmv);
	Vector3 vec3(vec.x, vec.y, vec.z);
	tangent = vec3;
}

void FbxLoader::searchBone(FbxNode* node) {
	if (node == nullptr) return;

	int attributeCount = node->GetNodeAttributeCount();
	//std::string countMessage = "Node Attribute Count: " + std::to_string(attributeCount) + '\n';
	//ThrowMessage(countMessage);

	for (int i = 0; i < node->GetNodeAttributeCount(); i++) {
		FbxNodeAttribute* attribute = node->GetNodeAttributeByIndex(i);
		FbxNodeAttribute::EType type = attribute->GetAttributeType();
		if (type == FbxNodeAttribute::eMesh && node->GetParent() == nullptr) {
			//メッシュの名前取得
			string meshName = node->GetName();
			if (meshName.empty()) {
				meshName = "Unnamed Mesh";	//名前ない場合
			}
			if (!rootMeshName[fbxNameW].flagName) {
				RootMeshName rootMesh;
				rootMesh.meshName = UTF8_to_UTF16(meshName);
				rootMesh.flagName = true;
				rootMeshName[fbxNameW] = rootMesh;
			}
		}
		std::string message = "\nattribute type" + AttributeTypeToString(type) + '\n';
		ThrowMessage(message);

		break;

	}


	int num = node->GetChildCount();
	// 再帰的に子ノードを探索
	for (int i = 0; i < node->GetChildCount(); i++) {
		searchBone(node->GetChild(i));
	}
}


//AttributeNode情報をデバッグ画面に表示
void FbxLoader::PrintNodeAttributes(FbxNode* node) {
	if (node == nullptr) return;

	int attributeCount = node->GetNodeAttributeCount();
	//std::string countMessage = "Node Attribute Count: " + std::to_string(attributeCount) + '\n';
	//ThrowMessage(countMessage);

	for (int i = 0; i < node->GetNodeAttributeCount(); i++) {
		FbxNodeAttribute* attribute = node->GetNodeAttributeByIndex(i);
		FbxNodeAttribute::EType type = attribute->GetAttributeType();
		std::string message = "\nattribute type" + AttributeTypeToString(type) + '\n';
		ThrowMessage(message);

		if (type == FbxNodeAttribute::eMesh) {
			//メッシュの名前取得
			string meshName = node->GetName();
			if (meshName.empty()) {
				meshName = "Unnamed Mesh";	//名前ない場合
			}

			fbxInfo[0].meshNum++;
			wstring message = L"No：" + to_wstring(fbxInfo[0].meshNum) + L"Mesh Name：" + UTF8_to_UTF16(meshName) + L"\n";

			ThrowMessageWstr(message);
		}
		if (type == FbxNodeAttribute::eSkeleton) {

			// 親ノードをチェックして parentNo を設定
			FbxNode* parentNode = node->GetParent();
			//スケルトンの名前取得
			string boneName = node->GetName();
			//boneNameSkinMeshを配列の初めから最後まで調べて、boneNameこれと一致するものがあるか調べてる
			auto existBoneSM = find_if(BoneNameSkinMesh(fbxNameW).begin(), BoneNameSkinMesh(fbxNameW).end(), [&](const wstring& b) {
				return b == UTF8_to_UTF16(boneName);
				});
			if (existBoneSM == BoneNameSkinMesh(fbxNameW).end()) {
				continue;
			}

			if (boneName.empty()) {
				boneName = "Unnamed Bone";	//名前ない場合
			}
			wstring message = L"No：" + to_wstring(BoneNamesIdx(fbxNameW).size() + 1) + L"Bone Name：" + UTF8_to_UTF16(boneName) + L"\n";
			PrintWstring(L"\n" + message);
			BoneInfo boneInfo;
			//ボーンの名前セット
			boneInfo.boneName = UTF8_to_UTF16(boneName);
			//親ノードのナンバーセット
			//親スケルトンかどうか
			if (parentNode != nullptr && parentNode->GetNodeAttribute() && node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eSkeleton) {
				//リストfbxBonesを初めから終わりまで調べていき、親ノードの名前に対応するボーンを探す
				auto it = find_if(FbxBones(fbxNameW).begin(), FbxBones(fbxNameW).end(), [&](const BoneInfo& b) {
					return b.boneName == UTF8_to_UTF16(parentNode->GetName());
					});

				if (it != FbxBones(fbxNameW).end()) {
					//リストのなかで、初めからitまで何個要素あるかをこれで取得しparentNoにインデックスとして与える
					boneInfo.parentNo = distance(FbxBones(fbxNameW).begin(), it);
					auto it = find_if(FbxBones(fbxNameW).begin(), FbxBones(fbxNameW).end(), [&](const BoneInfo& b) {
						return b.boneName == UTF8_to_UTF16(boneName);
						});
					//boneInfo.nextNo = distance(fbxBones.begin(), it);

				}
				else {
					boneInfo.parentNo = -1;
				}

			}
			else {
				//ルート
				boneInfo.parentNo = -1;

			}

			// nextNo の設定（現在のボーン数に基づく）
			boneInfo.nextNo = static_cast<int>(FbxBones(fbxNameW).size()) + 1;
			boneInfo.rootNode = node;
			//子ノードのナンバーセット
			BoneNamesIdx(fbxNameW).emplace_back(UTF8_to_UTF16(boneName));


			FbxMatrix globalTransform = node->EvaluateGlobalTransform();
			boneInfo.m = ConvertMatrix(globalTransform);
			XMVECTOR bonePosition = boneInfo.m.r[3];
			// FbxMatrixをXMMATRIXに変換
			boneInfo.startPos.x = XMVectorGetX(bonePosition);
			boneInfo.startPos.y = XMVectorGetY(bonePosition);
			boneInfo.startPos.z = XMVectorGetZ(bonePosition);
			FbxBones(fbxNameW).emplace_back(boneInfo);
			ThrowMessageWstr(message);


		}
		break;

	}


	int num = node->GetChildCount();
	// 再帰的に子ノードを探索
	for (int i = 0; i < node->GetChildCount(); i++) {
		PrintNodeAttributes(node->GetChild(i));
	}
}

std::string FbxLoader::AttributeTypeToString(FbxNodeAttribute::EType type)
{
	switch (type) {
	case FbxNodeAttribute::eNull:
		return "Null";
	case FbxNodeAttribute::eMesh:
		return "Mesh";
	case FbxNodeAttribute::eLight:
		return "Light";
	case FbxNodeAttribute::eCamera:
		return "Camera";
	case FbxNodeAttribute::eSkeleton:
		exBone = true;
		//numBone++;
		return "Skeleton";
	case FbxNodeAttribute::eNurbs:
		return "Nurbs";
	case FbxNodeAttribute::ePatch:
		return "Patch";
	case FbxNodeAttribute::eSubDiv:
		return "Subdivision Surface";
	case FbxNodeAttribute::eMarker:
		return "Marker";
	case FbxNodeAttribute::eLODGroup:
		return "LOD Group";
		// 必要に応じて他の型を追加
	default:
		return "Unknown";
	}
}



void FbxLoader::makeFbxBufferView()
{
	D3D12_CPU_DESCRIPTOR_HANDLE srvHeap_ = m_descHeap->GetCPUDescriptorHandleForHeapStart();
	auto incSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	//int numCbvSrvAll = NUM_CBV + NUM_SRV;
	srvHeap_.ptr += incSize * (ALL_NUM_CBV_SRV)*controlView->endHeap;
	int g = controlView->endHeap;
	bool flagCbv = true;
	int count = 0;
	int countOld = 0;
	int countMesh = 0;
	ThrowMessageWstr(L"\nメッシュ名 : " + fbxNameW + L"------------------------------");
	int countSubMesh = 0;
	HRESULT reason = m_device->GetDeviceRemovedReason();
	wprintf(L"DeviceRemovedReason = 0x%08X\n", reason);

	for (auto& numMesh : M_meshes(fbxNameW)) {
		countSubMesh = numMesh.subMeshes.size();
		for (auto& subMesh : numMesh.subMeshes) {
			//CBVとSTVの数変えても、動的に対応できるようになっている
			int cbvCount = 0;
			while (flagCbv) {
				if (cbvCount == NUM_CBV) {
					flagCbv = false;
					break;
				}


				createResource->CreateConstantBufferView(getter_m_constantBuffer(), srvHeap_, countMesh, getter_m_cBSize());
				ThrowMessageWstr(L"\nCBVアドレス : " + to_wstring(srvHeap_.ptr) + L"メッシュ名 : " + numMesh.meshName);

				countMesh++;
				count++;
				cbvCount++;

				//createResource->CreateConstantBufferView(light[lightNameW][0]->getterLightConstantBuffer(), srvHeap_, 0, light[lightNameW][0]->getterlightInfoSize());
				//ThrowMessageWstr(L"\nCBVアドレス : " + to_wstring(srvHeap_.ptr) + L"ライト名 : " + lightNameW);
				srvHeap_.ptr += incSize;

				controlView->countLight++;
				count++;
				cbvCount++;
				reason = m_device->GetDeviceRemovedReason();
				wprintf(L"DeviceRemovedReason = 0x%08X\n", reason);


				GetInstanceSrvGpuHandles()[fbxNameW].emplace_back(srvHeap_);

				for (int i = 0; i < sizeOfInstance.size(); i++) {
					instanceBufferManager->MakeInstancingResourceView(sizeOfInstance[i].keyName, sizeOfInstance[i].dataSize, srvHeap_, 0);
					//createResource->CreateShaderResourceStructuredBufferView(getter_m_BufferInstance()[i], srvHeap_, sizeOfInstance[i].instanceNum, sizeOfInstance[i].dataSize);
					controlView->endCountInstanceResource++;
					count++;

				}
				reason = m_device->GetDeviceRemovedReason();
				wprintf(L"DeviceRemovedReason = 0x%08X\n", reason);

			}

			int texCnt = 0;
			if (!subMesh.materials.empty()) {
				const auto& material = subMesh.materials.front();

				for (std::size_t i = 0; i < static_cast<std::size_t>(TypeTexture::TYPE_TEX_COUNT); ++i) {
					ComPtr<ID3D12Resource> textureResource = nullptr;
					std::wstring path =L"";
					if (material.texturePass[i].has_value()) {
						path = material.texturePass[i]->textureFileName;

						textureResource = texResource.at(path).Get();
					}

					//テクスチャが存在しない場合
					if(textureResource == nullptr) {
						switch (static_cast<TypeTexture>(i)) {
							case TypeTexture::TEX_DIFFUSE:
							textureResource = white.Get();
							break;

						case TypeTexture::TEX_NORMAL:
							textureResource = white.Get();
							break;

						case TypeTexture::TEX_SPECULAR:
							textureResource = black.Get();
							break;

						default:
							textureResource = white.Get();
							break;
						}
					}

					createResource->CreateShaderResourceView(textureResource, srvHeap_);
					ThrowMessageWstr(L"\nSRVアドレス : " + to_wstring(srvHeap_.ptr) + L"texありメッシュ名 : " + numMesh.meshName);
					ThrowMessageWstr(L"\nsrvに割り当てるテクスチャ名 : " + path);

					count++;
					texCnt++;

					if (count % (ALL_NUM_CBV_SRV) == 0) {
						flagCbv = true;
					}

				}




			}




			D3D12_SHADER_RESOURCE_VIEW_DESC resDesc = {};
			resDesc.Format = DXGI_FORMAT_R32_FLOAT;
			resDesc.Texture2D.MipLevels = 1;
			resDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			resDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

			count++;

			//ライトデプスバッファビューをSRVテクスチャとして作成
			createResource->CreateCustomShaderResourceView(getter_lightDepthBuffer().Get(), resDesc, srvHeap_);

			if (count % (ALL_NUM_CBV_SRV) == 0) {
				flagCbv = true;
			}

		}
	}

	count;
	//controlView->countMesh;


	//FBXリソースビュー作成済みフラグ立てる
	SetFbxResourceViewCreated(fbxNameW);


	//cotrolView->numHeap,endHeapの更新 
	fbxInstance_.SetHeapPos(fbxNameW, updateManageAddress(0, count / (ALL_NUM_CBV_SRV), controlView->numHeap, controlView->endHeap));
	//cotrolView->countInstanceResource,endCountInstanceResourceの更新
	int numIns = updateManageAddress(0, countSubMesh, controlView->countInstanceResource, controlView->endCountInstanceResource);
	fbxInstance_.SetInstanceDrawInfo(fbxNameW, numIns* getter_bufferSize());


	//fbxPos.fbxPosition.x = 1.0f + (k*2);
	//fbxPos.fbxPosition.y = 1.0f;
	//fbxPos.fbxPosition.z = 4.0f;

	//(*cPos)[fbxNameW].emplace_back(fbxPos);


	count = 0;
	flagCbv = true;;
	//countMesh= 0;

}


int FbxLoader::updateManageAddress(int loopNum, int size, int& numHeap, int& endHeap)
{
	const auto& instanceRec = fbxInstance_.GetInstanceRecords(fileName);

	if (loopNum == 0 && (instanceRec).size() == 0) {
		numHeap += 0;
		endHeap += size;
	}
	else if (loopNum == 0 && (instanceRec).size() != 0) {
		numHeap = endHeap;
		endHeap += size;
	}
	else {
		numHeap += size;
		endHeap += size;

	}

	return numHeap;


}

int FbxLoader::manageAddress(int loopNum, int size, int numHeap, int endHeap)
{
	const auto& instanceRec = fbxInstance_.GetInstanceRecords(fbxNameW);


	if (loopNum == 0 && (instanceRec).size() == 0) {
		numHeap += 0;
		endHeap += size;
	}
	else if (loopNum == 0 && (instanceRec).size() != 0) {
		numHeap = endHeap;
		endHeap += size;
	}
	else {
		numHeap += size;
		endHeap += size;

	}


	return numHeap;

}


//ボーンの木構造作成する
void FbxLoader::relateBoneName()
{
	//インデックスと名前の対応関係構築のためにあとで使う
	int count = 0;
	vector<wstring> boneNames(FbxBones(fbxNameW).size());
	//ボーンノードマップを作成する
	for (int idx = 0; idx < FbxBones(fbxNameW).size(); idx++) {
		auto& pd = FbxBones(fbxNameW,idx);
		//boneNames[idx]に自分のボーンの名前与えてる
		boneNames[idx] = pd.boneName;
		auto& node = BoneNodeTable(fbxNameW,pd.boneName);
		node.boneIdx = idx;
		node.boneName = BoneNamesIdx(fbxNameW,idx);
		node.startPos = pd.startPos;
		node.rootNode = pd.rootNode;
		node.m = pd.m;
	}
	//親子関係の構築
	for (auto& pd : FbxBones(fbxNameW)) {
		//親インデックスをチェック（あり得なければ飛ばす）
		if (pd.parentNo >= static_cast<int>(FbxBones(fbxNameW).size()) || pd.parentNo < 0) {
			continue;
		}
		//pd.parentNoの親の名前がparentNameに入る（parentNameの子ノードはpd.parent）
		auto parentName = boneNames[pd.parentNo];
		//子ノードがどれかをアドレスを渡して示している
		string log = "\nNo" + to_string(count) + "  boneName:";
		ThrowMessage(log);
		ThrowMessageWstr(parentName);
		string log2 = "  parentNo:" + to_string(pd.parentNo) + "  nextNumber:" + to_string(pd.nextNo);
		ThrowMessage(log2);
		//parentName親ノードの子ノードとしてpd.boneNameでキーを与えて設定してる
		BoneNodeTableAddChild(fbxNameW, parentName, pd.boneName);
		//親ノードのポインタをセット
		BoneNodeTableAddParent(fbxNameW, parentName, pd.boneName);

		count++;

	}
	fbxAnimationData->GetBoneMatrices()[fbxNameW].resize(FbxBones(fbxNameW).size());

	//ボーンの長さを求める
	for (auto& boneNode : BoneNodeTable(fbxNameW)) {
		calcBoneLength(&boneNode.second);
	}
	//ボーンを全て初期化する
	fill(fbxAnimationData->GetBoneMatrices()[fbxNameW].begin(), fbxAnimationData->GetBoneMatrices()[fbxNameW].end(), XMMatrixIdentity());

}

void FbxLoader::calcBoneLength(BoneInfo* node)
{
	if (node->children.empty()) {
		return;
	}

	for (auto& child : node->children) {
		XMVECTOR parentStartPos = XMLoadFloat3(&node->startPos);
		XMVECTOR childStartPos = XMLoadFloat3(&child->startPos);
		XMVECTOR diff = XMVectorSubtract(parentStartPos, childStartPos);
		//長さ取得
		float length = XMVectorGetX(XMVector3Length(diff));
		child->parentBoneLength = length;
		ThrowMessageWstr(L"\n" + node->boneName + L"ボーンの長さ : " + to_wstring(length));
		calcBoneLength(child);
	}
}


//boneNodeTableに変更する
void FbxLoader::boneTreeWrite(int depth)
{
	if (FbxBones(fbxNameW).empty()) return;
	// _boneNodeTable から BoneInfo を取得
	//auto it = _boneNodeTable[fbxNameW].find(fbxBones[fbxNameW][0].boneName);
	//if (it == _boneNodeTable[fbxNameW].end()) return;
	ThrowMessageWstr(L"\nボーンの木構造表示----------------");
	for (auto parent : BoneNodeTable(fbxNameW)) {
		if (parent.second.parent.empty()) {
			ThrowMessageWstr(L"\n");
			depth = 0;
			auto it = parent.second;
			BoneInfo& node = it;

			// 現在のノードを表示
			wstring indent(depth, L' ');
			ThrowMessageWstr(L"\n" + indent + node.boneName);


			// 子ノードを再帰的に処理
			for (auto& child : node.children) {
				boneTreeReWrite(child, depth + 1); // アドレスを渡す

			}

		}
	}

}

//再起処理で子をたどってく
void FbxLoader::boneTreeReWrite(BoneInfo* node, int depth)
{
	if (!node) return;
	wstring null(depth * 4, L' ');
	// 各ノードを処理
	// 子ノードの名前を表示
	ThrowMessageWstr(L"\n" + null + node->boneName);

	for (auto& child : node->children) {
		// 子ノードがあれば再帰的に処理
		if (!child->children.empty()) {
			boneTreeReWrite(child, depth + 1);
		}
	}
}


//どのボーンどれだけ回転させるか
void FbxLoader::boneSetPoint(wstring boneName, float rote)
{
	auto& nodePos = BoneNodeTable(fbxNameW);


	float x = BoneNodeTable(fbxNameW, boneName).startPos.x;
	float y = BoneNodeTable(fbxNameW, boneName).startPos.y;
	float z = BoneNodeTable(fbxNameW, boneName).startPos.z;

	BoneNodeTable(fbxNameW, boneName).movPos = XMMatrixTranslation(-x, -y, -z)
		* XMMatrixRotationZ(rote)
		* XMMatrixTranslation(x, y, z);

	fbxAnimationData->GetBoneMatrices()[fbxNameW][BoneNodeTable(fbxNameW, boneName).boneIdx] = BoneNodeTable(fbxNameW, boneName).movPos;

}


//FKアニメーションの処理---------------------------------------------------------------------------------------------------------

//アニメーションのフレーム毎の位置・回転・スケールどれか取得
vector<float> FbxLoader::animBonePos(FbxAnimCurve* curve, AnimationData* animData, std::vector<fbxsdk::FbxTime>* frameTime
	, std::vector<int>* frameNum)
{
	vector<float> vec;
	if (curve) {
		int keyCount = curve->KeyGetCount();
		for (int frameIndex = 0; frameIndex < keyCount; ++frameIndex) {
			FbxTime frameTimes = curve->KeyGetTime(frameIndex);
			frameTime->emplace_back(frameTimes);
			frameNum->emplace_back(static_cast<int>(frameTimes.GetFrameCount()));
			float x = curve->KeyGetValue(frameIndex);
			vec.emplace_back(x);
		}
		return vec;
	}
	else {
		ThrowMessageWstr(L"\nアニメーションレイヤーからFbxAnimCurve正しく取得出来てません");
		return{};
	}

}

//------------------------------------------------------------------------------






const char* FbxLoader::GetMaterialTextureFileName(fbxsdk::FbxSurfaceMaterial* material, const char* textureType)
{
	FbxProperty prop = material->FindProperty(textureType);
	if (prop.IsValid()) {
		int textureCount = prop.GetSrcObjectCount<FbxTexture>();
		for (int i = 0; i < textureCount; i++) {
			FbxTexture* texture = prop.GetSrcObject<FbxTexture>(i);
			if (texture) {
				FbxFileTexture* fileTexture = FbxCast<FbxFileTexture>(texture);
				if (fileTexture) {
					return fileTexture->GetFileName();	//テクスチャの名前取得
				}
			}
		}
	}
	return nullptr;
}

void FbxLoader::ExtractMaterialTextures(fbxsdk::FbxSurfaceMaterial* material, SubMesh* pMesh, MyMaterial* pMat)
{

	AddMaterialTexture( material, FbxSurfaceMaterial::sDiffuse, TypeTexture::TEX_DIFFUSE, *pMat);
	

	bool hasNormal = AddMaterialTexture( material, FbxSurfaceMaterial::sNormalMap, TypeTexture::TEX_NORMAL, *pMat);

	if (!hasNormal)
	{
		AddMaterialTexture( material, FbxSurfaceMaterial::sBump, TypeTexture::TEX_NORMAL, *pMat);
	}

	AddMaterialTexture( material, FbxSurfaceMaterial::sSpecularFactor, TypeTexture::TEX_SPECULAR, *pMat);

	//FbxProperty p = material->GetFirstProperty();
	//while (p.IsValid()) {
	//	int n = p.GetSrcObjectCount<FbxFileTexture>();
	//	for (int t = 0; t < n; ++t) {
	//		auto* tex = p.GetSrcObject<FbxFileTexture>(t);
	//		ThrowMessageWstr(L"\nprop: " + UTF8_to_UTF16(p.GetNameAsCStr())
	//			+ L" -> " + UTF8_to_UTF16(tex->GetRelativeFileName()));
	//	}
	//	p = material->GetNextProperty(p);
	//}
	//
	////すべてのテクスチャタイプを探索
	//const char* textureTypes[] = {
	//	FbxSurfaceMaterial::sDiffuse,
	//	FbxSurfaceMaterial::sNormalMap,
	//	FbxSurfaceMaterial::sSpecular,       
	//	FbxSurfaceMaterial::sSpecularFactor,
	//	FbxSurfaceMaterial::sBump,
	//	FbxSurfaceMaterial::sAmbient,
	//	FbxSurfaceMaterial::sEmissive,
	//	FbxSurfaceMaterial::sReflection
	//};

	//for (const char* textureType : textureTypes) {
	//	const char* textureFileName = GetMaterialTextureFileName(material, textureType);
	//	if (textureFileName) {
	//		string textureName = textureFileName;
	//		ThrowMessageWstr(L"\nテクスチャのファイルパス : " + UTF8_to_UTF16(textureName));
	//		pMat->texFlag = true;
	//		TexturePass texPass;
	//		texPass.textureFileName = UTF8_to_UTF16(textureName);
	//		//テクスチャの枚数によって、テクスチャの種類を割り当ててる
	//		//読み込んだテクスチャの順番で上から割り当てている
	//		switch (pMesh->texturePass.size())
	//		{
	//		case 0:
	//			texPass.texType = TEX_DIFFUSE;
	//			break;
	//		case 1:
	//			texPass.texType = TEX_NORMAL;
	//			break;
	//		case 2:
	//			texPass.texType = TEX_SPECULAR;
	//			break;
	//		default:
	//			break;
	//		}
	//		//テクスチャのファイルパス保存
	//		texName.emplace_back(UTF8_to_UTF16(textureName));
	//		texResource[UTF8_to_UTF16(textureName)] = loadTexure->loadTex(UTF8_to_UTF16(textureName));
	//		pMesh->texturePass.emplace_back(texPass);
	//		D3D12_RESOURCE_DESC desc = texResource[UTF8_to_UTF16(textureName)]->GetDesc();
	//		UINT width = static_cast<UINT>(desc.Width);
	//		UINT height = desc.Height;
	//		ThrowMessageWstr(L"\nテクスチャの横の大きさ : " + to_wstring(width));
	//		ThrowMessageWstr(L"\nテクスチャの縦の大きさ : " + to_wstring(height));

	//	}
	//}
}

bool FbxLoader::AddMaterialTexture(FbxSurfaceMaterial* material, const char* propertyName, TypeTexture typeTexture, MyMaterial& outMaterial)
{
	const char* fileName = GetMaterialTextureFileName(material, propertyName);
	//取得したテクスチャが空ではないか
	if (fileName == nullptr) {
		return false;
	}

	TexturePass texturePass;
	texturePass.textureFileName = UTF8_to_UTF16(fileName);
	texturePass.texType = typeTexture;
	const auto path = texturePass.textureFileName;

	//テクスチャのタイプ指定の列挙をそのままベクトルのインデックスに
	const auto index = static_cast<std::size_t>(typeTexture);

	outMaterial.texturePass[index] = std::move(texturePass);


	if (texResource.find(path) == texResource.end()) {
		texResource[path] = loadTexure->loadTex(path);
	}

	return true;
}


void FbxLoader::MoveFbx(const std::wstring& keyDrawFbx, float x, float y, float z)
{
	int idx = IdxFromKey(keyDrawFbx, fbxKeyIndex);
	const auto& instanceRec = fbxInstance_.TryGetInstance(fbxNameW, idx);

	if (instanceRec) {
		auto id = instanceRec->transformId;
		Engine::Components::TransformLocal trans;
		trans.pos = { XMFLOAT3(x,y,z) };
		//ここで座標更新
		transformSystem_.SetLocal(id, trans);


		GetKeyDrawFbxUpdateMap()[keyDrawFbx] = keyDrawFbx;

	}
	else {
		ThrrowError("\nFbxLoader::MoveFbxで存在しないインデックス指定しています。");
	}

	

}



//FBXの描画位置インスタンス描画の更新
void FbxLoader::upDateInstance(std::shared_ptr<FrustumCuller>& frustumCuller, std::shared_ptr<ComputeShaderManager>& computeShaderManager, CameraInfo& camInfo, ShaderInfo& shaderInfo, const std::vector<Engine::Components::TransformId>& objDirtyList, const std::vector<Engine::Components::TransformId>& animDirtyList)
{
	int count = 0;
	//インスタンス描画の数が前回と違うか、インスタンス描画の位置更新があったら、全てのインスタンス描画の位置を更新する
	//GetFbxDrawOldCount() != FbxDrawCount() || GetIsUpdateInstance()
	if (GetFbxDrawOldCount() != FbxDrawCount() || GetIsUpdateInstance()) {
		instanceData.clear();
		GetFbxDrawOldCount() = FbxDrawCount();
		int drawNum = FbxDrawCount();
		XMFLOAT3 eye = camInfo.eye;
		auto ligDir = shaderInfo.lightDir;

		auto& batches = fbxInstance_.GetBatches();
		auto it = batches.find(fbxNameW);
		if (it == batches.end()) return;
		auto& batch = it->second;
		for (auto& fbxPos : batch.instances) {
			GetIsUpdateInstance() = false;

			auto bindIt = lodInstanceBinding_.find(fbxPos.transformId);
			if (bindIt != lodInstanceBinding_.end()) {
				//LOD管理対象なら、今のローダーが担当するモデル以外は飛ばす
				if (bindIt->second.activeModelPath != fbxNameW) {
					continue;
				}
			}

			//float x = i*2.0f;
			//float y = 1.0f;
			//float z = i * 4.0f;
			InstanceDraw instance;

			if (fbxPos.drawType == PARTICLE) {
				//パーティクルのビルボード処理
				//XMVECTOR posXMV = XMVectorSet(fbxPos.fbxPosition.x, fbxPos.fbxPosition.y, fbxPos.fbxPosition.z, 1.0f);
				XMVECTOR camPos = XMVectorSet(eye.x, eye.y, eye.z, 1.0f);
				//XMMATRIX rotation = XMMatrixRotationRollPitchYaw(fbxPos.fbxRotetion.x, fbxPos.fbxRotetion.y, fbxPos.fbxRotetion.z);
				//for (auto particle : getter_particle()) {
				//	//particle->getBillBoardRotation(posXMV, camPos, rotation, camInfo);	//ビルボードの回転を求める
				//}
				//XMMATRIX rotationFBX = XMMatrixRotationRollPitchYaw(fbxPos.fbxRotetion.x, fbxPos.fbxRotetion.y, fbxPos.fbxRotetion.z);	//通常のFBXの回転を求める
				//instance.world = XMMatrixTranspose(rotation * rotationFBX * XMMatrixTranslation(fbxPos.fbxPosition.x, fbxPos.fbxPosition.y, fbxPos.fbxPosition.z));	//ワールド行列を渡す

			}
			else {
				instance.world = transformSystem_.GetWorldMatrixGpu(fbxPos.transformId);	//ワールド行列

			}

			instance.idxInsGPU = IdxFromKey(fbxPos.keyDrawFbx, GetfbxKeysIndex());

			if (fbxPos.animationType != NONE) {
				UINT animFrame = fbxPos.animState.frame;
				instance.animationId = fbxPos.animState.clipId;
				instance.frameIndex = animFrame;

			}

			instanceData.emplace_back(instance);
			transformToInstanceIdx_[fbxPos.transformId] =
				static_cast<int>(instanceData.size()) - 1;
			count++;
		}


		//GetCountDraw() = count;
		instanceBufferManager->UpDateInstance(sizeOfInstance[0].keyName, instanceData.data(), instanceData.size());
		if (count > 0) {
			MakeInstanceDrawIdxGPU(count);
		}

		return;
	}
	else {
		GetFbxDrawOldCount() = FbxDrawCount();
	}

	vector<InstanceDrawUpdateData> instanceDrawUpdateData;	//これをシェーダーに渡す
	InstanceDrawUpdateData insUpdateData;

	std::vector<Engine::Components::TransformId> dirtyListAll = objDirtyList;
	//ここでDirtyList結合する
	dirtyListAll.insert(dirtyListAll.end(), animDirtyList.begin(), animDirtyList.end());

	instanceDrawUpdateData.reserve(dirtyListAll.size());
	int insSize = dirtyListAll.size();

	for (auto tid : dirtyListAll)
	{
		MakeInstanceUpdatePos(insSize);

		auto it = transformToInstanceIdx_.find(tid);
		if (it == transformToInstanceIdx_.end()) continue;

		int idx = it->second;
		

		auto& batches = fbxInstance_.GetBatches();
		auto itb = batches.find(fbxNameW);
		if (itb == batches.end()) return;
		auto& batch = itb->second;

		if (batch.instances.size() <= idx) {
			continue;
		}

		const auto& fbxPos = batch.instances[idx];


		insUpdateData.updateidx = idx;
		insUpdateData.world = transformSystem_.GetWorldMatrixGpu(tid);

		if (fbxPos.animationType != NONE)
		{

			UINT animFrame = fbxPos.animState.frame;
			insUpdateData.animationId = fbxPos.animState.clipId;
			insUpdateData.frameIndex = animFrame;

		}

		if (instanceData.size() <= insUpdateData.updateidx) {
			continue;
		}

		instanceData[insUpdateData.updateidx].world = insUpdateData.world;
		instanceData[insUpdateData.updateidx].animationId = insUpdateData.animationId;
		instanceData[insUpdateData.updateidx].frameIndex = insUpdateData.frameIndex;

	}

	//インスタンス描画の位置更新があったら、全てのインスタンス描画の位置を更新する
	if (insSize > 0) {

		instanceBufferManager->UpDateInstance(sizeOfInstance[0].keyName, instanceData.data(), instanceData.size());

	}

	//コリジョン更新もここでする
	colliderComponent->UpdateFromDirty(dirtyListAll,transformSystem_);

	GetKeyDrawFbxUpdateMap().clear();

}

void FbxLoader::UpdateInstanceUpdatePos(std::shared_ptr<FrustumCuller>& frustumCuller)
{
	unordered_map<wstring, ColliderDesc> keyDrawFbxAndCollider;
	for (const auto& obb : Collider()) {
		if (obb.collisionType == CollisionType::WHOLE_OBJECT) {
			keyDrawFbxAndCollider[obb.keyDrawFbx] = obb;
		}
	}

	int count = 0;
	std::vector<InstanceDrawIdxGPU> insDrawIdxGPU;
	InstanceDrawIdxGPU insIdx;
	const auto& instanceRec = fbxInstance_.GetInstanceRecords(fbxNameW);

	int idx = 0;
	for (auto& fbxPos : instanceRec) {
		if (fbxPos.entityType == DEBUG) {
			idx++;
			return;
		}

		if (auto it = keyDrawFbxAndCollider.find(fbxPos.keyDrawFbx);
			it != keyDrawFbxAndCollider.end())
		{
			//フラスタムカリング判定
			bool is = frustumCuller->IsVisibleObb(it->second);
			fbxInstance_.SetIsCullingVisible(fbxNameW, idx, is);

			if (fbxPos.isDraw) {
				int idx = ColliderIdx(it->second.collisionName);
				Collider()[idx].isDebugDraw = true;
			}

		}

		if (!fbxPos.isDraw || !fbxPos.isCullingVisible) {
			idx++;
			continue;	//描画しないFBXはスキップ
		}

		insIdx.drawIdxGPU = GetfbxKeysIndex(fbxPos.keyDrawFbx);
		insDrawIdxGPU.emplace_back(insIdx);
		count++;
		idx++;
	}
	GetCountDraw() = count;

	vector<InstanceDrawUpdateData> instanceDrawUpdateData;	//これをシェーダーに渡す
	InstanceDrawUpdateData insUpdateData;
	//ここで、インスタンシングバッファ用のリソース作成
	int insSize = GetKeyDrawFbxUpdateMap().size();
	if (GetCountDraw() > 0) {
		instanceBufferManager->UpDateInstance(sizeOfInstance[2].keyName, insDrawIdxGPU.data(), insDrawIdxGPU.size());

	}


}

void FbxLoader::UploadInstanceAnimationData(const std::wstring& fbxName)
{
	if (FindKey(fbxName, flagUpDateAnimDataForGPU)) {
		flagUpDateAnimDataForGPU[fbxName] = false;
	}
	else {
		flagUpDateAnimDataForGPU[fbxName] = true;
	}

	if (flagUpDateAnimDataForGPU[fbxName]) {
		vector<XMMATRIX> animMat;
		int animFrameLast = 0;
		//アニメーション格納順でボーンのアニメーション情報一覧に書き込んでく
		for (const auto& animStackName : fbxAnimationData->GetAnimName()) {
			AnimationInfo animInfo;
			const auto& animStack = GetFrameBoneMatricesAll().at(fbxName).at(animStackName);

			//アニメーションのフレーム数取得
			animInfo.frameCount = static_cast<UINT>(animStack.size());
			
			fbxAnimationData->GetAnimName();
			for (const auto& keyFrame : animStack) {
				for (const auto& animation : keyFrame) {
					animMat.emplace_back(animation);

				}
			}
			//アニメーションの開始フレームをセットする
			animInfo.offsetAnim = animFrameLast;
			animFrameLast += animInfo.frameCount;
			fbxAnimationData->GetAnimInfo().emplace_back(animInfo);
			int size = fbxAnimationData->GetAnimInfo().size();
		}
		//ボーンアニメーションデータをまとめてGPUへ送る
		instanceBufferManager->UpDateInstance(sizeOfInstance[3].keyName, animMat.data(), animMat.size());
		//ボーンアニメーションデータをまとめてGPUへ送ったものに個別にアクセスするため、アニメーションオフセットや合計のキーフレーム数などを送る
		instanceBufferManager->UpDateInstance(sizeOfInstance[4].keyName, fbxAnimationData->GetAnimInfo().data(), fbxAnimationData->GetAnimInfo().size());

	}

}

Engine::Components::FbxInstanceHandle FbxLoader::DrawFbx(const std::wstring& keyDrawFbx,bool drawShadow, DrawType drawType, EntityType entityType ,
	AnimationType animationType, float x, float y, float z, float rX, float rY, float rZ, float sX, float sY, float sZ)
{
	if (FindKey(keyDrawFbx, fbxKeyIndex))
	{
		ThrrowError("DrawFbxで、キー名が被っています");
	}

	//FBXに対応するキー名とインデックス登録キーfbxNameが見つかりませんでした
	fbxKeyIndex[keyDrawFbx] = FbxDrawCount();
	//FBXで使用するリソースの作成
	//FBXの頂点情報などが入っている
	//用いるリソースを作成する
	FbxDrawCount()++;
	fbxDrawCount;
	//インスタンス描画で使用するバッファーサイズ設定
	getter_bufferSize(getter_m_cBSizeInstance() * FbxDrawCount());
	//ここで、FBXのクラス毎に、インスタンス描画の座標を受け取るリソースを生成している


	vector<ComPtr<ID3D12Resource>> insRes;

	//FBX描画専用のストラクチャーバッファ
	vector<InstanceBufferData> sizeIns;
	InstanceBufferData insBufferData;
	insBufferData.bufferSize = m_cBSizeInstance * FbxDrawCount();
	insBufferData.dataSize = m_cBSizeInstance;
	insBufferData.instanceNum = FbxDrawCount();
	insBufferData.keyName = L"FBXIns";

	sizeIns.emplace_back(insBufferData);
	instanceBufferManager->MakeInstancingResource(insBufferData.keyName, insBufferData.dataSize, insBufferData.instanceNum);
	insRes.emplace_back(createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, sizeIns[0].bufferSize, D3D12_RESOURCE_STATE_GENERIC_READ));

	const auto& animNames = fbxAnimationData->GetAnimName();
	int totalKeyFrame = 0;

	//登録されたインスタンスバッファ数が０であれば（初回時のみ実行）
	if (!GetFlagCreateConstantRes()) {
		SetFlagCreateConstantRes();
		setter_m_constantBuffer() = createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, getter_m_cBSize() * CountSubMesh(fbxNameW), D3D12_RESOURCE_STATE_GENERIC_READ);

		InstanceBufferData insBufferData;
		UINT sizeUpdateIns = sizeof(InstanceDrawUpdateData);
		//初期値は1で作成する0にするとエラーになるから
		int initVal = 1;
		insBufferData.bufferSize = sizeUpdateIns * initVal;
		insBufferData.dataSize = sizeUpdateIns;
		insBufferData.instanceNum = initVal;
		insBufferData.keyName = L"FBXInsUpdate";

		sizeIns.emplace_back(insBufferData);
		instanceBufferManager->MakeInstancingResource(insBufferData.keyName, insBufferData.dataSize, insBufferData.instanceNum);

		InstanceDrawIdxGPU instanceDrawIdxGPU;
		UINT sizeinstanceDrawIdxGPU = sizeof(InstanceDrawIdxGPU);
		insBufferData.bufferSize = sizeinstanceDrawIdxGPU * initVal;
		insBufferData.dataSize = sizeinstanceDrawIdxGPU;
		insBufferData.instanceNum = initVal;
		insBufferData.keyName = L"FBXInsIdxGPU";

		sizeIns.emplace_back(insBufferData);
		instanceBufferManager->MakeInstancingResource(insBufferData.keyName, insBufferData.dataSize, insBufferData.instanceNum);


		if (GetFrameBoneMatricesAll().find(fbxNameW)!= GetFrameBoneMatricesAll().end()) {
			GetFrameBoneMatricesAll()[keyDrawFbx] = GetFrameBoneMatricesAll().at(fbxNameW);
			//合計キーフレームを計算する
			for (auto& animName : animNames) {
				totalKeyFrame += GetFrame(keyDrawFbx, animName).size();
			}

		}
		//アニメーション数

		//アニメーションがあるFBXの場合
		if (totalKeyFrame != 0) {
			size_t animSize = fbxAnimationData->GetAnimName().size();

			{
				//アニメーションの塊用ストラクチャーバッファ
				insBufferData.bufferSize = sizeof(XMMATRIX) * kMaxBones * totalKeyFrame;
				insBufferData.dataSize = sizeof(XMMATRIX);
				insBufferData.instanceNum = kMaxBones * totalKeyFrame;
				insBufferData.keyName = L"Animation";
				sizeIns.emplace_back(insBufferData);
				instanceBufferManager->MakeInstancingResource(insBufferData.keyName, insBufferData.dataSize, insBufferData.instanceNum);
			}
			//insRes.emplace_back(createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, sizeIns[1].bufferSize, D3D12_RESOURCE_STATE_GENERIC_READ));

			{
				//アニメーションのキーフレームなどの情報を保持するストラクチャーバッファ
				insBufferData.bufferSize = sizeof(AnimationInfo) * animSize;
				insBufferData.dataSize = sizeof(AnimationInfo);
				insBufferData.instanceNum = animSize;
				insBufferData.keyName = L"AnimationInfo";
				sizeIns.emplace_back(insBufferData);
				instanceBufferManager->MakeInstancingResource(insBufferData.keyName, insBufferData.dataSize, insBufferData.instanceNum);
			}



			sizeOfInstance = sizeIns;

		}
		//アニメーションを持たないFBXの場合
		else {
			{
				//アニメーションデータを一つ分持ち、全て単位行列とする
				insBufferData.bufferSize = sizeof(XMMATRIX) * kMaxBones * 1;
				insBufferData.dataSize = sizeof(XMMATRIX);
				insBufferData.instanceNum = kMaxBones * 1;
				insBufferData.keyName = L"Animation";

				sizeIns.emplace_back(insBufferData);
				instanceBufferManager->MakeInstancingResource(L"Animation", insBufferData.dataSize, insBufferData.instanceNum);
			}


			{
				//アニメーションのキーフレームなどの情報を保持するストラクチャーバッファ
				insBufferData.bufferSize = sizeof(AnimationInfo) * 1;
				insBufferData.dataSize = sizeof(AnimationInfo);
				insBufferData.instanceNum = 1;
				insBufferData.keyName = L"AnimationInfo";
				sizeIns.emplace_back(insBufferData);
				instanceBufferManager->MakeInstancingResource(insBufferData.keyName, insBufferData.dataSize, insBufferData.instanceNum);

			}

			//insRes.emplace_back(createResource->CreateComittedBufferResource(D3D12_HEAP_TYPE_UPLOAD, sizeIns[1].bufferSize, D3D12_RESOURCE_STATE_GENERIC_READ));
			sizeOfInstance = sizeIns;

		}
	}
	else {
		sizeOfInstance[0] = sizeIns[0];

	}

	int count = 1;

	bool flagLoop = true;
	int i = 0;
	//一つ目がFBXで二つ目が影

	FbxPos fbxPos;
	//fbxPos.fbxPosition.x = x;
	//fbxPos.fbxPosition.y = y;
	//fbxPos.fbxPosition.z = z;
	fbxPos.entityType = entityType;
	fbxPos.drawType = drawType;
	fbxPos.animationType = animationType;
	fbxPos.shadowFlag = drawShadow;
	fbxPos.vert = vertSize;
	//fbxPos.destPos = XMFLOAT3(fbxPos.fbxPosition.x, fbxPos.fbxPosition.y, fbxPos.fbxPosition.z);
	fbxPos.keyDrawFbx = keyDrawFbx;
	fbxPos.isDraw = true;

	Engine::Components::TransformLocal trans;
	trans.pos = { XMFLOAT3(x,y,z) };
	trans.rot = { XMFLOAT3(rX, rY, rZ) };
	trans.scale = { XMFLOAT3(sX, sY, sZ) };
	fbxPos.transformId = transformSystem_.CreateWithLocal(trans);


	Engine::Components::FbxInstanceHandle fbxInstanceHandle;
	fbxInstanceHandle.keyDrawFbx = keyDrawFbx;
	fbxInstanceHandle.modelKey = fbxNameW;
	fbxInstanceHandle.tid = fbxPos.transformId;

	//インスタンスレコード登録
	fbxInstance_.Add(fbxNameW, fbxPos);
	//インスタンスレコードのアニメーション情報登録///////
	auto nameToId = fbxAnimationData->GetAnimationNameToIndexToIndex();
	//アニメーションクリップIDとアニメーション名の対応関係をfbxInstanceに登録している
	fbxInstance_.SetAnimationNameToClipId(fbxNameW, nameToId);
	auto animStackNames = fbxAnimationData->GetAnimName();
	fbxInstance_.SetAnimationNameList(fbxNameW, animStackNames);
	//アニメーションクリップのフレーム数やFPSなどの情報をfbxInstanceに登録している
	vector<Engine::Animation::AnimClipMeta> animClipMeta;
	//アニメーションクリップの数だけ、アニメーションクリップの情報を作成している
	animClipMeta.resize(nameToId.size());
	for (auto& [name,id] : nameToId) {
		Engine::Animation::AnimClipMeta meta;
		meta.frameCount = fbxAnimationData->GetDuration(name);
		meta.fps = fbxAnimationData->GetAnimationFrameRate().at(name).animationFps;
		animClipMeta[id] = meta;
	}

	//アニメーションクリップメタ情報をfbxInstanceに登録している
	fbxInstance_.SetAnimationClipMetaData(fbxNameW, animClipMeta);

	/////////////////////////////////////////////

	const auto& instanceRec = fbxInstance_.GetInstanceRecords(fbxNameW);
	transformToInstanceIdx_[fbxPos.transformId] = instanceRec.size() - 1;


	fbxAnimatedActor->ReceiveAnimationStack(animationType);
	//パーティクル描画する時は必ずsetDrawPosを一回は実行する
	if (drawType == PARTICLE) {
		//Particleクラス生成
		//particle[getterFileName()].emplace_back(make_shared<Particle>(getter_m_meshesVecSubMesh(), getterDrawNum(), getter_cPosFbxPos().fbxPosition.x, getter_cPosFbxPos().fbxPosition.y, getter_cPosFbxPos().fbxPosition.z));
		//テクスチャの名前や縦横の大きさを設定している
		//for (auto pos : particle[getterFileName()]) {
			//pos->setterTexInfo(getter_texResource());
		//}

	}

	return fbxInstanceHandle;
}


int FbxLoader::ColliderIdx(const std::wstring& keyName)const
{
	return colliderComponent->KeyCollisionNameFromIdx(keyName);
}

void FbxLoader::RotateModel(const std::wstring& keyDrawFbx,float pitch, float yaw, float roll)
{
	int idx = IdxFromKey(keyDrawFbx, fbxKeyIndex);

	const auto* instanceRec = fbxInstance_.TryGetInstance(fbxNameW,idx);
	if (instanceRec) {
		auto id = instanceRec->transformId;

		auto transData = transformSystem_.GetLocal(id);
		Engine::Components::RotationDeg rote;
		rote = transData->rot;

		rote.xyz.x += pitch;
		rote.xyz.y += yaw;
		rote.xyz.z += roll;

		//指定IDの回転更新
		transformSystem_.SetRotationDeg(id, rote);


		XMMATRIX mat = transformSystem_.GetRotationMatrix(id);

		for (auto& colli : Collider()) {
			if (colli.keyDrawFbx == keyDrawFbx) {
				if (colli.collisionType == CollisionType::WHOLE_OBJECT) {

					switch (colli.shape) {
					case EColliderShape::OBB:
					{
						//ここでコリジョンの回転を渡す
						RotateObbByDeltaQuat(colli, mat);
						break;
					}
					case EColliderShape::CAPSULE:
					{
						break;
					}
					default:
					{
						break;
					}

					}
				}
			}
		}

	}

}

void FbxLoader::QuaternionRotateModel(const std::wstring& keyDrawFbx, float pitch, float yaw, float roll)
{
	int idx = IdxFromKey(keyDrawFbx, fbxKeyIndex);

	XMVECTOR quaternionRote = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);

	XMMATRIX mat = XMMatrixRotationQuaternion(quaternionRote);


	//cPos.fbxRotetion.rote = mat;


	//for (auto& colliKey : cPos.collsionNameKey) {
		//if (colliKey.nameKey == collisionKey) {
			//cPos.fbxRotetion.rote = mat;
		//}
	//}

}




void FbxLoader::UpDateInstanceResourceView(const std::wstring& fbxName)
{
	auto heapHandle = GetInstanceSrvGpuHandles()[fbxNameW];
	for (auto& heap : heapHandle) {
		for (int i = 0; i < sizeOfInstance.size(); i++) {
			instanceBufferManager->MakeInstancingResourceView(sizeOfInstance[i].keyName, sizeOfInstance[i].dataSize, heap, 0);
		}

	}

}

void FbxLoader::AddOBB(const std::wstring& keyDrawFbx, const std::wstring& fbxName, const std::wstring& keyName, const Engine::Components::TransformId& tid, const CollisionType& collisionType,DirectX::XMFLOAT3 m_Pos, const std::array<DirectX::XMFLOAT3, 3>& m_NormaDirect, const std::array<float, 3>& m_fLength, EColliderKind colliderKind )
{
	colliderComponent->AddOBB(keyDrawFbx,keyName,fbxName, tid,collisionType,m_Pos, m_NormaDirect, m_fLength, colliderKind);
}

//OBBのpos更新
void FbxLoader::UpDateObbPos(const std::wstring& keyName, DirectX::XMFLOAT3& m_Pos)
{
	colliderComponent->UpDateObbPos(keyName, m_Pos);

}

void FbxLoader::RotateObbByDeltaQuat(ColliderDesc& Collision, const DirectX::XMMATRIX& dq)
{
	colliderComponent->RotateObbByDeltaQuat(Collision, dq);
}


void FbxLoader::WrapFBXWithOBB(const std::wstring& keyDrawFbx, const std::wstring& keyCollision, const Engine::Components::TransformId& tid,const CollisionType& collisionType,DirectX::XMFLOAT3& m_Pos, const DirectX::XMFLOAT3& scale,EColliderKind colliderKind, CollisionRegion region)
{
	int idx=IdxFromKey(keyDrawFbx, fbxKeyIndex);
	const auto* instanceRec = fbxInstance_.TryGetInstance(fbxNameW, idx);
	if (instanceRec) {
		auto id = instanceRec->transformId;

		auto rotate = transformSystem_.GetRotationMatrix(id);

		XMVECTOR vx = rotate.r[0];  // ローカル X 軸
		XMVECTOR vy = rotate.r[1];  // ローカル Y 軸
		XMVECTOR vz = rotate.r[2];  // ローカル Z 軸

		DirectX::XMFLOAT3 axis0, axis1, axis2;
		DirectX::XMStoreFloat3(&axis0, vx);
		DirectX::XMStoreFloat3(&axis1, vy);
		DirectX::XMStoreFloat3(&axis2, vz);

		std::array<DirectX::XMFLOAT3, 3> m_NormaDirect;
		m_NormaDirect[0] = axis0;
		m_NormaDirect[1] = axis1;
		m_NormaDirect[2] = axis2;

		//各方向のベクトル
		std::array<float, 3> m_fLength;
		m_fLength[0] = vertSize[region].width * scale.x / 2;
		m_fLength[1] = vertSize[region].height * scale.y / 2;
		m_fLength[2] = vertSize[region].depth * scale.z / 2;

		colliderComponent->AddOBB(keyDrawFbx, fbxNameW, keyCollision, tid, collisionType, m_Pos, m_NormaDirect, m_fLength, colliderKind);

		fbxInstance_.SetCollisionName(fbxNameW, idx, keyCollision);

	}
	else {
		ThrrowError("\nFbxLoader::WrapFBXWithOBB : 指定されたモデルが見つかりませんでした");
	}

}

void FbxLoader::WrapFBXWithCapsule(const std::wstring& keyDrawFbx, const std::wstring& fbxName, const std::wstring& keyCollision, const Engine::Components::TransformId& tid, DirectX::XMFLOAT3& m_Pos, EColliderKind colliderKind, CollisionRegion region)
{
	int idx = IdxFromKey(keyDrawFbx, fbxKeyIndex);
	vertSize[region].width;
	vertSize[region].height;
	vertSize[region].depth;

	float radius = vertSize[region].depth/2;

	XMFLOAT3 pos1 = m_Pos;
	XMFLOAT3 pos2 = m_Pos;

	pos1.y = m_Pos.y + radius;
	pos2.y = m_Pos.y + vertSize[region].height- radius;

	colliderComponent->AddCapsule(keyCollision,fbxName, tid,pos1, pos2, radius, colliderKind);
	fbxInstance_.SetCollisionName(fbxNameW, idx, keyCollision);
}


void FbxLoader::UpDateCapsule(const std::wstring& colliderKeyName, const DirectX::XMFLOAT3& pos, float newRadius, CollisionRegion region)
{
	//頂点の最大幅からカプセルコリジョンの幅や半径決める
	vertSize[region].width;
	vertSize[region].height;
	vertSize[region].depth;

	float radius = vertSize[region].depth / 2;

	XMFLOAT3 pos1 = pos;
	XMFLOAT3 pos2 = pos;

	pos1.y = pos.y + radius;
	pos2.y = pos.y + vertSize[region].height - radius;

	colliderComponent->UpDateCapsule(colliderKeyName, pos1, pos2, newRadius);
}

std::vector<std::wstring>& FbxLoader::GetAnimName()
{
	return fbxAnimationData->GetAnimName();
}


void FbxLoader::AddCapsuleCollider(const std::wstring keyName, const Engine::Components::TransformId& tid,const DirectX::XMFLOAT3& p1,
	const DirectX::XMFLOAT3& p2,
	float radius, EColliderKind colliderKind)
{
	colliderComponent->AddCapsule(keyName,fbxNameW,tid,p1, p2, radius, colliderKind);
}

const ColliderDesc& FbxLoader::Collider(int idx) const
{
	return colliderComponent->Collider(idx);
}

ColliderDesc& FbxLoader::Collider(int idx)
{
	return colliderComponent->Collider(idx);
}

const ColliderDesc& FbxLoader::Collider(const std::wstring& keyName) const
{
	return colliderComponent->Collider(keyName);
}

ColliderDesc& FbxLoader::Collider(const std::wstring& keyName)
{
	return colliderComponent->Collider(keyName);
}

const std::vector<ColliderDesc>& FbxLoader::Collider() const
{
	return colliderComponent->Collider();
}


std::vector<ColliderDesc>& FbxLoader::Collider()
{
	return colliderComponent->Collider();
}

const SubMesh& FbxLoader::M_meshes(const std::wstring& fbxName, int idx) const
{
	return fbxModel.at(fbxName)->M_meshes(fbxName, idx);
}

SubMesh& FbxLoader::M_meshes(const std::wstring& fbxName, int idx)
{
	return fbxModel[fbxName]->M_meshes(fbxName, idx);
}

std::vector<SubMesh>& FbxLoader::M_meshes(const std::wstring& fbxName)
{
	return fbxModel[fbxName]->M_meshes(fbxName);
}

std::size_t FbxLoader::GetM_meshesCount(const std::wstring& fbxName) const noexcept {
	return fbxModel.at(fbxName)->GetM_meshesCount(fbxName);
}

//サブメッシュの数
std::size_t FbxLoader::GetSubMeshesCount(const std::wstring& fbxName, int idx) {
	return fbxModel[fbxName]->GetSubMeshesCount(fbxName, idx);
}

const SubMesh& FbxLoader::SubMeshes(const std::wstring& fbxName, int idx, int idx2) const
{
	return fbxModel.at(fbxName)->SubMeshes(fbxName, idx, idx2);
}


SubMesh& FbxLoader::SubMeshes(const std::wstring& fbxName, int idx, int idx2)
{
	return fbxModel.at(fbxName)->SubMeshes(fbxName, idx, idx2);
}


int FbxLoader::CountSubMesh(std::wstring fbxName)
{
	int count = 0;
	for (auto mesh : M_meshes(fbxName)) {
		count += mesh.subMeshes.size();
	}
	return count;
}

const std::vector<std::wstring>& FbxLoader::BoneNameSkinMesh(const std::wstring& fbxName) const
{
	return fbxModel.at(fbxName)->BoneNameSkinMesh();
}


std::vector<std::wstring>& FbxLoader::BoneNameSkinMesh(const std::wstring& fbxName)
{
	return fbxModel[fbxName]->BoneNameSkinMesh();
}

const std::vector<std::wstring>& FbxLoader::BoneNamesIdx(const std::wstring& fbxName) const
{
	return fbxModel.at(fbxName)->BoneNamesIdx();
}


std::vector<std::wstring>& FbxLoader::BoneNamesIdx(const std::wstring& fbxName)
{
	return fbxModel[fbxName]->BoneNamesIdx();
}

//fbxName : FBXの名前
//boneNam :ボーンの名前
const BoneInfo& FbxLoader::BoneNodeTable(const std::wstring& fbxName, std::wstring boneName) const
{
	return fbxModel.at(fbxName)->BoneNodeTable(fbxName,boneName);
}


//fbxName : FBXの名前
//boneNam :ボーンの名前
BoneInfo& FbxLoader::BoneNodeTable(const std::wstring& fbxName, std::wstring boneName)
{
	return fbxModel[fbxName]->BoneNodeTable(fbxName, boneName);
}

const std::wstring& FbxLoader::BoneNamesIdx(const std::wstring& fbxName, size_t idx) const
{
	return fbxModel.at(fbxName)->BoneNamesIdx(idx);
}


std::wstring& FbxLoader::BoneNamesIdx(const std::wstring& fbxName, size_t idx)
{
	return fbxModel[fbxName]->BoneNamesIdx(idx);
}

//ボーンテーブルに子要素追加
//fbxName : FBX名
//boneNameParent : ボーン親名
//boneNameChildren : ボーン子名
void FbxLoader::BoneNodeTableAddChild(const std::wstring& fbxName, const std::wstring& boneNameParent, const std::wstring& boneNameChildren)
{
	fbxModel[fbxName]->BoneNodeTableAddChild(fbxName, boneNameParent, boneNameChildren);
}

//ボーンテーブル子要素に親要素追加
//fbxName : FBX名
//boneNameParent : ボーン親名
//boneNameChildren : ボーン子名
void FbxLoader::BoneNodeTableAddParent(const std::wstring& fbxName, const std::wstring& boneNameParent, const std::wstring& boneNameChildren)
{
	//親ノードのポインタをセット
	fbxModel[fbxName]->BoneNodeTableAddParent(fbxName, boneNameParent, boneNameChildren);

}

//BoneNodeTableのボーン名まで
//fbxName : FBXの名前
//boneNam :ボーンの名前
const std::map <std::wstring, BoneInfo>& FbxLoader::BoneNodeTable(const std::wstring& fbxName) const
{
	return fbxModel.at(fbxName)->BoneNodeTable(fbxName);
}

//BoneNodeTableのボーン名まで
//fbxName : FBXの名前
//boneNam :ボーンの名前
std::map <std::wstring, BoneInfo>& FbxLoader::BoneNodeTable(const std::wstring& fbxName)
{
	return fbxModel[fbxName]->BoneNodeTable(fbxName);
}


//std::map<std::wstring, std::map <std::wstring, BoneInfo>> _boneNodeTable丸ごと
const std::map<std::wstring, std::map <std::wstring, BoneInfo>>& FbxLoader::BoneNodeTableAll(const std::wstring& fbxName) const
{
	return fbxModel.at(fbxName)->BoneNodeTable();
}

//std::map<std::wstring, std::map <std::wstring, BoneInfo>> _boneNodeTable丸ごと
std::map<std::wstring, std::map <std::wstring, BoneInfo>>& FbxLoader::BoneNodeTableAll(const std::wstring& fbxName)
{
	return fbxModel[fbxName]->BoneNodeTable();
}

//ボーンの数
//fbxName : FBXの名前
const int& FbxLoader::M_numBones(const std::wstring& fbxName)const
{
	return fbxModel.at(fbxName)->M_numBones(fbxName);
}

//ボーンの数
//fbxName : FBXの名前
int& FbxLoader::M_numBones(const std::wstring& fbxName)
{
	return fbxModel[fbxName]->M_numBones(fbxName);
}

//Fbxのクラスター
//fbxName : FBXの名前
const std::vector<FbxCluster*>& FbxLoader::M_clusters(const std::wstring& fbxName) const
{
	return fbxModel.at(fbxName)->M_clusters(fbxName);
}

//Fbxのクラスター
//fbxName : FBXの名前
std::vector<FbxCluster*>& FbxLoader::M_clusters(const std::wstring& fbxName)
{
	return fbxModel[fbxName]->M_clusters(fbxName);
}

//Fbxのクラスター
//fbxName : FBXの名前
//idx : インデックス
const FbxCluster* FbxLoader::M_clusters(const std::wstring& fbxName, size_t idx) const
{
	return fbxModel.at(fbxName)->M_clusters(fbxName,idx);
}

//Fbxのクラスター
//fbxName : FBXの名前
//idx : インデックス
FbxCluster*& FbxLoader::M_clusters(const std::wstring& fbxName, size_t idx)
{
	return fbxModel[fbxName]->M_clusters(fbxName, idx);
}

//ボーン生成
//fbxName : FBX名
const std::vector<Bone>& FbxLoader::M_bones(const std::wstring& fbxName)  const
{
	return fbxModel.at(fbxName)->M_bones(fbxName);
}


//ボーン生成
//fbxName : FBX名
std::vector<Bone>& FbxLoader::M_bones(const std::wstring& fbxName)
{
	return fbxModel[fbxName]->M_bones(fbxName);
}

//Boneゲッター
//fbxName : FBX名
//idx : インデックス
const Bone& FbxLoader::M_bones(const std::wstring& fbxName, size_t idx)  const
{
	return fbxModel.at(fbxName)->M_bones(fbxName,idx);
}


//Bone参照
//fbxName : FBX名
//idx : インデックス
Bone& FbxLoader::M_bones(const std::wstring& fbxName, size_t idx)
{
	return fbxModel[fbxName]->M_bones(fbxName,idx);
}

//FbxBone
//fbxname : FBX名
const std::vector<BoneInfo>& FbxLoader::FbxBones(const std::wstring& fbxName) const
{
	return fbxModel.at(fbxName)->FbxBones(fbxName);
}

//FbxBone
//fbxname : FBX名
std::vector<BoneInfo>& FbxLoader::FbxBones(const std::wstring& fbxName)
{
	return fbxModel[fbxName]->FbxBones(fbxName);
}

//FbxBone
//fbxname : FBX名
//idx : インデックス
const BoneInfo& FbxLoader::FbxBones(const std::wstring& fbxName, size_t idx) const
{
	return fbxModel.at(fbxName)->FbxBones(fbxName,idx);
}

//FbxBone
//fbxname : FBX名
//idx : インデックス
BoneInfo& FbxLoader::FbxBones(const std::wstring& fbxName, size_t idx)
{
	return fbxModel[fbxName]->FbxBones(fbxName, idx);
}

void FbxLoader::UpDateAnim(const std::wstring& fbxName, const std::wstring& keyDrawName, const std::wstring& animationStackName)
{
	fbxAnimatedActor->UpDateAnim(fbxName, keyDrawName, animationStackName);
}

std::map<std::wstring, std::vector<DirectX::XMMATRIX>>& FbxLoader::GetBoneMatrices()
{
	return fbxAnimationData->GetBoneMatrices();
}

const std::map<std::wstring, std::vector<DirectX::XMMATRIX>>& FbxLoader::GetBoneMatrices() const
{
	return fbxAnimationData->GetBoneMatrices();
}

const std::vector<DirectX::XMMATRIX>& FbxLoader::GetBoneMatrices(const std::wstring& keyDrawName) const
{
	return fbxAnimationData->GetBoneMatrices()[keyDrawName];
}

const std::vector<DirectX::XMMATRIX>& FbxLoader::GetFrameBoneMatrices(const std::wstring& keyDrawName) const
{
	wstring animStack=fbxAnimationData->GetAnimationInstance(keyDrawName).animationStackName;
	wstring drawFbxName = fbxAnimationData->GetAnimationInstance(keyDrawName).drawFbxName;
	UINT frame = fbxAnimationData->GetAnimationInstance(keyDrawName).frame;
	if (frame > fbxAnimationData->GetDuration(animStack)) {
		ThrrowError("PlayAnimation : フレーム番号が範囲外です");
	}
	return fbxAnimationData->GetFrameBoneMatrices()[drawFbxName][animStack][frame];
}

const std::unordered_map <std::wstring,std::unordered_map<std::wstring,std::vector<std::vector<DirectX::XMMATRIX>>>>& FbxLoader::GetFrameBoneMatricesAll() const
{
	return fbxAnimationData->GetFrameBoneMatrices();
}

std::unordered_map <std::wstring, std::unordered_map<std::wstring, std::vector<std::vector<DirectX::XMMATRIX>>>>& FbxLoader::GetFrameBoneMatricesAll()
{
	return fbxAnimationData->GetFrameBoneMatrices();
}



const std::vector <std::vector<DirectX::XMMATRIX>>& FbxLoader::GetFrame(const std::wstring& keyDrawName,const std::wstring& animStack) const
{
	UINT frame = fbxAnimationData->GetAnimationInstance(keyDrawName).frame;
	if (frame > fbxAnimationData->GetDuration(animStack)) {
		ThrrowError("PlayAnimation : フレーム番号が範囲外です");
	}
	return fbxAnimationData->GetFrameBoneMatrices()[fbxNameW][animStack];
}

void FbxLoader::CloneAnimationData(const std::shared_ptr<FbxLoader>& source)
{
	auto& sourcePtr = source->fbxAnimationData;
	*fbxAnimationData = *sourcePtr;
}

void FbxLoader::ReNameKey(const std::wstring& oldKey, const std::wstring& newKey)
{
	fbxAnimationData->ReNameKey(oldKey, newKey);
}

void FbxLoader::MakeInstanceUpdatePos(int upDateInsCount)
{
	//FBX描画専用のストラクチャーバッファ
	vector<InstanceBufferData> sizeIns;
	InstanceBufferData insBufferData;

	if (sizeOfInstance[1].instanceNum > GetCountDraw()) {
		return;
	}

	UINT sizeUpdateIns = sizeof(InstanceDrawUpdateData);
	insBufferData.bufferSize = sizeUpdateIns * upDateInsCount;
	insBufferData.dataSize = sizeUpdateIns;
	insBufferData.instanceNum = upDateInsCount;
	insBufferData.keyName = L"FBXInsUpdate";

	D3D12_CPU_DESCRIPTOR_HANDLE heap = m_descHeap->GetCPUDescriptorHandleForHeapStart();
	auto incSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	int idxUpDateIns = NUM_CBV + 1;

	instanceBufferManager->MakeInstancingResource(insBufferData.keyName, insBufferData.dataSize, insBufferData.instanceNum);
	//サブメッシュのインスタンスまで全てを更新する必要ある
	UpDateInstanceResourceView(fbxNameW);

}

void FbxLoader::MakeInstanceDrawIdxGPU(int drawCount)
{
	InstanceDrawIdxGPU instanceDrawIdxGPU;
	UINT sizeinstanceDrawIdxGPU = sizeof(InstanceDrawIdxGPU);
	InstanceBufferData insBufferData;

	if (sizeOfInstance[2].instanceNum > drawCount) {
		return;
	}

	insBufferData.bufferSize = sizeinstanceDrawIdxGPU * drawCount;
	insBufferData.dataSize = sizeinstanceDrawIdxGPU;
	insBufferData.instanceNum = drawCount;
	insBufferData.keyName = L"FBXInsIdxGPU";

	instanceBufferManager->MakeInstancingResource(insBufferData.keyName, insBufferData.dataSize, insBufferData.instanceNum);
	sizeOfInstance[2] = insBufferData;
	//サブメッシュのインスタンスまで全てを更新する必要ある
	UpDateInstanceResourceView(fbxNameW);

}

//アニメーション名からアニメーションインデックスを取得する
bool FbxLoader::TryGetAnimClipId(const std::wstring& animName, uint32_t& outClipId) const
{
	return fbxAnimationData->TryGetAnimClipId(animName, outClipId);
}