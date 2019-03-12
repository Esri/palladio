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
#include <set>
#include <memory>


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

std::pair<std::vector<const double*>, std::vector<size_t>> toPtrVec(const std::vector<prtx::DoubleVector>& v) {
	std::vector<const double*> pv(v.size());
	std::vector<size_t> ps(v.size());
	for (size_t i = 0; i < v.size(); i++) {
		pv[i] = v[i].data();
		ps[i] = v[i].size();
	}
	return std::make_pair(pv, ps);
}

std::wstring uriToPath(const prtx::TexturePtr& t){
	return t->getURI()->getPath();
}

// we blacklist all CGA-style material attribute keys, see prtx/Material.h
const std::set<std::wstring> MATERIAL_ATTRIBUTE_BLACKLIST = {
	L"ambient.b",
	L"ambient.g",
	L"ambient.r",
	L"bumpmap.rw",
	L"bumpmap.su",
	L"bumpmap.sv",
	L"bumpmap.tu",
	L"bumpmap.tv",
	L"color.a",
	L"color.b",
	L"color.g",
	L"color.r",
	L"color.rgb",
	L"colormap.rw",
	L"colormap.su",
	L"colormap.sv",
	L"colormap.tu",
	L"colormap.tv",
	L"dirtmap.rw",
	L"dirtmap.su",
	L"dirtmap.sv",
	L"dirtmap.tu",
	L"dirtmap.tv",
	L"normalmap.rw",
	L"normalmap.su",
	L"normalmap.sv",
	L"normalmap.tu",
	L"normalmap.tv",
	L"opacitymap.rw",
	L"opacitymap.su",
	L"opacitymap.sv",
	L"opacitymap.tu",
	L"opacitymap.tv",
	L"specular.b",
	L"specular.g",
	L"specular.r",
	L"specularmap.rw",
	L"specularmap.su",
	L"specularmap.sv",
	L"specularmap.tu",
	L"specularmap.tv",
	L"bumpmap",
	L"colormap",
	L"dirtmap",
	L"normalmap",
	L"opacitymap",
	L"specularmap"
};

void convertMaterialToAttributeMap(
		prtx::PRTUtils::AttributeMapBuilderPtr& aBuilder,
		const prtx::Material& prtxAttr,
		const prtx::WStringVector& keys,
		const prt::ResolveMap* rm
) {
//	log_wdebug(L"-- converting material: %1%") % prtxAttr.name();
	for(const auto& key : keys) {
		if (MATERIAL_ATTRIBUTE_BLACKLIST.count(key) > 0)
			continue;

//		log_wdebug(L"   key: %1%") % key;

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
				const std::wstring& v = prtxAttr.getString(key); // explicit copy
				aBuilder->setString(key.c_str(), v.c_str()); // also passing on empty strings
				break;
			}

			case prt::Attributable::PT_BOOL_ARRAY: {
				const std::vector<uint8_t>& ba = prtxAttr.getBoolArray(key);
				auto boo = std::unique_ptr<bool[]>(new bool[ba.size()]);
				for (size_t i = 0; i < ba.size(); i++)
					boo[i] = (ba[i] == prtx::PRTX_TRUE);
				aBuilder->setBoolArray(key.c_str(), boo.get(), ba.size());
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

			case prt::Attributable::PT_STRING_ARRAY: {
				const prtx::WStringVector& a = prtxAttr.getStringArray(key);
				std::vector<const wchar_t*> pw = toPtrVec(a);
				aBuilder->setStringArray(key.c_str(), pw.data(), pw.size());
				break;
			}

			case prtx::Material::PT_TEXTURE: {
				const auto& t = prtxAttr.getTexture(key);
				const std::wstring p = uriToPath(t);
				aBuilder->setString(key.c_str(), p.c_str());
				break;
			}

			case prtx::Material::PT_TEXTURE_ARRAY: {
				const auto& ta = prtxAttr.getTextureArray(key);

				prtx::WStringVector pa(ta.size());
				std::transform(ta.begin(), ta.end(), pa.begin(), uriToPath);

				std::vector<const wchar_t*> ppa = toPtrVec(pa);
				aBuilder->setStringArray(key.c_str(), ppa.data(), ppa.size());
				break;
			}

			default:
				if (DBG) log_wdebug(L"ignored atttribute '%s' with type %d") % key % prtxAttr.getType(key);
				break;
		}
	}
}

