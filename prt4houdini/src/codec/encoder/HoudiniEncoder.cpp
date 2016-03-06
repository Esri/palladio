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
#include "prtx/Attributable.h"
#include "prtx/URI.h"

#include "encoder/HoudiniCallbacks.h"
#include "encoder/HoudiniEncoder.h"


namespace {
const bool DBG = false;

std::vector<const wchar_t*> toPtrVec(const prtx::WStringVector& wsv) {
	std::vector<const wchar_t*> pw(wsv.size());
	for (size_t i = 0; i < wsv.size(); i++)
		pw[i] = wsv[i].c_str();
	return pw;
}

void convertAttributabletoAttributeMap(
		prtx::PRTUtils::AttributeMapBuilderPtr& aBuilder,
		const prtx::Attributable& prtxAttr,
		const prtx::WStringVector& keys,
		const prt::ResolveMap* rm
) {
	for(size_t k=0; k<keys.size(); k++) {
		switch(prtxAttr.getType(keys[k])) {
			case prt::Attributable::PT_BOOL:
				aBuilder->setBool(keys[k].c_str(), prtxAttr.getBool(keys[k]) == prtx::PRTX_TRUE);
				break;

			case prt::Attributable::PT_FLOAT:
				aBuilder->setFloat(keys[k].c_str(), prtxAttr.getFloat(keys[k]));
				break;

			case prt::Attributable::PT_INT:
				aBuilder->setInt(keys[k].c_str(), prtxAttr.getInt(keys[k]));
				break;

			case prt::Attributable::PT_STRING: {
				const wchar_t* tv = prtxAttr.getString(keys[k]).c_str();
				if (tv && wcslen(tv) > 0) {
					std::wstring v(tv);

					// resolvemap search heuristic
					std::vector<std::wstring> rmKeys;
					rmKeys.push_back(v);
					rmKeys.push_back(L"assets/" + v);

					for (size_t ki = 0; ki < rmKeys.size(); ki++) {
						const std::wstring& k = rmKeys[ki];
						if (rm->hasKey(k.c_str())) {
							const wchar_t* tmp = rm->getString(k.c_str());
							if (tmp && std::wcslen(tmp) > 0) {
								prtx::URIPtr u(prtx::URI::create(tmp));
								v = u->getPath();
							}
							break;
						}
					}
					aBuilder->setString(keys[k].c_str(), v.c_str());
				}
				break;
			}
			case prt::Attributable::PT_BOOL_ARRAY: {
				const std::vector<uint8_t>& ba = prtxAttr.getBoolArray(keys[k]);
				bool* boo = new bool[ba.size()];
				for (size_t i = 0; i < ba.size(); i++)
					boo[i] = (ba[i] == prtx::PRTX_TRUE);
				aBuilder->setBoolArray(keys[k].c_str(), boo, ba.size());
				delete [] boo;
				break;
			}

			case prt::Attributable::PT_INT_ARRAY: {
				const std::vector<int32_t>& array = prtxAttr.getIntArray(keys[k]);
				aBuilder->setIntArray(keys[k].c_str(), &array[0], array.size());
				break;
			}

			case prt::Attributable::PT_FLOAT_ARRAY: {
				const std::vector<double>& array = prtxAttr.getFloatArray(keys[k]);
				aBuilder->setFloatArray(keys[k].c_str(), array.data(), array.size());
				break;
			}

			case prt::Attributable::PT_STRING_ARRAY:{
				const prtx::WStringVector& a = prtxAttr.getStringArray(keys[k]);
				std::vector<const wchar_t*> pw = toPtrVec(a);
				aBuilder->setStringArray(keys[k].c_str(), pw.data(), pw.size());
				break;
			}

			// TODO: convert texture members...

			default:
				log_wdebug(L"ignored atttribute '%s' with type %d") % keys[k] % prtxAttr.getType(keys[k]);
				break;
		}
	}
}

} // namespace


