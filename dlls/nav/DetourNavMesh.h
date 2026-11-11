// DetourNavMesh.h
//
// Based on Detour/Recast (public domain)

#ifndef DETOURNAVMESH_H
#define DETOURNAVMESH_H

#include "DetourCommon.h"

// Common constants
static const unsigned int DT_NULL_LINK = 0xffffffff;

enum dtPolyType
{
	DT_POLYTYPE_GROUND = 0,
	DT_POLYTYPE_OFFMESH = 1,
};

// The maximum number of connected neighbours per polygon
static const int DT_POLYMAX = 6;

// The number of vertices per navigation polygon
static const int DT_VERTS_PER_POLYGON = 6;

// Type used to represent a poly reference
typedef unsigned int dtPolyRef;

struct dtNavMeshParams
{
	float origin[3];		// World origin of the mesh
	float tileSize;		// Width and depth of each tile in world units
	float tileHeight;		// Height of each tile in world units
	int maxTiles;			// The maximum number of tiles the navmesh can contain
	int maxPolys;			// The maximum number of polygons the navmesh can contain
	int tileBits;			// The number of bits required to represent the tile index
	int polyBits;			// The number of bits required to represent the poly index
	int saltBits;			// The number of bits required to represent the tile salt
};

struct dtMeshHeader
{
	unsigned int salt;		// 0
	unsigned int version;		// 4
	int tileX;			// 8
	int tileY;			// 12
	float reserved[2];		// 16..23, not decoded by the live paths
	int polyCount;			// 24
	float verts[1];			// 28, vertex data follows (not used by the live paths)
};

struct dtMeshTile;

// (forward declarations for the tile member pointers below)
struct dtPoly;
struct dtLink;

struct dtMeshTile
{
	unsigned int salt;		// 0
	unsigned int firstLink;		// 4, head of the per-tile link free list
	dtMeshHeader* header;		// 8
	dtPoly* polys;			// 12
	unsigned char reserved1[4];	// 16..19, not decoded by the live paths
	dtLink* links;			// 20
	unsigned char reserved2[32];	// 24..55, not decoded by the live paths
	dtMeshTile* next;		// 56, next tile in the tile bucket (pointer)
};

struct dtPoly
{
	unsigned int firstLink;			// 0
	unsigned char reserved1[12];		// 4..15, not decoded by the live paths
	unsigned short refs[DT_POLYMAX];	// 16, neighbour indices
	unsigned char reserved2[2];		// 28..29, not decoded by the live paths
	unsigned char neighbCnt;			// 30
	unsigned char polyType;			// 31
};

struct dtLink
{
	dtPolyRef ref;
	unsigned int next;
	unsigned char edge;
	unsigned char internal;
	unsigned char pad[2];
};

struct dtOffMeshConnection
{
	unsigned int poly;
	unsigned int flags;
	unsigned int userId;
	unsigned int next;
	unsigned int rad;
	unsigned char verts[6];
};

struct dtTileState
{
	unsigned int salt;
	int tileX;
	int tileY;
};

struct dtNode
{
	unsigned int next;
	unsigned int parent;
	unsigned short hash;
	unsigned char flag;
	unsigned char pad;
};

class dtNodePool
{
public:
	dtNodePool(int maxNodes, int hashSize);
	~dtNodePool();

	void clear();
	dtNode* getNode(unsigned int i);
	dtNode* findNode(unsigned int i);
	int findNodes(unsigned int i, dtNode** nodes, int maxNodes);

private:
	unsigned short* m_next;
	dtNode* m_nodes;
	int m_nodeCount;
	int m_nodeCapacity;
	int m_hashSize;
	unsigned short* m_hash;
};

class dtNavMesh
{
public:
	dtNavMesh();
	~dtNavMesh();