void convertReportsToAttributeMap(prtx::PRTUtils::AttributeMapBuilderPtr& amb, const prtx::ReportsPtr& r) {
	if (!r)
		return;

	for (const auto& b: r->mBools)
		amb->setBool(b.first->c_str(), b.second);
	for (const auto& f: r->mFloats)
		amb->setFloat(f.first->c_str(), f.second);
	for (const auto& s: r->mStrings)
		amb->setString(s.first->c_str(), s.second->c_str());
}

template<typename F>
void forEachKey(prt::Attributable const* a, F f) {
	if (a == nullptr)
		return;

	size_t keyCount = 0;
	wchar_t const* const* keys = a->getKeys(&keyCount);

	for (size_t k = 0; k < keyCount; k++) {
		wchar_t const* const key = keys[k];
		f(a, key);
	}
}

void forwardGenericAttributes(HoudiniCallbacks* hc, size_t initialShapeIndex, const prtx::InitialShape& initialShape, const prtx::ShapePtr& shape) {
	forEachKey(initialShape.getAttributeMap(), [&hc,&shape,&initialShapeIndex,&initialShape](prt::Attributable const* a, wchar_t const* key) {
		switch (shape->getType(key)) {
			case prtx::Attributable::PT_STRING: {
				const auto v = shape->getString(key);
				hc->attrString(initialShapeIndex, shape->getID(), key, v.c_str());
				break;
			}
			case prtx::Attributable::PT_FLOAT: {
				const auto v = shape->getFloat(key);
				hc->attrFloat(initialShapeIndex, shape->getID(), key, v);
				break;
			}
			case prtx::Attributable::PT_BOOL: {
				const auto v = shape->getBool(key);
				hc->attrBool(initialShapeIndex, shape->getID(), key, (v == prtx::PRTX_TRUE));
				break;
			}
			default:
				break;
		}
	});
}

using AttributeMapNOPtrVector = std::vector<const prt::AttributeMap*>;

struct AttributeMapNOPtrVectorOwner {
	AttributeMapNOPtrVector v;
	~AttributeMapNOPtrVectorOwner() {
		for (const auto& m: v) {
			if (m) m->destroy();
		}
	}
};

struct TextureUVMapping {
	std::wstring key;
	uint8_t      index;
	uint8_t      uvSet;
};

const std::vector<TextureUVMapping> TEXTURE_UV_MAPPINGS = {
		{ L"diffuseMap",  0, 0 }, // colormap
		{ L"bumpMap",     0, 1 }, // bumpmap
		{ L"diffuseMap",  1, 2 }, // dirtmap
		{ L"specularMap", 0, 3 }, // specularmap
		{ L"opacityMap",  0, 4 }, // opacitymap
		{ L"normalMap",   0, 5 }  // normalmap
		// TODO for PRT 2
		//6	emissivemap
		//7	occlusionmap
		//8	roughnessmap
		//9	metallicmap
};

// return the highest required uv set (where a valid texture is present)
uint8_t scanValidTextures(const prtx::MaterialPtr& mat) {
	uint8_t highestUVSet = 0;
	for (const auto& t: TEXTURE_UV_MAPPINGS) {
		const auto& ta = mat->getTextureArray(t.key);
		if (ta.size() > t.index && ta[t.index]->isValid())
			highestUVSet = std::max(highestUVSet, t.uvSet);
	}
	return highestUVSet;
}

} // namespace


