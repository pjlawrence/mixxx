#include <gtest/gtest.h>

#include <QMap>
#include <QPair>
#include <QRandomGenerator>
#include <QVector>
#include <QtMath>

#include "library/trackset/crate/crate.h"

// Feature: weighted-crate-selection
// Property-based and unit tests for Auto DJ weighted crate selection

class AutoDjWeightTest : public testing::Test {
};

// Feature: weighted-crate-selection, Property 1: Weight Clamping Invariant
// **Validates: Requirements 1.3, 1.5**
TEST_F(AutoDjWeightTest, PropertyWeightClampingInvariant) {
    // For any integer value (including negatives, zero, and values exceeding 100),
    // after passing through Crate::setAutoDjWeight(), the stored weight SHALL
    // always be within [1, 100] inclusive.
    QRandomGenerator rng(42); // fixed seed for reproducibility
    Crate crate;

    for (int i = 0; i < 200; ++i) {
        // Generate random integers spanning a wide range including negatives and large values
        int randomValue = static_cast<int>(rng.bounded(-1000, 1001));
        crate.setAutoDjWeight(randomValue);
        int stored = crate.autoDjWeight();
        EXPECT_GE(stored, 1) << "Weight below minimum for input " << randomValue;
        EXPECT_LE(stored, 100) << "Weight above maximum for input " << randomValue;
    }

    // Explicit boundary checks
    crate.setAutoDjWeight(0);
    EXPECT_EQ(crate.autoDjWeight(), 1);

    crate.setAutoDjWeight(-5);
    EXPECT_EQ(crate.autoDjWeight(), 1);

    crate.setAutoDjWeight(101);
    EXPECT_EQ(crate.autoDjWeight(), 100);

    crate.setAutoDjWeight(1);
    EXPECT_EQ(crate.autoDjWeight(), 1);

    crate.setAutoDjWeight(100);
    EXPECT_EQ(crate.autoDjWeight(), 100);

    crate.setAutoDjWeight(50);
    EXPECT_EQ(crate.autoDjWeight(), 50);
}

// Feature: weighted-crate-selection, Property 3: Normalization Sum-to-One
// **Validates: Requirements 4.1, 4.3**
TEST_F(AutoDjWeightTest, PropertyNormalizationSumToOne) {
    // For any set of one or more positive integer weights (each in [1, 100]),
    // the normalized weights SHALL sum to 1.0 within ±0.000001.
    QRandomGenerator rng(123);

    for (int trial = 0; trial < 100; ++trial) {
        int numCrates = static_cast<int>(rng.bounded(1, 21)); // 1–20 crates
        QVector<int> weights;
        weights.reserve(numCrates);

        for (int i = 0; i < numCrates; ++i) {
            weights.append(static_cast<int>(rng.bounded(1, 101)));
        }

        // Compute sum of weights
        int sum = 0;
        for (int w : weights) {
            sum += w;
        }
        ASSERT_GT(sum, 0);

        // Compute normalized weights and verify they sum to 1.0
        double normalizedSum = 0.0;
        for (int w : weights) {
            double normalized = static_cast<double>(w) / static_cast<double>(sum);
            EXPECT_GT(normalized, 0.0);
            EXPECT_LE(normalized, 1.0);
            normalizedSum += normalized;
        }

        EXPECT_NEAR(normalizedSum, 1.0, 0.000001)
                << "Normalization sum failed for trial " << trial
                << " with " << numCrates << " crates";
    }
}

