//
// Expansion Hunter
// Copyright 2016-2019 Illumina, Inc.
// All rights reserved.
//
// Author: Xiao Chen <xchen2@illumina.com>,
//         Egor Dolzhenko <edolzhenko@illumina.com>
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

#include "workflow/CNVVariantAnalyzer.hh"
#include "genotyping/CopyNumberGenotyper.hh"

#include "common/Common.hh"

using std::shared_ptr;
using std::vector;

namespace ehunter
{
CNVVariantAnalyzer::CNVVariantAnalyzer(
    std::string variantId, double regionLength, VariantSubtype variantSubtype, ContigCopyNumber contigCopyNumber,
    CnvGenotyperParameters cnvParameters, std::shared_ptr<ReadCounter> counter)
    : variantId_(variantId)
    , regionLength_(regionLength)
    , variantSubtype_(variantSubtype)
    , contigCopyNumber_(contigCopyNumber)
    , cnvParameters_(cnvParameters)
    , counter_(std::move(counter))

{
}

vector<shared_ptr<Feature>> CNVVariantAnalyzer::features() { return { counter_ }; }

CNVVariantFindings CNVVariantAnalyzer::analyze() const
{
    const int numReads = counter_->numReads();
    double normalizedDepth = (double)numReads / (double)regionLength_;

    CopyNumberGenotyper cnvGenotyper = CopyNumberGenotyper(
        cnvParameters_.maxCopyNumber, cnvParameters_.depthScaleFactor, cnvParameters_.standardDeviationOfCN2,
        cnvParameters_.meanDepthValues, cnvParameters_.priorCopyNumberFrequency);

    boost::optional<int> copyNumberCall = cnvGenotyper.genotype(normalizedDepth);

    return CNVVariantFindings(copyNumberCall);
}
}