#include"AstarPathFinder.h"
#include <queue>
#include<algorithm>
#include"ColliderComponent.h"

using namespace std;
using namespace DirectX;


namespace
{
    constexpr float kDiagonalCost = 1.41421356f;

    constexpr float kInf = std::numeric_limits<float>::infinity();

    //8近傍（対角を許可しないなら 4 要素にする）
    constexpr std::array<XMINT2, 8> kDir = {
        XMINT2{ 1, 0 }, XMINT2{ -1, 0 }, XMINT2{ 0, 1 }, XMINT2{ 0, -1 },
        XMINT2{ 1, 1 }, XMINT2{ -1, 1 }, XMINT2{ 1, -1}, XMINT2{ -1,-1 }
    };
}

//ピクセル座標とワールド座標対応づける
static inline XMINT2 WorldToGrid(const XMFLOAT3& pos, int width, int height, float scaleXz)
{
    //原点＝地形中央 と想定
    return {
        static_cast<int>(std::round(pos.x / scaleXz + 0.5f * (width - 1)/scaleXz)),
        static_cast<int>(std::round(pos.z / scaleXz + 0.5f * (height - 1)/scaleXz))
    };
}

//グリッドからワールド座標
static inline XMFLOAT3 GridToWorld(int gx, int gy, int width, int height, float scaleXZ, float y = 0.f)
{
    return {
        (gx - 0.5f * width) * scaleXZ,
        y,
        (gy - 0.5f * height) * scaleXZ
    };
}


std::vector<DirectX::XMFLOAT3> AstarPathFinder::AStarSearch(const std::vector<UINT>& astarObstacle, const GridInfo& gridInfo, DirectX::XMFLOAT3 startPos, DirectX::XMFLOAT3 goalPos)
{
    //経路復元
    std::vector<XMFLOAT3> path;


	const int width = gridInfo.width;
    const int height = gridInfo.height;
    const float scaleXz = gridInfo.scaleXZ;

    const int N = width * height;

    //1次元配列で2次元を表す
    auto ToIndex = [width](int x, int y) 
        {
            return y * width + x;
        };

    XMINT2 start = WorldToGrid(startPos, width, height, scaleXz);
    XMINT2 goal = WorldToGrid(goalPos, width, height, scaleXz);

    //返却する値はbool
    auto InBounds = [width, height](int x, int y)
        {
            return (0 <= x && x < width && 0 <= y && y < height);
        };

    //マンハッタン距離
    //x0 : 現在位置＿X
    //y0 : 現在位置＿Z
    //x1 : ゴール位置＿X
    //y1 : ゴール位置＿Z
    auto Heuristic = [](int x0, int y0, int x1, int y1)
        {
            return static_cast<float>(abs(x1 - x0) + abs(y1 - y0));
        };


    vector<float> gScore(N, kInf); //各要素の初期値=kInf
    vector<int> parent(N, -1);     //各要素の初期値=-1
    vector<uint8_t> closed(N, 0);  //探索していない=0

    const int startIdx = ToIndex(start.x, start.y);
    const int goalIdx = ToIndex(goal.x, goal.y);

    if (!IsValidIndex(astarObstacle,goalIdx)) {
        return path;
    }

    //この時点でゴールが障害物であれば、Aスターアルゴリズムで計算しない
    if (astarObstacle[goalIdx] == 0) {
        return path;
    }

    //始点を初期化
    gScore[startIdx] = 0.f;

    //f が小さいものほど高い優先度になる
    //Node : キューで格納する要素型
    //vector<Node> : 内部コンテナ
    //Compare : 要素の優先度を比較するもの
    std::priority_queue<Node, std::vector<Node>, Compare> open;

    //fの値,idxでNodeに渡してる
    open.push({ Heuristic(start.x,start.y,goal.x,goal.y),startIdx });

    //メインループ

    while (!open.empty()) {
        //ヒューリスティックコストを計算してオープンリスト先頭にきたNodeのidxを取る
        const int currentIdx = open.top().idx;
        //キューを取り除く
        open.pop();

        if (currentIdx == goalIdx) {
            break;                      //目的地到着
        }

        //0=未探索 / 1=探索済み
        if (closed[currentIdx]) {  
            continue;                   //すでに確定済み
        }

        closed[currentIdx] = 1;

        //currentIdxを分解してXとYそれぞれでidxを受け取る
        const int cx = currentIdx % width;
        const int cy = currentIdx / width;

        //近傍セルを調べる
        //ループで８方向を見る
        for (const auto& d : kDir) {
            const int nx = cx + d.x;
            const int ny = cy + d.y;

            //範囲外チェック
            if (!InBounds(nx, ny)) {
                continue;
            }

            const int nIdx = ToIndex(nx, ny);
            if (astarObstacle[nIdx] == 0) continue;

            if (d.x != 0 && d.y != 0) {
                int idxX = ToIndex(cx + d.x, cy);
                int idxY = ToIndex(cx, cy + d.y);
                if (astarObstacle[idxX] == 0 || astarObstacle[idxY] == 0)
                    continue;  // いずれかが障害物なら斜め移動禁止
            }

            if (closed[nIdx]) continue;

            const float stepCost = (d.x != 0 && d.y != 0) ? kDiagonalCost : 1.f;

            const float tentative = gScore[currentIdx] + stepCost;

            if (tentative < gScore[nIdx]) {
                parent[nIdx] = currentIdx;
                gScore[nIdx] = tentative;
                const float f = tentative + Heuristic(nx, ny, goal.x, goal.y);
                open.push({ f,nIdx });
            }
        }


    }

    int idx = goalIdx;
    if (parent[idx] == -1 && idx != startIdx) {
        return path; //ゴール到達不可
    }

    while (idx != -1) {
        const int x = idx % width;
        const int y = idx / width;
        //グリッド座標からワールド座標へ変換
        path.push_back(GridToWorld(x, y, width, height, scaleXz));
        if (idx == startIdx) {
            break;
        }
        idx = parent[idx];

    }

    reverse(path.begin(), path.end());

	return path;
}

