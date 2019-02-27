//
// Expansion Hunter
// Copyright 2016-2019 Illumina, Inc.
// All rights reserved.
//
// Author: Egor Dolzhenko <edolzhenko@illumina.com>
// Concept: Michael Eberle <meberle@illumina.com>
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//

#include "input/CatalogLoading.hh"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <boost/optional.hpp>

#include "thirdparty/json/json.hpp"
#include "thirdparty/spdlog/spdlog.h"

#include "common/Common.hh"
#include "common/Reference.hh"
#include "input/LocusSpecDecoding.hh"

using boost::optional;
using graphtools::NodeId;
using std::map;
using std::ostream;
using std::string;
using std::to_string;
using std::vector;

using Json = nlohmann::json;

namespace spd = spdlog;

namespace ehunter
{

static bool checkIfFieldExists(const Json& record, const string& fieldName)
{
    return record.find(fieldName) != record.end();
}

static void assertFieldExists(const Json& record, const string& fieldName)
{
    if (!checkIfFieldExists(record, fieldName))
    {
        std::stringstream out;
        out << record;
        throw std::logic_error("Field " + fieldName + " must be present in " + out.str());
    }
}

static void assertRecordIsArray(const Json& record)
{
    if (!record.is_array())
    {
        std::stringstream out;
        out << record;
        throw std::logic_error("Expected array but got this instead " + out.str());
    }
}

static void makeArray(Json& record)
{
    if (record.type() != Json::value_t::array)
    {
        record = Json::array({ record });
    }
}

static VariantTypeFromUser decodeVariantTypeFromUser(const string& encoding)
{
    if (encoding == "RareRepeat")
    {
        return VariantTypeFromUser::kRareRepeat;
    }
    if (encoding == "Repeat")
    {
        return VariantTypeFromUser::kCommonRepeat;
    }
    if (encoding == "SmallVariant")
    {
        return VariantTypeFromUser::kSmallVariant;
    }
    if (encoding == "SMN")
    {
        return VariantTypeFromUser::kSMN;
    }
    else
    {
        throw std::logic_error("Encountered invalid variant type: " + encoding);
    }
}

static vector<string> generateIds(const std::string& locusId, const Json& variantRegionEncodings)
{
    if (variantRegionEncodings.size() == 1)
    {
        return { locusId };
    }

    vector<string> variantIds;
    for (const string& regionEncoding : variantRegionEncodings)
    {
        variantIds.push_back(locusId + "_" + regionEncoding);
    }

    return variantIds;
}

static LocusDescriptionFromUser loadUserDescription(Json& locusJson, const ReferenceContigInfo& contigInfo)
{
    LocusDescriptionFromUser userDescription;

    assertFieldExists(locusJson, "LocusId");
    userDescription.locusId = locusJson["LocusId"];

    assertFieldExists(locusJson, "ReferenceRegion");
    makeArray(locusJson["ReferenceRegion"]);
    for (const string& encoding : locusJson["ReferenceRegion"])
    {
        GenomicRegion region = decode(contigInfo, encoding);
        userDescription.referenceRegions.push_back(region);
    }

    assertFieldExists(locusJson, "LocusStructure");
    userDescription.locusStructure = locusJson["LocusStructure"];

    assertFieldExists(locusJson, "VariantType");
    makeArray(locusJson["VariantType"]);
    for (const string& encoding : locusJson["VariantType"])
    {
        userDescription.variantTypesFromUser.push_back(decodeVariantTypeFromUser(encoding));
    }

    if (checkIfFieldExists(locusJson, "TargetRegion"))
    {
        makeArray(locusJson["TargetRegion"]);
        for (const string& locusEncoding : locusJson["TargetRegion"])
        {
            GenomicRegion region = decode(contigInfo, locusEncoding);
            userDescription.targetRegions.push_back(region);
        }
    }

    if (checkIfFieldExists(locusJson, "VariantId"))
    {
        makeArray(locusJson["VariantId"]);
        for (const string& variantId : locusJson["VariantId"])
        {
            userDescription.variantIds.push_back(variantId);
        }
    }
    else
    {
        userDescription.variantIds = generateIds(userDescription.locusId, locusJson["ReferenceRegion"]);
    }

    if (checkIfFieldExists(locusJson, "OfftargetRegions"))
    {
        assertRecordIsArray(locusJson["OfftargetRegions"]);
        for (const string& locusEncoding : locusJson["OfftargetRegions"])
        {
            GenomicRegion region = decode(contigInfo, locusEncoding);
            userDescription.offtargetRegions.push_back(region);
        }
    }

    return userDescription;
}

RegionCatalog loadLocusCatalogFromDisk(
    const string& catalogPath, Sex sampleSex, const HeuristicParameters& heuristicParams, const Reference& reference)
{
    std::ifstream inputStream(catalogPath.c_str());

    if (!inputStream.is_open())
    {
        throw std::runtime_error("Failed to open catalog file " + catalogPath);
    }

    Json catalogJson;
    inputStream >> catalogJson;
    makeArray(catalogJson);

    RegionCatalog catalog;
    for (auto& locusJson : catalogJson)
    {
        LocusDescriptionFromUser userDescription = loadUserDescription(locusJson, reference.contigInfo());
        LocusSpecification locusSpec = decodeLocusSpecification(userDescription, sampleSex, reference, heuristicParams);
        catalog.emplace(std::make_pair(locusSpec.locusId(), locusSpec));
    }

    return catalog;
}

}