namespace detail {

SerializedGeometry serializeGeometry(const prtx::GeometryPtrVector& geometries, const std::vector<prtx::MaterialPtrVector>& materials) {
	// PASS 1: scan
	uint32_t maxNumUVSets = 0;
	auto matsIt = materials.cbegin();
	for (const auto& geo: geometries) {
		const prtx::MeshPtrVector& meshes = geo->getMeshes();
		const prtx::MaterialPtrVector& mats = *matsIt;
		auto matIt = mats.cbegin();
		for (const auto& mesh: meshes) {
			const prtx::MaterialPtr& mat = *matIt;
			const uint32_t requiredUVSetsByMaterial = scanValidTextures(mat);
			maxNumUVSets = std::max(maxNumUVSets, std::max(mesh->getUVSetsCount(), requiredUVSetsByMaterial+1));
			++matIt;
		}
		++matsIt;
	}
	detail::SerializedGeometry sg(maxNumUVSets);

	// PASS 2: copy
	uint32_t vertexIndexBase = 0;
	for (const auto& geo: geometries) {
		const prtx::MeshPtrVector& meshes = geo->getMeshes();
		for (const auto& mesh: meshes) {
			const prtx::DoubleVector& verts = mesh->getVertexCoords();
			sg.coords.insert(sg.coords.end(), verts.begin(), verts.end());

			const prtx::DoubleVector& norms = mesh->getVertexNormalsCoords();
			sg.normals.insert(sg.normals.end(), norms.begin(), norms.end());

			const uint32_t numUVSets = mesh->getUVSetsCount();
			log_wdebug(L"mesh name: %1% (numUVSets %2%)") % mesh->getName() % numUVSets;
			if (numUVSets > 0) {
				const prtx::DoubleVector& uvs0 = mesh->getUVCoords(0);
				for (uint32_t uvSet = 0; uvSet < sg.uvs.size(); uvSet++) {
					const prtx::DoubleVector& uvs = (uvSet < numUVSets) ? mesh->getUVCoords(uvSet) : prtx::DoubleVector();
					log_wdebug(L"uvSet %1%: uvs.size() = %2%") % uvSet % uvs.size();
					auto& suvs = sg.uvs[uvSet];
					if (!uvs.empty())
						suvs.insert(suvs.end(), uvs.begin(), uvs.end());
					else if (!uvs0.empty()) // fallback to uv set 0
						suvs.insert(suvs.end(), uvs0.begin(), uvs0.end());
				}
			}

			sg.counts.reserve(sg.counts.size() + mesh->getFaceCount());
			for (uint32_t fi = 0, faceCount = mesh->getFaceCount(); fi < faceCount; ++fi) {
				const uint32_t* vtxIdx = mesh->getFaceVertexIndices(fi);
				const uint32_t vtxCnt = mesh->getFaceVertexCount(fi);
				sg.counts.push_back(vtxCnt);
				sg.indices.reserve(sg.indices.size() + vtxCnt);
				for (uint32_t vi = 0; vi < vtxCnt; vi++)
					sg.indices.push_back(vertexIndexBase + vtxIdx[vtxCnt - vi - 1]); // reverse winding
			}

			vertexIndexBase += (uint32_t)verts.size() / 3u;
		} // for all meshes
	} // for all geometries

	return sg;
}

} // namespace detail


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

	// generate geometry
	prtx::ReportsAccumulatorPtr reportsAccumulator{prtx::WriteFirstReportsAccumulator::create()};
	prtx::ReportingStrategyPtr reportsCollector{prtx::LeafShapeReportingStrategy::create(context, initialShapeIndex, reportsAccumulator)};
	prtx::LeafIteratorPtr li = prtx::LeafIterator::create(context, initialShapeIndex);
	for (prtx::ShapePtr shape = li->getNext(); shape; shape = li->getNext()) {
		prtx::ReportsPtr r = reportsCollector->getReports(shape->getID());
		encPrep->add(context.getCache(), shape, initialShape.getAttributeMap(), r);

		// get final values of generic attributes
		if (emitAttrs)
			forwardGenericAttributes(cb, initialShapeIndex, initialShape, shape);
	}

	prtx::EncodePreparator::InstanceVector instances;
	encPrep->fetchFinalizedInstances(instances, PREP_FLAGS);
	convertGeometry(initialShape, instances, cb);
}

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

	const detail::SerializedGeometry sg = detail::serializeGeometry(geometries, materials);

	if (DBG) {
		log_debug("resolvemap: %s") % prtx::PRTUtils::objectToXML(initialShape.getResolveMap());
		log_debug("encoder #materials = %s") % materials.size();
	}

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
				convertMaterialToAttributeMap(amb, *(mat.get()), mat->getKeys(), initialShape.getResolveMap());
				matAttrMaps.v.push_back(amb->createAttributeMapAndReset());
			}

			if (emitReports) {
				convertReportsToAttributeMap(amb, *repIt);
				reportAttrMaps.v.push_back(amb->createAttributeMapAndReset());
				if (DBG) log_debug("report attr map: %1%") % prtx::PRTUtils::objectToXML(reportAttrMaps.v.back());
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

	const std::pair<std::vector<const double*>, std::vector<size_t>> puvs = toPtrVec(sg.uvs);

	cb->add(initialShape.getName(),
	        sg.coords.data(), sg.coords.size(),
			sg.normals.data(), sg.normals.size(),
			sg.counts.data(), sg.counts.size(),
			sg.indices.data(), sg.indices.size(),
			puvs.first.data(), puvs.second.data(), sg.uvs.size(),
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