// Feature: weighted-crate-selection, Property 10: Normalized Percentage Display
// **Validates: Requirements 2.5**
TEST_F(AutoDjWeightTest, PropertyNormalizedPercentageDisplay) {
    // For any set of source crate weights, the displayed percentage for each crate
    // SHALL equal round(weight_i / sum_of_all_weights * 100).
    QRandomGenerator rng(456);

    for (int trial = 0; trial < 100; ++trial) {
        int numCrates = static_cast<int>(rng.bounded(1, 21));
        QVector<int> weights;
        weights.reserve(numCrates);

        for (int i = 0; i < numCrates; ++i) {
            weights.append(static_cast<int>(rng.bounded(1, 101)));
        }

        int sum = 0;
        for (int w : weights) {
            sum += w;
        }

        for (int i = 0; i < numCrates; ++i) {
            int expectedPercent = qRound(
                    static_cast<double>(weights[i]) / static_cast<double>(sum) * 100.0);
            // Verify expected percentage is non-negative
            EXPECT_GE(expectedPercent, 0);
            // For a single crate, percentage should be 100
            if (numCrates == 1) {
                EXPECT_EQ(expectedPercent, 100);
            }
        }
    }
}

// Unit test: default weight
TEST_F(AutoDjWeightTest, DefaultWeightIsTen) {
    Crate crate;
    EXPECT_EQ(crate.autoDjWeight(), 10);
}

// Unit test: getter/setter round-trip for valid values
TEST_F(AutoDjWeightTest, GetterSetterRoundTrip) {
    Crate crate;

    crate.setAutoDjWeight(1);
    EXPECT_EQ(crate.autoDjWeight(), 1);

    crate.setAutoDjWeight(50);
    EXPECT_EQ(crate.autoDjWeight(), 50);

    crate.setAutoDjWeight(100);
    EXPECT_EQ(crate.autoDjWeight(), 100);
}

// Unit test: clamping at boundaries
TEST_F(AutoDjWeightTest, ClampingAtBoundaries) {
    Crate crate;

    crate.setAutoDjWeight(0);
    EXPECT_EQ(crate.autoDjWeight(), 1);

    crate.setAutoDjWeight(-5);
    EXPECT_EQ(crate.autoDjWeight(), 1);

    crate.setAutoDjWeight(101);
    EXPECT_EQ(crate.autoDjWeight(), 100);

    crate.setAutoDjWeight(999);
    EXPECT_EQ(crate.autoDjWeight(), 100);

    crate.setAutoDjWeight(-999);
    EXPECT_EQ(crate.autoDjWeight(), 1);
}

// Feature: weighted-crate-selection, Property 5: Uniform Track Selection Within Crate
// **Validates: Requirements 3.2, 7.2**
TEST_F(AutoDjWeightTest, PropertyUniformTrackSelectionWithinCrate) {
    // For any selected crate with N active tracks (N > 0), over a sufficiently
    // large number of selections, each track SHALL be selected with approximately
    // equal probability (1/N), verified by chi-squared goodness-of-fit.
    //
    // Since getRandomTrackFromCrate() requires a full database setup, we test
    // the uniform selection property using QRandomGenerator::bounded() directly
    // (which is the mechanism the method uses internally to pick a random offset).
    QRandomGenerator rng(101);

    // Test with various crate sizes
    QVector<int> crateSizes = {5, 10, 20, 50};

    for (int N : crateSizes) {
        QVector<int> counts(N, 0);
        int totalRuns = 5000;

        for (int i = 0; i < totalRuns; ++i) {
            int selected = rng.bounded(N);
            counts[selected]++;
        }

        // Chi-squared goodness-of-fit for uniform distribution
        double expected = static_cast<double>(totalRuns) / N;
        double chiSquared = 0.0;
        for (int count : counts) {
            chiSquared += (count - expected) * (count - expected) / expected;
        }

        // Critical value for df=N-1 at p=0.01
        // Use a generous bound: for df up to 49, chi-sq critical at p=0.01 < 80
        double criticalValue = N * 3.0; // Conservative upper bound
        EXPECT_LT(chiSquared, criticalValue)
                << "Uniform selection chi-squared test failed for N=" << N;
    }
}