const std::wstring HoudiniEncoder::ID          	= L"HoudiniEncoder";
const std::wstring HoudiniEncoder::NAME        	= L"SideFX(tm) Houdini(tm) Encoder";
const std::wstring HoudiniEncoder::DESCRIPTION	= L"Encodes geometry into the Houdini format.";


HoudiniEncoder::HoudiniEncoder(const std::wstring& id, const prt::AttributeMap* options, prt::Callbacks* callbacks)
: prtx::GeometryEncoder(id, options, callbacks)
{  }

HoudiniEncoder::~HoudiniEncoder() {  }

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
	//std::vector<prtx::DoubleVector> trafos;
	std::vector<prtx::MaterialPtrVector> materials;

	prtx::EncodePreparator::PreparationFlags prepFlags;
	prepFlags.instancing(false); // TODO
	prepFlags.mergeByMaterial(true);
	prepFlags.triangulate(false);
	prepFlags.mergeVertices(true);
	prepFlags.cleanupVertexNormals(true);
	prepFlags.cleanupUVs(true);
	prepFlags.processVertexNormals(prtx::VertexNormalProcessor::SET_MISSING_TO_FACE_NORMALS);
	prepFlags.indexSharing(prtx::EncodePreparator::PreparationFlags::INDICES_SAME_FOR_ALL_VERTEX_ATTRIBUTES);

	prtx::EncodePreparator::InstanceVector finalizedInstances;
	encPrep->fetchFinalizedInstances(finalizedInstances, prepFlags);
	for (prtx::EncodePreparator::InstanceVector::const_iterator instIt = finalizedInstances.begin(); instIt != finalizedInstances.end(); ++instIt) {
		geometries.push_back(instIt->getGeometry());
		//trafos.push_back(instIt->getTransformation());
		materials.push_back(instIt->getMaterials());
	}

	convertGeometry(initialShape, geometries, materials, oh);
}

