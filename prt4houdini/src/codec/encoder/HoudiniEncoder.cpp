#include <iostream>
#include <sstream>
#include <vector>
#include <numeric>
#include <limits>

#include "prt/prt.h"

#include "prtx/Exception.h"
#include "prtx/Log.h"
#include "prtx/Geometry.h"
#include "prtx/Material.h"
#include "prtx/Shape.h"
#include "prtx/ShapeIterator.h"
#include "prtx/EncodePreparator.h"
#include "prtx/ExtensionManager.h"
#include "prtx/GenerateContext.h"

#include "encoder/HoudiniCallbacks.h"
#include "encoder/HoudiniEncoder.h"


namespace {
const bool DBG = false;
}


const std::wstring HoudiniEncoder::ID          	= L"HoudiniEncoder";
const std::wstring HoudiniEncoder::NAME        	= L"SideFX(tm) Houdini(tm) Encoder";
const std::wstring HoudiniEncoder::DESCRIPTION	= L"Encodes geometry into the Houdini format.";


HoudiniEncoder::HoudiniEncoder(const std::wstring& id, const prt::AttributeMap* options, prt::Callbacks* callbacks)
: prtx::GeometryEncoder(id, options, callbacks)
{
}


HoudiniEncoder::~HoudiniEncoder() {
}


void HoudiniEncoder::init(prtx::GenerateContext&) {
	prt::Callbacks* cb = getCallbacks();
	if (DBG) log_debug("HoudiniEncoder::init: cb = %x") % (size_t)cb;
	HoudiniCallbacks* oh = dynamic_cast<HoudiniCallbacks*>(cb);
	if (DBG) log_debug("                   oh = %x") % (size_t)oh;
	if(oh == 0) throw(prtx::StatusException(prt::STATUS_ILLEGAL_CALLBACK_OBJECT));
}


void HoudiniEncoder::encode(prtx::GenerateContext& context, size_t initialShapeIndex) {
	const prtx::InitialShape& initialShape = *context.getInitialShape(initialShapeIndex);

	HoudiniCallbacks* oh = dynamic_cast<HoudiniCallbacks*>(getCallbacks());

	prtx::DefaultNamePreparator        namePrep;
	prtx::NamePreparator::NamespacePtr nsMesh     = namePrep.newNamespace();
	prtx::NamePreparator::NamespacePtr nsMaterial = namePrep.newNamespace();

	prtx::EncodePreparatorPtr encPrep = prtx::EncodePreparator::create(true, namePrep, nsMesh, nsMaterial);

	prtx::LeafIteratorPtr li = prtx::LeafIterator::create(context, initialShapeIndex);
	for (prtx::ShapePtr shape = li->getNext(); shape; shape = li->getNext())
		encPrep->add(context.getCache(), shape, initialShape.getAttributeMap());

	prtx::GeometryPtrVector geometries;
	std::vector<prtx::DoubleVector> trafos;
	prtx::MeshPtrVector meshes;
	std::vector<prtx::MaterialPtrVector> materials;

	prtx::EncodePreparator::PreparationFlags prepFlags;
	prepFlags.instancing(false);
	prepFlags.mergeByMaterial(true);
	prepFlags.triangulate(false);
	prepFlags.mergeVertices(true);
	prepFlags.cleanupVertexNormals(true);
	prepFlags.cleanupUVs(true);
	prepFlags.processVertexNormals(prtx::VertexNormalProcessor::SET_MISSING_TO_FACE_NORMALS);
	prepFlags.indexSharing(prtx::EncodePreparator::PreparationFlags::INDICES_SAME_FOR_VERTICES_AND_NORMALS);

	prtx::EncodePreparator::InstanceVector finalizedInstances;
	encPrep->fetchFinalizedInstances(finalizedInstances, prepFlags);
	for (prtx::EncodePreparator::InstanceVector::const_iterator instIt = finalizedInstances.begin(); instIt != finalizedInstances.end(); ++instIt) {
		geometries.push_back(instIt->getGeometry());
		trafos.push_back(instIt->getTransformation());
		materials.push_back(instIt->getMaterials());
	}

	convertGeometry(initialShape.getName(), geometries, materials, oh);
}