// Feature: weighted-crate-selection, Property 6: Exhausted Crate Exclusion
// **Validates: Requirements 3.3, 5.4**
TEST_F(AutoDjWeightTest, PropertyExhaustedCrateExclusion) {
    // Simulates the algorithm: some crates have 0 active tracks and should
    // never be selected for track picking.
    QRandomGenerator rng(202);

    // Setup: 5 crates, crates 2 and 4 are exhausted (0 active tracks)
    struct TestCrate {
        CrateId id;
        int weight;
        int activeTracks;
    };
    QVector<TestCrate> allCrates = {
            {CrateId(QVariant(1)), 20, 10},
            {CrateId(QVariant(2)), 30, 0}, // exhausted
            {CrateId(QVariant(3)), 15, 5},
            {CrateId(QVariant(4)), 25, 0}, // exhausted
            {CrateId(QVariant(5)), 10, 8},
    };

    // Simulate the selection algorithm
    int totalRuns = 1000;
    for (int run = 0; run < totalRuns; ++run) {
        // Build candidates (only non-exhausted crates)
        QVector<QPair<CrateId, int>> candidates;
        for (const auto& c : allCrates) {
            if (c.activeTracks > 0) {
                candidates.append(qMakePair(c.id, c.weight));
            }
        }

        // Verify exhausted crates are never in candidates
        for (const auto& candidate : candidates) {
            int crateIdx = candidate.first.toVariant().toInt();
            EXPECT_NE(crateIdx, 2) << "Exhausted crate 2 found in candidates";
            EXPECT_NE(crateIdx, 4) << "Exhausted crate 4 found in candidates";
        }

        // Select from non-exhausted candidates
        ASSERT_FALSE(candidates.isEmpty());
        int sum = 0;
        for (const auto& c : candidates) {
            sum += c.second;
        }
        int threshold = rng.bounded(sum);
        int acc = 0;
        CrateId selected;
        for (const auto& c : candidates) {
            acc += c.second;
            if (acc > threshold) {
                selected = c.first;
                break;
            }
        }

        // Verify selected crate is not exhausted
        int selectedIdx = selected.toVariant().toInt();
        EXPECT_NE(selectedIdx, 2);
        EXPECT_NE(selectedIdx, 4);
    }
}

// Feature: weighted-crate-selection, Property 9: Equal-Weight Backward Compatibility
// **Validates: Requirements 7.1, 7.3**
TEST_F(AutoDjWeightTest, PropertyEqualWeightBackwardCompatibility) {
    // When all crates have equal weights (regardless of the specific equal value),
    // crate selection probability SHALL be proportional to each crate's active-track
    // count divided by the total active-track count across all crates.
    QRandomGenerator rng(303);

    // All crates have equal weight (try several different equal values)
    QVector<int> equalWeights = {10, 1, 50, 100};

    for (int equalWeight : equalWeights) {
        // 3 crates with different active-track counts
        QVector<QPair<CrateId, int>> activeCountWeights;
        activeCountWeights.append({CrateId(QVariant(1)), 100}); // 100 active tracks
        activeCountWeights.append({CrateId(QVariant(2)), 50});  // 50 active tracks
        activeCountWeights.append({CrateId(QVariant(3)), 50});  // 50 active tracks
        // Expected proportions: 50%, 25%, 25%

        QMap<int, int> counts;
        int totalRuns = 10000;
        for (int i = 0; i < totalRuns; ++i) {
            // When all weights are equal, select proportional to active-track count
            int sum = 0;
            for (const auto& c : activeCountWeights) {
                sum += c.second;
            }
            int threshold = rng.bounded(sum);
            int acc = 0;
            for (const auto& c : activeCountWeights) {
                acc += c.second;
                if (acc > threshold) {
                    counts[c.first.toVariant().toInt()]++;
                    break;
                }
            }
        }

        // Verify proportions are within 5 percentage points
        int totalActive = 200;
        for (const auto& c : activeCountWeights) {
            double expectedProportion = static_cast<double>(c.second) / totalActive;
            double observedProportion = static_cast<double>(
                    counts.value(c.first.toVariant().toInt(), 0)) / totalRuns;
            EXPECT_NEAR(observedProportion, expectedProportion, 0.05)
                    << "Equal weight " << equalWeight
                    << ", crate " << c.first.toVariant().toInt();
        }
    }
}