int AstarPathFinder::FloorDiv(int pos, int chunkSize)
{
    int q = pos / chunkSize;  //１チャンクのサイズが16の時、X=-17であればマイナスチャンク２になるはずで、q=-1
    int r = pos % chunkSize;  //１チャンクのサイズが16の時、X=-17であればマイナスチャンク２になるはずで、r=-1
    return (r < 0) ? q - 1 : q; //rが０より小さい時、(r=-1)の時、q - 1 = -1-1 =-2つまり マイナスチャンク2を返すという感じ
}

UINT AstarPathFinder::SamplingObstacleMap(ChunkManager& chunkManager, int gX, int gZ, UINT fallback)
{
    //チャンク座標を求める
    int chunkSize = chunkManager.GetChunkSize();
    int mapId = 0;
    const int cX = FloorDiv(gX, chunkSize);
    const int cZ = FloorDiv(gZ, chunkSize);

    MapChunkKey key{ mapId,cX, cZ };

    if (!FindKey(key, chunkManager.GetChunkDatas())) {
        printf("\nSamplingObstacleMap : mapDataのキーが見つかりませんでした");
        return fallback;
    }
    int lX = (gX - cX * chunkSize);
    int lZ = (gZ - cZ * chunkSize);

    int idx = lZ * chunkSize + lX;

    if (chunkManager.GetChunkDatas()[key].data.obstacleMap.size() <= idx) {
        return fallback;
    }

    return chunkManager.GetChunkDatas()[key].data.obstacleMap[idx];
}

DirectX::XMINT2 AstarPathFinder::WorldToGlobalGrid(const DirectX::XMFLOAT3& pos, float scaleXZ)
{
    //x,zの位置をスケールで割ってセル番号に変換
    //floorを使うことで負の座標も「床方向」に丸める
    const int gx = static_cast<int>(std::floor(pos.x / scaleXZ));
    const int gz = static_cast<int>(std::floor(pos.z / scaleXZ));

    return { gx, gz };
}