	dtStatus init(const dtNavMeshParams* params);
	dtStatus init(const unsigned char* data, int dataSize, int flags);
	const dtNavMeshParams* getParams() const;
	const dtMeshTile* getTileAt(int x, int y) const;
	int getTilesAt(int x, int y, const dtMeshTile** tiles, int maxTiles) const;
	int getTilesAt(int x, int y, dtMeshTile** tiles, int maxTiles);
	int getNeighbourTilesAt(int x, int y, int layer, const dtMeshTile** tiles, int maxTiles) const;
	const dtMeshTile* getTileByRef(dtPolyRef ref) const;
	int getMaxTiles() const;
	const dtMeshTile* getTile(int n) const;
	dtMeshTile* getTile(int n);
	void calcTileLoc(const float* pos, int* tx, int* ty) const;
	dtStatus getTileAndPolyByRef(dtPolyRef ref, const dtMeshTile** tile, const dtPoly** poly) const;
	dtStatus getTileAndPolyByRefUnsafe(dtPolyRef ref, const dtMeshTile** tile, const dtPoly** poly) const;
	dtStatus closestPointOnPoly(dtPolyRef ref, const float* pos, float* closest, bool* posOverPoly) const;
	bool isValidPolyRef(dtPolyRef ref) const;
	dtPolyRef getPolyRefBase(const dtMeshTile* tile) const;
	dtPolyRef getTileRef(const dtMeshTile* tile) const;
	dtStatus unconnectExtLinks(dtMeshTile* tile, dtMeshTile* prev);
	dtStatus removeTile(dtPolyRef ref, unsigned char** data, int* dataSize);
	dtStatus getTileRefAt(int x, int y, int layer) const;
	void connectIntLinks(dtMeshTile* tile);
	void findConnectingPolys(const float* beg, const float* end, const dtMeshTile* tile, int max, dtPolyRef* refs, float* tst, int* nrefs) const;
	void connectExtLinks(dtMeshTile* tile, dtMeshTile* prev);
	void queryPolygonsInTile(const dtMeshTile* tile, const float* centre, const float* extents, dtPolyRef* refs, int* nrefs, const int maxRefs) const;
	void findNearestPolyInTile(const dtMeshTile* tile, const float* pos, dtPolyRef* ref, float* tst) const;
	void connectExtOffMeshLinks(dtMeshTile* tile, dtMeshTile* prev);
	void baseOffMeshLinks(dtMeshTile* tile);
	dtStatus addTile(const unsigned char* data, int dataSize, int flags, dtPolyRef* result);
	int getTileStateSize(const dtMeshTile* tile) const;
	dtStatus storeTileState(const dtMeshTile* tile, unsigned char* data, int* dataSize) const;
	dtStatus restoreTileState(dtMeshTile* tile, const unsigned char* data, int dataSize);
	dtStatus getOffMeshConnectionPolyEndPoints(dtPolyRef prevRef, dtPolyRef nextRef, float* outStartPos, float* outEndPos) const;
	const dtOffMeshConnection* getOffMeshConnectionByRef(dtPolyRef ref) const;
	void setPolyFlags(dtPolyRef ref, unsigned short flags);
	void getPolyFlags(dtPolyRef ref, unsigned short* result) const;
	void setPolyArea(dtPolyRef ref, unsigned char area);
	void getPolyArea(dtPolyRef ref, unsigned char* result) const;

private:
	dtNavMeshParams m_params;
	dtMeshTile* m_tiles;
	int m_gridHashSize;			// Bucket count minus one
	dtMeshTile** m_gridBuckets;
	unsigned int m_nextFreeTile;
	unsigned int m_tileFreeList;
	unsigned int m_tileCopy;
	unsigned int m_offMeshBase;
	unsigned int m_offMeshCon;
	int m_tileCount;
	int m_offMeshConCount;
	int m_offMeshBaseCount;
};

struct dtNavMeshCreateParams
{
	int vertCount;
	float* verts;

	int polyCount;
	unsigned short* polys;
	unsigned char* polyAreas;
	unsigned short* polyFlags;
	unsigned int* polyUserDatas;

	int linkCount;
	unsigned int* links;
	unsigned char* linkAreas;

	int offMeshConCount;
	float* offMeshConVerts;
	unsigned char* offMeshConAreas;
	int* offMeshConFlags;
	unsigned int* offMeshConUserDatas;
	unsigned char* offMeshConDirs;
	float* offMeshConRad;

	int tileX;
	int tileY;
	float tileOrigin[3];
	float tileWidth;
	float tileHeight;

	int maxPolysPerTile;
	int maxLinksPerTile;
};

dtStatus dtCreateNavMeshData(const dtNavMeshCreateParams* params, unsigned char** outData, int* outDataSize);

void dtNavMeshHeaderSwapEndian(unsigned char* data);
void dtNavMeshDataSwapEndian(unsigned char* data);

#endif
