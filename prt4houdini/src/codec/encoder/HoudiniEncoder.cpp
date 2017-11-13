#include "encoder/HoudiniEncoder.h"
#include "encoder/HoudiniCallbacks.h"

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
#include "prtx/ReportsCollector.h"

#include "prt/prt.h"

#include <iostream>
#include <sstream>
#include <vector>
#include <numeric>
#include <limits>
#include <algorithm>


namespace {
	constexpr bool DBG = false;

	constexpr const wchar_t* ENC_ID          	= L"HoudiniEncoder";
	constexpr const wchar_t* ENC_NAME        	= L"SideFX(tm) Houdini(tm) Encoder";
	constexpr const wchar_t* ENC_DESCRIPTION	= L"Encodes geometry into the Houdini format.";

	constexpr const wchar_t* EO_EMIT_ATTRIBUTES       = L"emitAttributes";
	constexpr const wchar_t* EO_EMIT_MATERIALS        = L"emitMaterials";
	constexpr const wchar_t* EO_EMIT_REPORTS          = L"emitReports";
	constexpr const wchar_t* EO_EMIT_REPORT_SUMMARIES = L"emitReportSummaries";

const prtx::EncodePreparator::PreparationFlags PREP_FLAGS = prtx::EncodePreparator::PreparationFlags()
	.instancing(false)
	.mergeByMaterial(true)
	.triangulate(false)
	.mergeVertices(true)
	.cleanupVertexNormals(true)
	.cleanupUVs(true)
	.processVertexNormals(prtx::VertexNormalProcessor::SET_MISSING_TO_FACE_NORMALS)
	.indexSharing(prtx::EncodePreparator::PreparationFlags::INDICES_SAME_FOR_ALL_VERTEX_ATTRIBUTES);

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
	for(const auto& key : keys) {
		switch(prtxAttr.getType(key)) {
			case prt::Attributable::PT_BOOL:
				aBuilder->setBool(key.c_str(), prtxAttr.getBool(key) == prtx::PRTX_TRUE);
				break;

			case prt::Attributable::PT_FLOAT:
				aBuilder->setFloat(key.c_str(), prtxAttr.getFloat(key));
				break;

			case prt::Attributable::PT_INT:
				aBuilder->setInt(key.c_str(), prtxAttr.getInt(key));
				break;

			case prt::Attributable::PT_STRING: {
				const wchar_t* tv = prtxAttr.getString(key).c_str();
				if (tv && wcslen(tv) > 0) {
					std::wstring v(tv);

					// resolvemap search heuristic
					std::vector<std::wstring> rmKeys;
					rmKeys.push_back(v);
					rmKeys.push_back(L"assets/" + v);

					for (const auto& rmKey: rmKeys) {
						if (rm->hasKey(rmKey.c_str())) {
							const wchar_t* tmp = rm->getString(rmKey.c_str());
							if (tmp && std::wcslen(tmp) > 0) {
								prtx::URIPtr u(prtx::URI::create(tmp));
								v = u->getPath();
							}
							break;
						}
					}
					aBuilder->setString(key.c_str(), v.c_str());
				}
				break;
			}
			case prt::Attributable::PT_BOOL_ARRAY: {
				const std::vector<uint8_t>& ba = prtxAttr.getBoolArray(key);
				auto* boo = new bool[ba.size()];
				for (size_t i = 0; i < ba.size(); i++)
					boo[i] = (ba[i] == prtx::PRTX_TRUE);
				aBuilder->setBoolArray(key.c_str(), boo, ba.size());
				delete [] boo;
				break;
			}

			case prt::Attributable::PT_INT_ARRAY: {
				const std::vector<int32_t>& array = prtxAttr.getIntArray(key);
				aBuilder->setIntArray(key.c_str(), &array[0], array.size());
				break;
			}

			case prt::Attributable::PT_FLOAT_ARRAY: {
				const std::vector<double>& array = prtxAttr.getFloatArray(key);
				aBuilder->setFloatArray(key.c_str(), array.data(), array.size());
				break;
			}

			case prt::Attributable::PT_STRING_ARRAY:{
				const prtx::WStringVector& a = prtxAttr.getStringArray(key);
				std::vector<const wchar_t*> pw = toPtrVec(a);
				aBuilder->setStringArray(key.c_str(), pw.data(), pw.size());
				break;
			}

			// TODO: convert texture members...

			default:
				log_wdebug(L"ignored atttribute '%s' with type %d") % key % prtxAttr.getType(key);
				break;
		}
	}
}

} // namespace


HoudiniEncoder::HoudiniEncoder(const std::wstring& id, const prt::AttributeMap* options, prt::Callbacks* callbacks)
: prtx::GeometryEncoder(id, options, callbacks)
{  }

void HoudiniEncoder::init(prtx::GenerateContext&) {
	prt::Callbacks* cb = getCallbacks();
	if (DBG) log_debug("HoudiniEncoder::init: cb = %x") % (size_t)cb;
	auto* oh = dynamic_cast<HoudiniCallbacks*>(cb);
	if (DBG) log_debug("                   oh = %x") % (size_t)oh;
	if(oh == nullptr) throw(prtx::StatusException(prt::STATUS_ILLEGAL_CALLBACK_OBJECT));
}