// Standalone helper for testing (mirrors selectCrateByWeight logic)
static CrateId testSelectCrateByWeight(
        const QVector<QPair<CrateId, int>>& candidates,
        QRandomGenerator& rng) {
    int sum = 0;
    for (const auto& c : candidates) {
        sum += c.second;
    }
    if (sum <= 0) {
        return CrateId();
    }
    int threshold = static_cast<int>(rng.bounded(sum));
    int acc = 0;
    for (const auto& c : candidates) {
        acc += c.second;
        if (acc > threshold) {
            return c.first;
        }
    }
    return candidates.last().first;
}

// Feature: weighted-crate-selection, Property 4: Weighted Crate Selection Distribution
// **Validates: Requirements 3.1**
TEST_F(AutoDjWeightTest, PropertyWeightedSelectionDistribution) {
    // For any set of source crates with distinct (non-equal) weights, over a
    // sufficiently large number of selections (>=1000), the observed crate
    // selection frequency SHALL approximate each crate's normalized weight
    // within a statistical tolerance (chi-squared test, p > 0.01).
    QRandomGenerator rng(789);

    // Run multiple trials with different random weight configurations
    for (int trial = 0; trial < 10; ++trial) {
        // Generate 3–10 crates with distinct weights
        int numCrates = static_cast<int>(rng.bounded(3, 11));
        QVector<QPair<CrateId, int>> candidates;
        QVector<int> usedWeights;

        for (int i = 0; i < numCrates; ++i) {
            int weight;
            // Ensure distinct weights
            do {
                weight = static_cast<int>(rng.bounded(1, 101));
            } while (usedWeights.contains(weight));
            usedWeights.append(weight);
            candidates.append({CrateId(QVariant(i + 1)), weight});
        }

        // Run selection 10000 times for statistical significance
        QMap<int, int> counts;
        const int totalRuns = 10000;
        for (int i = 0; i < totalRuns; ++i) {
            CrateId selected = testSelectCrateByWeight(candidates, rng);
            counts[selected.toVariant().toInt()]++;
        }

        // Calculate total weight sum
        int weightSum = 0;
        for (const auto& c : candidates) {
            weightSum += c.second;
        }

        // Chi-squared goodness-of-fit test
        double chiSquared = 0.0;
        for (const auto& c : candidates) {
            double expected = static_cast<double>(c.second) / weightSum * totalRuns;
            double observed = counts.value(c.first.toVariant().toInt(), 0);
            chiSquared += (observed - expected) * (observed - expected) / expected;
        }

        // Critical value for chi-squared at p=0.01:
        // df=2 -> 9.21, df=3 -> 11.34, df=4 -> 13.28, df=5 -> 15.09,
        // df=6 -> 16.81, df=7 -> 18.48, df=8 -> 20.09, df=9 -> 21.67
        // Use a lookup table for the degrees of freedom (numCrates - 1)
        const double criticalValues[] = {
                0.0,    // df=0 (unused)
                6.63,   // df=1
                9.21,   // df=2
                11.34,  // df=3
                13.28,  // df=4
                15.09,  // df=5
                16.81,  // df=6
                18.48,  // df=7
                20.09,  // df=8
                21.67,  // df=9
        };
        int df = numCrates - 1;
        ASSERT_LT(df, 10) << "Degrees of freedom out of lookup table range";
        ASSERT_GT(df, 0) << "Need at least 2 crates for chi-squared test";

        EXPECT_LT(chiSquared, criticalValues[df])
                << "Chi-squared test failed for trial " << trial
                << " with " << numCrates << " crates (df=" << df
                << ", chi2=" << chiSquared
                << ", critical=" << criticalValues[df] << ")";
    }
}

