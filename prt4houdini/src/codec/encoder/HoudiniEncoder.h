#pragma once

#include "prtx/ResolveMap.h"
#include "prtx/Encoder.h"
#include "prtx/EncoderFactory.h"
#include "prtx/EncoderInfoBuilder.h"
#include "prtx/PRTUtils.h"
#include "prtx/Singleton.h"

#include "prt/ContentType.h"
#include "prt/InitialShape.h"

#include <string>
#include <iostream>
#include <stdexcept>


class HoudiniCallbacks;


class HoudiniEncoder : public prtx::GeometryEncoder {
public:
	HoudiniEncoder(const std::wstring& id, const prt::AttributeMap* options, prt::Callbacks* callbacks);
	~HoudiniEncoder() override = default;

public:
	void init(prtx::GenerateContext& context) override;
	void encode(prtx::GenerateContext& context, size_t initialShapeIndex) override;
	void finish(prtx::GenerateContext& context) override;

private:
	void convertGeometry(
		const prtx::InitialShape& initialShape,
		const prtx::GeometryPtrVector& geometries,
		const std::vector<prtx::MaterialPtrVector>& mat,
		const std::vector<prtx::ReportsPtr>& reports,
		HoudiniCallbacks* callbacks
	);
};


class HoudiniEncoderFactory : public prtx::EncoderFactory, public prtx::Singleton<HoudiniEncoderFactory> {
public:
	static HoudiniEncoderFactory* createInstance();

	explicit HoudiniEncoderFactory(const prt::EncoderInfo* info) : prtx::EncoderFactory(info) { }
	~HoudiniEncoderFactory() override = default;

	HoudiniEncoder* create(const prt::AttributeMap* options, prt::Callbacks* callbacks) const override {
		return new HoudiniEncoder(getID(), options, callbacks);
	}
};