void HoudiniEncoder::encode(prtx::GenerateContext& context, size_t initialShapeIndex) {
	const prtx::InitialShape& initialShape = *context.getInitialShape(initialShapeIndex);
	auto* oh = dynamic_cast<HoudiniCallbacks*>(getCallbacks());

	prtx::DefaultNamePreparator        namePrep;
	prtx::NamePreparator::NamespacePtr nsMesh     = namePrep.newNamespace();
	prtx::NamePreparator::NamespacePtr nsMaterial = namePrep.newNamespace();

	prtx::EncodePreparatorPtr encPrep = prtx::EncodePreparator::create(true, namePrep, nsMesh, nsMaterial);

	prtx::ReportsAccumulatorPtr reportsAccumulator{prtx::WriteFirstReportsAccumulator::create()};
	prtx::ReportingStrategyPtr reportsCollector{prtx::LeafShapeReportingStrategy::create(context, initialShapeIndex, reportsAccumulator)};
	prtx::LeafIteratorPtr li = prtx::LeafIterator::create(context, initialShapeIndex);
	for (prtx::ShapePtr shape = li->getNext(); shape; shape = li->getNext()) {
		prtx::ReportsPtr r = reportsCollector->getReports(shape->getID());
		encPrep->add(context.getCache(), shape, initialShape.getAttributeMap(), r);
	}

	prtx::GeometryPtrVector geometries;
	std::vector<prtx::MaterialPtrVector> materials;
	std::vector<prtx::ReportsPtr> reports;

	prtx::EncodePreparator::InstanceVector finalizedInstances;
	encPrep->fetchFinalizedInstances(finalizedInstances, PREP_FLAGS);
	for (const auto& inst: finalizedInstances) {
		geometries.push_back(inst.getGeometry());
		materials.push_back(inst.getMaterials());
		reports.push_back(inst.getReports());
	}

	convertGeometry(initialShape, geometries, materials, reports, oh);
}

void HoudiniEncoder::convertGeometry(
	const prtx::InitialShape& initialShape,
	const prtx::GeometryPtrVector& geometries,
	const std::vector<prtx::MaterialPtrVector>& mats,
	const std::vector<prtx::ReportsPtr>& reports,
	HoudiniCallbacks* hc
) {
	const bool emitMaterials = getOptions()->getBool(EO_EMIT_MATERIALS);

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

	//log_debug("resolvemap: %s") % prtx::PRTUtils::objectToXML(initialShape.getResolveMap());
	log_debug("encoder #materials = %s") % mats.size();

	uint32_t faceCount = 0;
	std::vector<uint32_t> faceRanges;
	std::vector<const prt::AttributeMap*> matAttrMaps;
	auto matIt = mats.cbegin();
	auto repIt = reports.cbegin();
	for (const auto& geo: geometries) {
		const prtx::MeshPtrVector& meshes = geo->getMeshes();
		const prtx::MaterialPtr& mat = matIt->front();

		// TODO: shape attributes (!)

		if (emitMaterials) {
			prtx::PRTUtils::AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::create());
			convertAttributabletoAttributeMap(amb, *(mat.get()), mat->getKeys(), initialShape.getResolveMap());
			matAttrMaps.push_back(amb->createAttributeMap());
		}

		// TODO: reports

		faceRanges.push_back(faceCount);
		std::for_each(meshes.begin(), meshes.end(), [&faceCount](const prtx::MeshPtr& m) {
			faceCount += m->getFaceCount();
		});

		++matIt;
	}
	faceRanges.push_back(faceCount); // close last range

	hc->add(initialShape.getName(),
			coords.data(), coords.size(),
			normals.data(), normals.size(),
			uvCoords.data(), uvCoords.size(),
			faceCounts.data(), faceCounts.size(),
			vertexIndices.data(), vertexIndices.size(),
			uvCounts.data(), uvCounts.size(),
			uvIndices.data(), uvIndices.size(),
			holes.data(), holes.size(),
			matAttrMaps.data(), matAttrMaps.size(),
			faceRanges.data()
	);

	for (std::vector<const prt::AttributeMap*>::iterator it = matAttrMaps.begin(); it != matAttrMaps.end(); ++it)
		(*it)->destroy();

	if (DBG) log_debug("HoudiniEncoder::convertGeometry: end");
}

void HoudiniEncoder::finish(prtx::GenerateContext& /*context*/) {
}


HoudiniEncoderFactory* HoudiniEncoderFactory::createInstance() {
	prtx::EncoderInfoBuilder encoderInfoBuilder;

	encoderInfoBuilder.setID(ENC_ID);
	encoderInfoBuilder.setName(ENC_NAME);
	encoderInfoBuilder.setDescription(ENC_DESCRIPTION);
	encoderInfoBuilder.setType(prt::CT_GEOMETRY);

	prtx::PRTUtils::AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::create());
	amb->setBool(EO_EMIT_ATTRIBUTES,       prtx::PRTX_FALSE);
	amb->setBool(EO_EMIT_MATERIALS,        prtx::PRTX_TRUE);
	amb->setBool(EO_EMIT_REPORTS,          prtx::PRTX_FALSE);
	amb->setBool(EO_EMIT_REPORT_SUMMARIES, prtx::PRTX_FALSE);
	encoderInfoBuilder.setDefaultOptions(amb->createAttributeMap());

	return new HoudiniEncoderFactory(encoderInfoBuilder.create());
}