void HoudiniEncoder::convertGeometry(
	const wchar_t* isName,
	const prtx::GeometryPtrVector& geometries,
	const std::vector<prtx::MaterialPtrVector>& mats,
	HoudiniCallbacks* hc
) {
	std::vector<double> 	vertices;
	std::vector<int32_t>    counts;
	std::vector<int32_t>    connects;
	std::vector<int32_t>	holes;
	int base    = 0;

	std::vector<double> normals;

	std::vector<float>  tcsU, tcsV;
	std::vector<int32_t>    uvCounts;
	std::vector<int32_t>    uvConnects;
	size_t uvBase  = 0;

	for(size_t gi = 0, geoCount = geometries.size(); gi < geoCount; ++gi) {
		prtx::Geometry* geo = geometries[gi].get();

		const prtx::MeshPtrVector& meshes = geo->getMeshes();
		for(size_t mi = 0, meshCount = meshes.size(); mi < meshCount; mi++) {
			prtx::MeshPtr mesh = meshes[mi];

			const prtx::DoubleVector&  verts    = mesh->getVertexCoords();
			const prtx::DoubleVector&  norms    = mesh->getVertexNormalsCoords();
			bool                       hasUVs   = (mesh->getUVSetsCount() > 0);
			size_t                     uvsCount = 0;

			vertices.reserve(    vertices.size()     + verts.size());
			normals.reserve(     normals.size()      + norms.size());
			counts.reserve(      counts.size()       + mesh->getFaceCount());
			uvCounts.reserve(    uvCounts.size()     + mesh->getFaceCount());

			vertices.insert(vertices.end(), verts.begin(), verts.end());
			normals.insert(normals.end(), norms.begin(), norms.end());

			if(hasUVs) {
				const prtx::DoubleVector& uvs = mesh->getUVCoords(0);
				uvsCount                      = uvs.size();

				tcsU.reserve(tcsU.size() + uvsCount / 2);
				tcsV.reserve(tcsV.size() + uvsCount / 2);

				for(size_t i = 0, size = uvsCount; i < size; i += 2) {
					tcsU.push_back((float)uvs[i+0]);
					tcsV.push_back((float)uvs[i+1]);
				}
			}
			if (DBG) log_debug("      copied vertex attributes");

			for (uint32_t fi = 0, faceCount = mesh->getFaceCount(); fi < faceCount; ++fi) {
				const uint32_t* vtxIdx = mesh->getFaceVertexIndices(fi);
				prtx::IndexVector vidxs(vtxIdx, vtxIdx + mesh->getFaceVertexCount(fi));

				counts.push_back(vidxs.size());
				for (size_t vi = 0, size = vidxs.size(); vi < size; ++vi)
					connects.push_back(base + vidxs[vi]);

				// collect hole-face indices for each face delimited by max int32_t
				static const int32_t HOLE_DELIM = std::numeric_limits<int32_t>::max();
				uint32_t faceHoleCount = mesh->getFaceHolesCount(fi);
				const uint32_t* faceHoleIndices = mesh->getFaceHolesIndices(fi);
				if (faceHoleCount > 0)
					holes.insert(holes.end(), faceHoleIndices, faceHoleIndices + faceHoleCount);
				holes.push_back(HOLE_DELIM);

				if(hasUVs && mesh->getFaceUVCount(fi, 0) > 0) {
						//if (DBG) log_debug("    faceuvcount(%d, 0) = %d") % fi % mesh->getFaceUVCount(fi, 0);
						//if (DBG) log_debug("    getFaceVertexCount(%d) = %d") % fi % mesh->getFaceVertexCount(fi);
						assert(mesh->getFaceUVCount(fi, 0) == mesh->getFaceVertexCount(fi));

						const uint32_t* uv0Idx = mesh->getFaceUVIndices(fi, 0);
						uvCounts.push_back((int)mesh->getFaceUVCount(fi, 0));
						for(size_t vi = 0, size = mesh->getFaceUVCount(fi, 0); vi < size; ++vi) {
							//if (DBG) log_debug("       vi = %d, uvBase = %d, uv0Idx[vi] = %d") % vi % uvBase % uv0Idx[vi];
							uvConnects.push_back(uvBase + uv0Idx[vi]);
						}
				}
				else
					uvCounts.push_back(0);
			}

			base   += (int)verts.size() / 3;
			uvBase += (int)uvsCount     / 2;

			if (DBG) log_debug("copied face attributes");
		}
	}

	bool hasUVS     = tcsU.size() > 0;
	bool hasNormals = normals.size() > 0;

	hc->setVertices(vertices.data(), vertices.size());
	if (DBG) log_debug("    set vertices");

	hc->setUVs(hasUVS ? tcsU.data() : 0, hasUVS ? tcsV.data() : 0, tcsU.size());
	if (DBG) log_debug("    set uvs");

	if (!normals.empty())
		hc->setNormals(normals.data(), normals.size());

	hc->setFaces(
			counts.data(), counts.size(),
			connects.data(), connects.size(),
			hasUVS ? uvCounts.data()   : 0, uvCounts.size(),
			hasUVS ? uvConnects.data() : 0, uvConnects.size(),
			holes.empty() ? 0 : holes.data(), holes.size()
	);
	if (DBG) log_debug("set faces");

	hc->createMesh(isName);
	if (DBG) log_debug("    maya output: created mesh");

	hc->finishMesh();

	int startFace = 0;
	for(size_t gi = 0, geoCount = geometries.size(); gi < geoCount; ++gi) {
		prtx::Geometry* geo = geometries[gi].get();
		prtx::MaterialPtr mat = mats[gi].front();

		int faceCount = 0;
		const prtx::MeshPtrVector& meshes = geo->getMeshes();
		for(size_t mi = 0, meshCount = meshes.size(); mi < meshCount; mi++)
			faceCount += (int)meshes[mi]->getFaceCount();

		hc->matSetColor(startFace, faceCount, mat->color_r(), mat->color_g(), mat->color_b());

//		std::wstring tex;
//		if(mat->diffuseMap().size() > 0 && mat->diffuseMap()[0]->isValid()) {
//			prtx::URIPtr texURI = mat->diffuseMap()[0]->getURI();
//			log_wtrace(L"trying to set texture uri: %s") % texURI->wstring();
//			std::wstring texPath = texURI->getPath();
//			hc->matSetDiffuseTexture(startFace, faceCount, texPath.c_str());
//		}

		startFace += faceCount;
	}

	if (DBG) log_debug("HoudiniEncoder::convertGeometry: end");
}


void HoudiniEncoder::finish(prtx::GenerateContext& /*context*/) {
}