std::vector<DirectX::XMFLOAT3> AstarPathFinder::AStarSearchPerChunk(ChunkManager& chunkManager, DirectX::XMFLOAT3 startPos, const PathGoal& goalInfo, const PathQuery& pathQuery, bool isGoalObstacle)
{
    //経路復元
    std::vector<XMFLOAT3> path;
    int chunkRange = chunkManager.GetChunkRange();
    int chunkSize = chunkManager.GetChunkSize();


    const int width = (2 * chunkRange+1) * chunkSize;
    const int height = (2 * chunkRange+1) * chunkSize;
    const float scaleXz = 1.0f;

    const int N = width * height;

    //1次元配列で2次元を表す
    auto ToIndex = [width](int x, int y)
        {
            return y * width + x;
        };

    const XMINT2 gStart = WorldToGlobalGrid(startPos, scaleXz);
    //gMinX,gMinZ グリッド全体でのグリッドの左上の位置を指す
    const int gMinX = gStart.x - width / 2;
    const int gMinZ = gStart.y - height / 2;

    const XMINT2 gGoal = WorldToGlobalGrid(goalInfo.goalPosition, scaleXz);


    //A*内部で使うローカル座標
    //障害物マップ全体でのグリッド座標を示す物を、チャンク内のスタートPos中心としたもので、グリッド座標を示すように値を変更する
    XMINT2 startL = { gStart.x - gMinX, gStart.y - gMinZ };
    XMINT2 goalL = { gGoal.x - gMinX, gGoal.y - gMinZ };

    startL.x = std::clamp(startL.x, 0, width - 1);
    startL.y = std::clamp(startL.y, 0, height - 1);
    goalL.x = std::clamp(goalL.x, 0, width - 1);
    goalL.y = std::clamp(goalL.y, 0, height - 1);

    //返却する値はbool
    auto InBounds = [width, height](int x, int y)
        {
            return (0 <= x && x < width && 0 <= y && y < height);
        };


    auto IsWalkable = [&](int lx, int ly) -> bool {
        if (!InBounds(lx, ly)) return false;
        const int gx = gMinX + lx;
        const int gz = gMinZ + ly;
        return SamplingObstacleMap(chunkManager, gx, gz, 0u) != 0u; // 未ロード=不可
        };



    //マンハッタン距離
    //x0 : 現在位置＿X
    //y0 : 現在位置＿Z
    //x1 : ゴール位置＿X
    //y1 : ゴール位置＿Z
    auto Heuristic = [](int x0, int y0, int x1, int y1)
        {
            return static_cast<float>(abs(x1 - x0) + abs(y1 - y0));
        };




    vector<float> gScore(N, kInf); //各要素の初期値=kInf
    vector<int> parent(N, -1);     //各要素の初期値=-1
    vector<uint8_t> closed(N, 0);  //探索していない=0

    const int startIdx = ToIndex(startL.x, startL.y);
    const int goalIdx = ToIndex(goalL.x, goalL.y);

    //もしゴール地点にオブジェクト置かれていたら
    if (!IsWalkable(goalL.x, goalL.y)) {
        //return path;    //この時点で経路探索打ち切り
    }

    //始点を初期化
    gScore[startIdx] = 0.f;

    //f が小さいものほど高い優先度になる
    //Node : キューで格納する要素型
    //vector<Node> : 内部コンテナ
    //Compare : 要素の優先度を比較するもの
    std::priority_queue<Node, std::vector<Node>, Compare> open;

    //fの値,idxでNodeに渡してる
    open.push({ Heuristic(startL.x,startL.y,goalL.x,goalL.y),startIdx });


    auto goalBand = BuildGoalBandLocalIndices(goalL.x, goalL.y, interactR, scaleXz, width, height, gMinX, gMinZ, goalInfo, IsWalkable);

    if (isGoalObstacle) {
        while (!open.empty())
        {
            const int currentIdx = open.top().idx;
            open.pop();

            if (closed[currentIdx]) continue;
            closed[currentIdx] = 1;

            //カレントインデックスが示す場所がすでにゴール候補である場合、ここで経路探索行う
            if (goalBand.find(currentIdx) != goalBand.end())
            {
                CreatePath(currentIdx, startIdx, width, gMinX, gMinZ, scaleXz, parent, path);
                return path;
            }

            //八近傍をチェックして、オープンノードに追加する
            CheckDirEight(currentIdx, width, goalL, closed, gScore, parent, open, InBounds, IsWalkable, ToIndex, Heuristic);


        }

    }
    else {
        //障害物をクリックしていない場合
        //メインループ
        while (!open.empty()) {
            //ヒューリスティックコストを計算してオープンリスト先頭にきたNodeのidxを取る
            const int currentIdx = open.top().idx;
            //キューを取り除く
            open.pop();

            if (currentIdx == goalIdx) {
                break;                      //目的地到着
            }

            //0=未探索 / 1=探索済み
            if (closed[currentIdx]) {
                continue;                   //すでに確定済み
            }

            closed[currentIdx] = 1;

            CheckDirEight(currentIdx, width, goalL, closed, gScore, parent, open, InBounds, IsWalkable, ToIndex, Heuristic);

        }

    }

    int idx = goalIdx;
    if (parent[idx] == -1 && idx != startIdx) {
        return path; //ゴール到達不可
    }


    CreatePath(idx, startIdx, width,gMinX, gMinZ, scaleXz, parent, path);
    return path;

}

