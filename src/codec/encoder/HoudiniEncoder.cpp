/*
 * Copyright 2014-2018 Esri R&D Zurich and VRBN
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "HoudiniEncoder.h"
#include "HoudiniCallbacks.h"

#include "prtx/Exception.h"
#include "prtx/Log.h"
#include "prtx/Geometry.h"
#include "prtx/Material.h"
#include "prtx/Shape.h"
#include "prtx/ShapeIterator.h"
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

constexpr bool           DBG                = false;

constexpr const wchar_t* ENC_NAME           = L"SideFX(tm) Houdini(tm) Encoder";
constexpr const wchar_t* ENC_DESCRIPTION    = L"Encodes geometry into the Houdini format.";

const prtx::EncodePreparator::PreparationFlags PREP_FLAGS = prtx::EncodePreparator::PreparationFlags()
	.instancing(false)
	.mergeByMaterial(false)
	.triangulate(false)
	.processHoles(prtx::HoleProcessor::TRIANGULATE_FACES_WITH_HOLES)
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
				std::wstring v = prtxAttr.getString(key); // explicit copy
				if (v.length() > 0) {

					// resolvemap search heuristic
					std::vector<std::wstring> rmKeys;
					rmKeys.push_back(v);
					rmKeys.push_back(L"assets/" + v);

					for (const auto& rmKey: rmKeys) {
						if (rm->hasKey(rmKey.c_str())) {
							const wchar_t* tmp = rm->getString(rmKey.c_str());
							if (tmp && (std::wcslen(tmp) > 0)) {
								prtx::URIPtr u(prtx::URI::create(tmp));
								v = u->getPath();
							}
							break;
						}
					}
				}
				aBuilder->setString(key.c_str(), v.c_str()); // also passing on empty strings
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
				if (DBG) log_wdebug(L"ignored atttribute '%s' with type %d") % key % prtxAttr.getType(key);
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
	if(oh == nullptr) throw prtx::StatusException(prt::STATUS_ILLEGAL_CALLBACK_OBJECT);
}

void HoudiniEncoder::encode(prtx::GenerateContext& context, size_t initialShapeIndex) {
	const prtx::InitialShape& initialShape = *context.getInitialShape(initialShapeIndex);
	auto* cb = dynamic_cast<HoudiniCallbacks*>(getCallbacks());

	const bool emitAttrs = getOptions()->getBool(EO_EMIT_ATTRIBUTES);

	prtx::DefaultNamePreparator        namePrep;
	prtx::NamePreparator::NamespacePtr nsMesh     = namePrep.newNamespace();
	prtx::NamePreparator::NamespacePtr nsMaterial = namePrep.newNamespace();
	prtx::EncodePreparatorPtr encPrep = prtx::EncodePreparator::create(true, namePrep, nsMesh, nsMaterial);

	// get attribute names from initial shape
	const std::vector<std::wstring> isAttrKeys = emitAttrs ? [&initialShape](){
		size_t keyCount = 0;
		wchar_t const* const* keys = initialShape.getAttributeMap()->getKeys(&keyCount);
		return std::vector<std::wstring>(keys, keys+keyCount);
	}() : std::vector<std::wstring>();

	// generate geometry
	prtx::ReportsAccumulatorPtr reportsAccumulator{prtx::WriteFirstReportsAccumulator::create()};
	prtx::ReportingStrategyPtr reportsCollector{prtx::LeafShapeReportingStrategy::create(context, initialShapeIndex, reportsAccumulator)};
	prtx::LeafIteratorPtr li = prtx::LeafIterator::create(context, initialShapeIndex);
	for (prtx::ShapePtr shape = li->getNext(); shape; shape = li->getNext()) {
		prtx::ReportsPtr r = reportsCollector->getReports(shape->getID());
		encPrep->add(context.getCache(), shape, initialShape.getAttributeMap(), r);

		// get final values of generic attributes
		if (emitAttrs) {
			for (const std::wstring& key: isAttrKeys) {
				switch (shape->getType(key)) {
					case prtx::Attributable::PT_STRING: {
						const auto v = shape->getString(key);
						cb->attrString(initialShapeIndex, shape->getID(), key.c_str(), v.c_str());
						break;
					}
					case prtx::Attributable::PT_FLOAT: {
						const auto v = shape->getFloat(key);
						cb->attrFloat(initialShapeIndex, shape->getID(), key.c_str(), v);
						break;
					}
					case prtx::Attributable::PT_BOOL: {
						const auto v = shape->getBool(key);
						cb->attrBool(initialShapeIndex, shape->getID(), key.c_str(), (v == prtx::PRTX_TRUE));
						break;
					}
					default:
						break;
				}
			}
		}
	}

	prtx::EncodePreparator::InstanceVector instances;
	encPrep->fetchFinalizedInstances(instances, PREP_FLAGS);
	convertGeometry(initialShape, instances, cb);
}

void serializeGeometry(SerializedGeometry& sg, const prtx::GeometryPtrVector& geometries) {
	// PASS 1: scan
	for (const auto& geo: geometries) {
		const prtx::MeshPtrVector& meshes = geo->getMeshes();
		for (const auto& mesh: meshes) {
			// TODO: use opportunity to count/reserve space in containers

			// scan for largest uv set count
			sg.uvSets = std::max(sg.uvSets, mesh->getUVSetsCount());
		}
	}

	// PASS 2: copy
	uint32_t vertexIndexBase = 0;
	uint32_t uvIndexBase = 0;
	for (const auto& geo: geometries) {
		const prtx::MeshPtrVector& meshes = geo->getMeshes();
		for (const auto& mesh: meshes) {
			const prtx::DoubleVector& verts = mesh->getVertexCoords();
			sg.coords.insert(sg.coords.end(), verts.begin(), verts.end());

			const prtx::DoubleVector& norms = mesh->getVertexNormalsCoords();
			sg.normals.insert(sg.normals.end(), norms.begin(), norms.end());

			const uint32_t numUVSets = mesh->getUVSetsCount();

			std::vector<uint32_t> uvSetIndexBases(sg.uvSets, 0u); // first value is always 0
			const uint32_t uvsBefore = (uint32_t)sg.uvCoords.size();
			for (uint32_t uvSet = 0; uvSet < numUVSets; uvSet++) {

				if (uvSet > 0)
					uvSetIndexBases[uvSet] = uvSetIndexBases[uvSet-1] + (uint32_t)mesh->getUVCoords(uvSet-1).size() / 2u;

				const prtx::DoubleVector& uvs = mesh->getUVCoords(uvSet);
				if (!uvs.empty())
					sg.uvCoords.insert(sg.uvCoords.end(), uvs.begin(), uvs.end());
			}
			const uint32_t uvsDelta = (uint32_t)sg.uvCoords.size() - uvsBefore;

			sg.counts.reserve(sg.counts.size() + mesh->getFaceCount());

			for (uint32_t fi = 0, faceCount = mesh->getFaceCount(); fi < faceCount; ++fi) {
				const uint32_t* vtxIdx = mesh->getFaceVertexIndices(fi);
				const uint32_t vtxCnt = mesh->getFaceVertexCount(fi);

				sg.counts.push_back(vtxCnt);
				sg.indices.reserve(sg.indices.size() + vtxCnt);
				for (uint32_t vi = 0; vi < vtxCnt; vi++)
					sg.indices.push_back(vertexIndexBase + vtxIdx[vtxCnt-vi-1]); // reverse winding

				for (uint32_t uvSet = 0; uvSet < sg.uvSets; uvSet++) {
					const uint32_t uvCount = (uvSet < numUVSets) ? mesh->getFaceUVCount(fi, uvSet) : 0u;
					if (uvCount == vtxCnt) {
						const uint32_t* uvIdx = mesh->getFaceUVIndices(fi, uvSet);
						for (uint32_t vi = 0; vi < uvCount; ++vi)
							sg.uvIndices.push_back(uvIndexBase + uvSetIndexBases[uvSet] + uvIdx[uvCount - vi - 1u]); // reverse winding
					}
					sg.uvCounts.push_back(uvCount);
				}
			}

			vertexIndexBase += (uint32_t)verts.size() / 3u;
			uvIndexBase += uvsDelta / 2u; // TODO: directly add to per uv set index bases
		}
	}
}

namespace {

using AttributeMapNOPtrVector = std::vector<const prt::AttributeMap*>;

struct AttributeMapNOPtrVectorOwner {
	AttributeMapNOPtrVector v;
	~AttributeMapNOPtrVectorOwner() {
		for (const auto& m: v) {
			if (m) m->destroy();
		}
	}
};

} // namespace

void HoudiniEncoder::convertGeometry(const prtx::InitialShape& initialShape,
                                     const prtx::EncodePreparator::InstanceVector& instances,
                                     HoudiniCallbacks* cb)
{
	const bool emitMaterials = getOptions()->getBool(EO_EMIT_MATERIALS);
	const bool emitReports = getOptions()->getBool(EO_EMIT_REPORTS);

	prtx::GeometryPtrVector geometries;
	std::vector<prtx::MaterialPtrVector> materials;
	std::vector<prtx::ReportsPtr> reports;
	std::vector<int32_t> shapeIDs;

	geometries.reserve(instances.size());
	materials.reserve(instances.size());
	reports.reserve(instances.size());
	shapeIDs.reserve(instances.size());

	for (const auto& inst: instances) {
		geometries.push_back(inst.getGeometry());
		materials.push_back(inst.getMaterials());
		reports.push_back(inst.getReports());
		shapeIDs.push_back(inst.getShapeId());
	}

	SerializedGeometry sg;
	serializeGeometry(sg, geometries);

	// log_debug("resolvemap: %s") % prtx::PRTUtils::objectToXML(initialShape.getResolveMap());
	if (DBG) log_debug("encoder #materials = %s") % materials.size();

	uint32_t faceCount = 0;
	std::vector<uint32_t> faceRanges;
	AttributeMapNOPtrVectorOwner matAttrMaps;
	AttributeMapNOPtrVectorOwner reportAttrMaps;

	assert(geometries.size() == reports.size());
	assert(materials.size() == reports.size());
	auto matIt = materials.cbegin();
	auto repIt = reports.cbegin();
	prtx::PRTUtils::AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::create());
	for (const auto& geo: geometries) {
		const prtx::MeshPtrVector& meshes = geo->getMeshes();

		for (size_t mi = 0; mi < meshes.size(); mi++) {
			const prtx::MeshPtr& m = meshes.at(mi);
			const prtx::MaterialPtr& mat = matIt->at(mi);

			faceRanges.push_back(faceCount);

			if (emitMaterials) {
				convertAttributabletoAttributeMap(amb, *(mat.get()), mat->getKeys(), initialShape.getResolveMap());
				matAttrMaps.v.push_back(amb->createAttributeMapAndReset());
			}

			if (emitReports) {
				const auto& r = *repIt;
				if (r) {
					for (const auto& b: r->mBools)
						amb->setBool(b.first->c_str(), b.second);
					for (const auto& f: r->mFloats)
						amb->setFloat(f.first->c_str(), f.second);
					for (const auto& s: r->mStrings)
						amb->setString(s.first->c_str(), s.second->c_str());
					reportAttrMaps.v.push_back(amb->createAttributeMapAndReset());
					if (DBG) log_debug("report attr map: %1%") % prtx::PRTUtils::objectToXML(reportAttrMaps.v.back());
				}
			}

			faceCount += m->getFaceCount();
		}

		++matIt;
		++repIt;
	}
	faceRanges.push_back(faceCount); // close last range

	assert(matAttrMaps.v.empty() || matAttrMaps.v.size() == faceRanges.size()-1);
	assert(reportAttrMaps.v.empty() || reportAttrMaps.v.size() == faceRanges.size()-1);
	assert(shapeIDs.size() == faceRanges.size()-1);

	cb->add(initialShape.getName(),
	        sg.coords.data(), sg.coords.size(),
			sg.normals.data(), sg.normals.size(),
			sg.uvCoords.data(), sg.uvCoords.size(),
			sg.counts.data(), sg.counts.size(),
			sg.indices.data(), sg.indices.size(),
			sg.uvCounts.data(), sg.uvCounts.size(),
			sg.uvIndices.data(), sg.uvIndices.size(),
	        sg.uvSets,
			faceRanges.data(), faceRanges.size(),
	        matAttrMaps.v.empty() ? nullptr : matAttrMaps.v.data(),
	        reportAttrMaps.v.empty() ? nullptr : reportAttrMaps.v.data(),
			shapeIDs.data());

	if (DBG) log_debug("HoudiniEncoder::convertGeometry: end");
}

void HoudiniEncoder::finish(prtx::GenerateContext& /*context*/) {
}


HoudiniEncoderFactory* HoudiniEncoderFactory::createInstance() {
	prtx::EncoderInfoBuilder encoderInfoBuilder;

	encoderInfoBuilder.setID(ENCODER_ID_HOUDINI);
	encoderInfoBuilder.setName(ENC_NAME);
	encoderInfoBuilder.setDescription(ENC_DESCRIPTION);
	encoderInfoBuilder.setType(prt::CT_GEOMETRY);

	prtx::PRTUtils::AttributeMapBuilderPtr amb(prt::AttributeMapBuilder::create());
	amb->setBool(EO_EMIT_ATTRIBUTES, prtx::PRTX_FALSE);
	amb->setBool(EO_EMIT_MATERIALS,  prtx::PRTX_FALSE);
	amb->setBool(EO_EMIT_REPORTS,    prtx::PRTX_FALSE);
	encoderInfoBuilder.setDefaultOptions(amb->createAttributeMap());

	return new HoudiniEncoderFactory(encoderInfoBuilder.create());
}