void HoudiniEncoder::convertGeometry(
	const prtx::InitialShape& initialShape,
	const prtx::GeometryPtrVector& geometries,
	const std::vector<prtx::MaterialPtrVector>& mats,
	HoudiniCallbacks* hc
) {
	std::vector<double> coords, normals, uvCoords;
	std::vector<int32_t> faceCounts;
	std::vector<int32_t> vertexIndices;
	std::vector<int32_t> holes;
	std::vector<uint32_t> uvCounts; // unused
	std::vector<uint32_t> uvIndices;
	int32_t base = 0;
	int32_t uvBase  = 0;

	for(size_t gi = 0, geoCount = geometries.size(); gi < geoCount; ++gi) {
		prtx::Geometry* geo = geometries[gi].get();
		const prtx::MeshPtrVector& meshes = geo->getMeshes();
		for(size_t mi = 0, meshCount = meshes.size(); mi < meshCount; mi++) {
			prtx::MeshPtr mesh = meshes[mi];

			const prtx::DoubleVector&	verts		= mesh->getVertexCoords();
			const prtx::DoubleVector&	norms   	= mesh->getVertexNormalsCoords();
			bool                      	hasUVs		= (mesh->getUVSetsCount() > 0);
			const prtx::DoubleVector&	uvs			= hasUVs ? mesh->getUVCoords(0) : prtx::DoubleVector();

			faceCounts.reserve(faceCounts.size() + mesh->getFaceCount());
			uvCounts.reserve(uvCounts.size() + mesh->getFaceCount());

			coords.insert(coords.end(), verts.begin(), verts.end());
			normals.insert(normals.end(), norms.begin(), norms.end());
			if (!uvs.empty())
				uvCoords.insert(uvCoords.end(), uvs.begin(), uvs.end());

			for (uint32_t fi = 0, faceCount = mesh->getFaceCount(); fi < faceCount; ++fi) {
				const uint32_t* vtxIdx = mesh->getFaceVertexIndices(fi);
				const uint32_t vtxCnt = mesh->getFaceVertexCount(fi);

				faceCounts.push_back(vtxCnt);
				vertexIndices.reserve(vertexIndices.size() + vtxCnt);
				for (uint32_t vi = 0; vi < vtxCnt; vi++)
					vertexIndices.push_back(base + vtxIdx[vtxCnt-vi-1]); // reverse winding

#if 0
				// collect hole-face indices for each face delimited by max int32_t
				static const int32_t HOLE_DELIM = std::numeric_limits<int32_t>::max();
				uint32_t faceHoleCount = mesh->getFaceHolesCount(fi);
				const uint32_t* faceHoleIndices = mesh->getFaceHolesIndices(fi);
				if (faceHoleCount > 0)
					holes.insert(holes.end(), faceHoleIndices, faceHoleIndices + faceHoleCount);
				holes.push_back(HOLE_DELIM);
#endif

				if (hasUVs && mesh->getFaceUVCount(fi, 0) > 0) {
						//if (DBG) log_debug("    faceuvcount(%d, 0) = %d") % fi % mesh->getFaceUVCount(fi, 0);
						//if (DBG) log_debug("    getFaceVertexCount(%d) = %d") % fi % mesh->getFaceVertexCount(fi);
						assert(mesh->getFaceUVCount(fi, 0) == mesh->getFaceVertexCount(fi));

						const uint32_t* uv0Idx = mesh->getFaceUVIndices(fi, 0);
						uvCounts.push_back(mesh->getFaceUVCount(fi, 0));
						for (uint32_t vi = 0, size = uvCounts.back(); vi < size; ++vi) {
							//if (DBG) log_debug("       vi = %d, uvBase = %d, uv0Idx[vi] = %d") % vi % uvBase % uv0Idx[vi];
							uvIndices.push_back(uvBase + uv0Idx[vi]);
						}
				}
				else {
					uvCounts.push_back(vtxCnt);
					for (uint32_t vi = 0; vi < vtxCnt; ++vi) {
						uvIndices.push_back(uint32_t(-1)); // no uv
					}
				}
			}

			base   += (int32_t)verts.size() / 3u;
			uvBase += uvs.size() / 2u;

			if (DBG) log_debug("copied face attributes");
		}
	}

	hc->setVertices(coords.data(), coords.size());

	if (!uvCoords.empty())
		hc->setUVs(uvCoords.data(), uvCoords.size());

	if (!normals.empty())
		hc->setNormals(normals.data(), normals.size());

	hc->setFaces(
			faceCounts.data(), faceCounts.size(),
			vertexIndices.data(), vertexIndices.size(),
			uvCounts.data(), uvCounts.size(),
			uvIndices.data(), uvIndices.size(),
			holes.data(), holes.size()
	);

	hc->createMesh(initialShape.getName());
	hc->finishMesh();

	//log_debug("resolvemap: %s") % prtx::PRTUtils::objectToXML(initialShape.getResolveMap());
	log_debug("encoder #materials = %s") % mats.size();

	uint32_t faceCount = 0;
	std::vector<uint32_t> faceRanges;
	std::vector<const prt::AttributeMap*> matAttrMaps;
	for(size_t gi = 0, geoCount = geometries.size(); gi < geoCount; ++gi) {
		prtx::Geometry* geo = geometries[gi].get();
		const prtx::MeshPtrVector& meshes = geo->getMeshes();
		prtx::MaterialPtr mat = mats[gi].front();

		prtx::PRTUtils::AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::create());
		convertAttributabletoAttributeMap(amb, *(mat.get()), mat->getKeys(), initialShape.getResolveMap());

		faceRanges.push_back(faceCount);
		matAttrMaps.push_back(amb->createAttributeMap());

		for(size_t mi = 0, meshCount = meshes.size(); mi < meshCount; mi++)
			faceCount += meshes[mi]->getFaceCount();
	}
	faceRanges.push_back(faceCount); // close last range
	hc->setMaterials(faceRanges.data(), matAttrMaps.data(), matAttrMaps.size());

	for (std::vector<const prt::AttributeMap*>::iterator it = matAttrMaps.begin(); it != matAttrMaps.end(); ++it)
		(*it)->destroy();

	if (DBG) log_debug("HoudiniEncoder::convertGeometry: end");
}


void HoudiniEncoder::finish(prtx::GenerateContext& /*context*/) {
}