float AstarPathFinder::DistancePointToOBB_XZ(
    const DirectX::XMFLOAT3& point,
    const ColliderDesc& collider)
{
    if (collider.shape != EColliderShape::OBB) {
        return FLT_MAX;
    }

    const auto& obb = collider.obb;

    DirectX::XMVECTOR p = DirectX::XMLoadFloat3(&point);
    DirectX::XMVECTOR c = DirectX::XMLoadFloat3(&obb.m_Pos);

    //OBB中心から点へのベクトル
    DirectX::XMVECTOR d = DirectX::XMVectorSubtract(p, c);

    //OBBのローカルX軸
    DirectX::XMVECTOR axisX = DirectX::XMLoadFloat3(&obb.m_NormaDirect[0]);

    //OBBのローカルZ軸
    DirectX::XMVECTOR axisZ = DirectX::XMLoadFloat3(&obb.m_NormaDirect[2]);

    axisX = DirectX::XMVector3Normalize(axisX);
    axisZ = DirectX::XMVector3Normalize(axisZ);

    //点がOBB中心から各軸方向にどれだけ離れているか
    float localX = DirectX::XMVectorGetX(DirectX::XMVector3Dot(d, axisX));
    float localZ = DirectX::XMVectorGetX(DirectX::XMVector3Dot(d, axisZ));


    //float halfX = obb.m_fLength[0];
    //float halfZ = obb.m_fLength[2];

    float halfX = obb.m_fLength[0] * 0.5f;
    float halfZ = obb.m_fLength[2] * 0.5f;

    //OBBの範囲を超えた分だけ距離にする
    float dx = (std::max)(std::abs(localX) - halfX, 0.0f);
    float dz = (std::max)(std::abs(localZ) - halfZ, 0.0f);

    return std::sqrt(dx * dx + dz * dz);
}
std::vector<DirectX::XMFLOAT3> AstarPathFinder::AstarRange(int lX, int lY, const int width, const int height, const int gMinX, const int gMinZ, const float scaleXZ, int range, const std::function<bool(int, int)>& IsWalkable)
{
    //ゴールから範囲内(range)に当たる座標をリストにまとめる
    std::vector<DirectX::XMFLOAT3> out;
    for (int dy = -range; dy <= range; ++dy) {
        for (int dx = -range; dx <= range; ++dx) {
            const int nx = lX + dx;
            const int ny = lY + dy;
            if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;
            if (!IsWalkable(nx, ny)) continue;

            //ローカル→グローバル→ワールド
            const int gx = gMinX + nx;
            const int gz = gMinZ + ny;

            DirectX::XMFLOAT3 wp;
            wp.x = static_cast<float>(gx) * scaleXZ;
            wp.y = 0.0f;
            wp.z = static_cast<float>(gz) * scaleXZ;
            out.push_back(wp);
        }
    }
    return out;
}