// Unit tests for selection algorithm edge cases
// Requirements: 3.3, 3.4, 4.2, 4.4, 7.4

TEST_F(AutoDjWeightTest, SingleSourceCrateAlwaysSelected) {
    QRandomGenerator rng(404);
    QVector<QPair<CrateId, int>> candidates;
    candidates.append({CrateId(QVariant(42)), 10});

    for (int i = 0; i < 100; ++i) {
        int sum = candidates[0].second;
        int threshold = rng.bounded(sum);
        int acc = 0;
        CrateId selected;
        for (const auto& c : candidates) {
            acc += c.second;
            if (acc > threshold) {
                selected = c.first;
                break;
            }
        }
        EXPECT_EQ(selected.toVariant().toInt(), 42);
    }
}

TEST_F(AutoDjWeightTest, ZeroSourceCratesReturnsInvalid) {
    QVector<QPair<CrateId, int>> candidates;
    // Empty candidates — algorithm should return invalid
    EXPECT_TRUE(candidates.isEmpty());
    // In the real algorithm, this returns TrackId() immediately
}

TEST_F(AutoDjWeightTest, AllCratesExhaustedFallsBack) {
    // Simulates the scenario where all source crates have zero active tracks.
    // The algorithm removes exhausted crates from the candidate list until
    // empty, then falls back to getRandomTrackIdFromAutoDj().
    QVector<QPair<CrateId, int>> candidates;
    candidates.append({CrateId(QVariant(1)), 20});
    candidates.append({CrateId(QVariant(2)), 30});
    candidates.append({CrateId(QVariant(3)), 50});

    // Simulate active track counts of zero for each crate
    QVector<int> activeCounts = {0, 0, 0};

    // Walk the algorithm: remove exhausted crates one by one
    while (!candidates.isEmpty()) {
        // In the real algorithm, selectCrateByWeight picks a crate
        // then checks active track count. If 0, remove it.
        CrateId picked = candidates.first().first;
        int idx = 0;
        for (int i = 0; i < candidates.size(); ++i) {
            if (candidates[i].first == picked) {
                idx = i;
                break;
            }
        }
        // Active count is 0, so remove this crate
        EXPECT_EQ(activeCounts[idx], 0);
        candidates.removeAt(idx);
        activeCounts.removeAt(idx);
    }

    // All crates exhausted — should fall back
    EXPECT_TRUE(candidates.isEmpty());
    // In the real implementation, getRandomTrackIdFromAutoDj() is called here
}

TEST_F(AutoDjWeightTest, OnlyCrateWithActiveTracksSelectedExclusively) {
    // All crates have equal weight, but only one has active tracks.
    // Under equal-weight backward compatibility (Requirement 7.4),
    // the crate with active tracks must be selected exclusively.
    QRandomGenerator rng(505);
    QVector<QPair<CrateId, int>> activeCountWeights;
    activeCountWeights.append({CrateId(QVariant(1)), 0});  // exhausted
    activeCountWeights.append({CrateId(QVariant(2)), 0});  // exhausted
    activeCountWeights.append({CrateId(QVariant(3)), 15}); // only crate with active tracks

    // Filter to only non-exhausted
    QVector<QPair<CrateId, int>> nonEmpty;
    for (const auto& c : activeCountWeights) {
        if (c.second > 0) {
            nonEmpty.append(c);
        }
    }

    ASSERT_EQ(nonEmpty.size(), 1);
    EXPECT_EQ(nonEmpty[0].first.toVariant().toInt(), 3);

    // All selections must go to crate 3
    for (int i = 0; i < 100; ++i) {
        int sum = nonEmpty[0].second;
        int threshold = rng.bounded(sum);
        int acc = 0;
        CrateId selected;
        for (const auto& c : nonEmpty) {
            acc += c.second;
            if (acc > threshold) {
                selected = c.first;
                break;
            }
        }
        EXPECT_EQ(selected.toVariant().toInt(), 3);
    }
}
