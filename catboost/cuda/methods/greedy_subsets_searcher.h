#pragma once

#include "helpers.h"

#include "catboost/cuda/models/add_oblivious_tree_model_doc_parallel.h"
#include <catboost/cuda/cuda_lib/cuda_buffer.h>
#include <catboost/cuda/cuda_lib/cuda_manager.h>
#include <catboost/cuda/cuda_lib/cuda_profiler.h>
#include <catboost/cuda/models/oblivious_model.h>
#include <catboost/cuda/methods/leaves_estimation/leaves_estimation_config.h>
#include <catboost/cuda/methods/greedy_subsets_searcher/structure_searcher_template.h>
#include <catboost/cuda/methods/leaves_estimation/doc_parallel_leaves_estimator.h>
#include <catboost/libs/options/catboost_options.h>

namespace NCatboostCuda {

    inline TTreeStructureSearcherOptions MakeStructureSearcherOptions(const NCatboostOptions::TObliviousTreeLearnerOptions& config) {
        TTreeStructureSearcherOptions options;
        options.ScoreFunction = config.ScoreFunction;
        options.BootstrapOptions = config.BootstrapConfig;
        options.MaxLeaves = 1 << config.MaxDepth;
        options.MaxDepth = config.MaxDepth;
        options.MinLeafSize = 0;
        options.L2Reg = config.L2Reg;
        options.Policy = EGrowingPolicy::ObliviousTree;
        options.RandomStrength = config.RandomStrength;
        return options;
    }

    class TGreedySubsetsSearcher {
    public:
        using TResultModel = TObliviousTreeModel;
        using TWeakModelStructure = TObliviousTreeStructure;
        using TDataSet = TDocParallelDataSet;

        TGreedySubsetsSearcher(const TBinarizedFeaturesManager& featuresManager,
                               const NCatboostOptions::TCatBoostOptions& config,
                               bool makeZeroAverage = false)
            : FeaturesManager(featuresManager)
            , TreeConfig(config.ObliviousTreeOptions)
            , StructureSearcherOptions(MakeStructureSearcherOptions(config.ObliviousTreeOptions))
            , MakeZeroAverage(makeZeroAverage)
            , ZeroLastBinInMulticlassHack(config.LossFunctionDescription->GetLossFunction() == ELossFunction::MultiClass)
        {
        }

        bool NeedEstimation() const {
            return TreeConfig.LeavesEstimationMethod != ELeavesEstimation::Simple;
        }

        template <class TTarget,
                  class TDataSet>
        TGreedyTreeLikeStructureSearcher  CreateStructureSearcher(double randomStrengthMult) {
            TTreeStructureSearcherOptions options=  StructureSearcherOptions;
            options.RandomStrength *= randomStrengthMult;
            return TGreedyTreeLikeStructureSearcher(FeaturesManager,
                                                    options);
        }

        TDocParallelLeavesEstimator CreateEstimator() {
            CB_ENSURE(NeedEstimation());
            return TDocParallelLeavesEstimator(CreateLeavesEstimationConfig(TreeConfig, MakeZeroAverage));
        }

        template <class TDataSet>
        TAddDocParallelObliviousTree CreateAddModelValue(bool useStreams = false) {
            return TAddDocParallelObliviousTree(useStreams);
        }

    private:
        const TBinarizedFeaturesManager& FeaturesManager;
        const NCatboostOptions::TObliviousTreeLearnerOptions& TreeConfig;
        TTreeStructureSearcherOptions StructureSearcherOptions;
        bool MakeZeroAverage = false;
        bool ZeroLastBinInMulticlassHack = false;
    };
}