std::unordered_set<int> AstarPathFinder::BuildGoalBandLocalIndices(
    int goalLX, int goalLY, float interactR, float scaleXZ,int width, int height, int gMinX, int gMinZ, const PathGoal& goalInfo,const std::function<bool(int, int)>& IsWalkable
) {
    std::unordered_set<int> S;
    float searchRange = goalInfo.interactRange;
    //interactRこれで示す範囲をグリッドの大きさで直す
    if (goalInfo.collision && goalInfo.collision->shape == EColliderShape::OBB) {
        const auto& obb = goalInfo.collision->obb;
		//OBBの長い方の半分を半径とする
        float halfX = obb.m_fLength[0] * 0.5f;
        float halfZ = obb.m_fLength[2] * 0.5f;
		//OBBの中心からOBBの角までの距離を半径とする（OBBを内包する円の半径）
        float obbRadius = std::sqrt(halfX * halfX + halfZ * halfZ);
        searchRange = obbRadius + searchRange;
    }
    int rCells = static_cast<int>(std::ceil(searchRange / scaleXZ));

    auto ToIndex = [width](int x, int y) { return y * width + x; };

    for (int dy = -rCells; dy <= rCells; ++dy) {
        for (int dx = -rCells; dx <= rCells; ++dx) {
            const int nx = goalLX + dx;
            const int ny = goalLY + dy;
            //マップ外スキップ
            if (nx < 0 || ny < 0 || nx >= width || ny >= height) continue;

            float dist = 0;

            {
				//コリジョン情報を持つ構造体がNULLでなく、かつコリジョンがある場合、点とOBBの距離を計算する
                if (goalInfo.collision) {
                    //printf("interactR=%.2f, scaleXZ=%.2f, rCells=%d\n", interactR, scaleXZ, rCells);
                    //printf("goalInfo.interactRange=%.2f\n", searchRange);

                    const int gx = gMinX + nx;
                    const int gz = gMinZ + ny;

                    DirectX::XMFLOAT3 cellWorldPos;
                    cellWorldPos.x = static_cast<float>(gx) * scaleXZ;
                    cellWorldPos.y = 0.0f;
                    cellWorldPos.z = static_cast<float>(gz) * scaleXZ;

                    dist = DistancePointToOBB_XZ(cellWorldPos, *goalInfo.collision);
                    if (dist == 0.0f) continue;
                }
                else
                {
                    //OBBなしの場合は、普通に中心からの距離で判定
                    const float wx = static_cast<float>(dx) * scaleXZ;
                    const float wz = static_cast<float>(dy) * scaleXZ;
                    dist = std::sqrt(wx * wx + wz * wz);
                }

            }

            if (dist <= searchRange)
            {
                //障害物ではないかどうか
                if (!IsWalkable(nx, ny)) continue;
                S.insert(ToIndex(nx, ny));
            }

        }
    }
    return S;
}

void AstarPathFinder::CheckDirEight(const int currentIdx,const int width, const XMINT2& goalL,const std::vector<uint8_t>& closed,
    std::vector<float>& gScore, std::vector<int>& parent, std::priority_queue<AstarPathFinder::Node, std::vector<AstarPathFinder::Node>, AstarPathFinder::Compare>& open
    , const std::function<bool(int, int)>& InBounds, const std::function<bool(int, int)>& IsWalkable, const std::function<int(int, int)>& ToIndex, const std::function<int(int x0, int y0, int x1, int y1)>& Heuristic)
{

    //currentIdxを分解してXとYそれぞれでidxを受け取る
    const int cx = currentIdx % width;
    const int cy = currentIdx / width;

    //近傍セルを調べる
    //ループで８方向を見る
    for (const auto& d : kDir) {
        const int nx = cx + d.x;
        const int ny = cy + d.y;

        //範囲外チェック
        if (!InBounds(nx, ny)) {
            continue;
        }

        const int nIdx = ToIndex(nx, ny);
        if (!IsWalkable(nx, ny)) continue;

        if (d.x != 0 && d.y != 0) {
            if (d.x != 0 && d.y != 0) {
                if (!IsWalkable(cx + d.x, cy) || !IsWalkable(cx, cy + d.y)) continue;
            }
        }

        if (closed[nIdx]) continue;

        const float stepCost = (d.x != 0 && d.y != 0) ? kDiagonalCost : 1.f;

        const float tentative = gScore[currentIdx] + stepCost;

        if (tentative < gScore[nIdx]) {
            parent[nIdx] = currentIdx;
            gScore[nIdx] = tentative;
            const float f = tentative + Heuristic(nx, ny, goalL.x, goalL.y);
            open.push({ f,nIdx });
        }
    }
}

void AstarPathFinder::CreatePath(int idx,const int startIdx,const int width, const int gMinX, const int gMinZ, float scaleXZ, const std::vector<int>& parent, std::vector<DirectX::XMFLOAT3>& path)
{
    while (idx != -1) {
        const int lx = idx % width; //インデックスを一次元から二次元に分解  ly * width + lx;
        const int ly = idx / width;

        // ローカル -> グローバル格子
        //Aの探索窓（ローカル窓）の左上をグローバルで示すgMinX,Zにローカルでのインデックスをくわえる事でグローバルに直してる
        const int gx = gMinX + lx;
        const int gz = gMinZ + ly;

        //グローバル格子 ->ワールド座標
        DirectX::XMFLOAT3 wp;
        wp.x = static_cast<float>(gx) * scaleXZ;
        wp.y = 0.0f;
        wp.z = static_cast<float>(gz) * scaleXZ;

        path.push_back(wp);
        if (idx == startIdx) break;
        idx = parent[idx];
    }

    reverse(path.begin(), path.end());

}