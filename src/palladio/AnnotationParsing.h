/*
 * Copyright 2014-2020 Esri R&D Zurich and VRBN
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

#pragma once

#include "Utils.h"

#include "prt/Annotation.h"

#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <variant>

namespace AnnotationParsing {

using ColorAnnotation = std::array<double, 3>;
ColorAnnotation parseColor(const std::wstring colorString);
std::wstring getColorString(const std::array<float, 3>& rgb);

struct RangeAnnotation {
	std::pair<double, double> minMax = {std::numeric_limits<double>::quiet_NaN(),
	                                    std::numeric_limits<double>::quiet_NaN()};
	bool restricted = false;

	bool rangeIsValid() const {
		return (std::isnan(minMax.first) || std::isnan(minMax.second)) && (minMax.first < minMax.second);
	}
};
RangeAnnotation parseRangeAnnotation(const prt::Annotation& annotation);

struct EnumAnnotation {
	bool mRestricted;
	std::wstring mValuesAttr;
	std::vector<std::wstring> mOptions;
};
EnumAnnotation parseEnumAnnotation(const prt::Annotation& annotation);

using FileAnnotation = std::vector<std::wstring>;
FileAnnotation parseFileAnnotation(const prt::Annotation& annotation);

enum class AttributeTrait { ENUM, RANGE, ANGLE, PERCENT, FILE, DIR, COLOR, DESCRIPTION, NONE };
AttributeTrait detectAttributeTrait(const prt::Annotation& annotation);

std::map<std::wstring, std::map<AttributeTrait, AnnotationVariant>>
getAttributeAnnotations(const RuleFileInfoUPtr& info);
} // namespace AnnotationParsing
