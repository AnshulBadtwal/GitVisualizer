#include <gtest/gtest.h>
#include "core/OidCache.h"

using Git::OidCache;

TEST(OidCache, MissThenHit) {
    OidCache<int> cache(10);
    EXPECT_EQ(cache.find("a"), nullptr);
    cache.put("a", 1, 1);
    const int* value = cache.find("a");
    ASSERT_NE(value, nullptr);
    EXPECT_EQ(*value, 1);
    EXPECT_EQ(cache.hits(), 1u);
    EXPECT_EQ(cache.misses(), 1u);
}

TEST(OidCache, EvictsLeastRecentlyUsed) {
    OidCache<int> cache(3);
    cache.put("a", 1, 1);
    cache.put("b", 2, 1);
    cache.put("c", 3, 1);
    cache.put("d", 4, 1);                 // over the limit: "a" is the oldest
    EXPECT_EQ(cache.find("a"), nullptr);
    EXPECT_NE(cache.find("b"), nullptr);
    EXPECT_NE(cache.find("d"), nullptr);
    EXPECT_EQ(cache.size(), 3u);
}

TEST(OidCache, FindRefreshesRecency) {
    OidCache<int> cache(3);
    cache.put("a", 1, 1);
    cache.put("b", 2, 1);
    cache.put("c", 3, 1);
    ASSERT_NE(cache.find("a"), nullptr);  // "a" is now the newest, "b" the oldest
    cache.put("d", 4, 1);
    EXPECT_NE(cache.find("a"), nullptr);
    EXPECT_EQ(cache.find("b"), nullptr);
}

TEST(OidCache, ReplacingAKeyUpdatesValueAndCost) {
    OidCache<int> cache(10);
    cache.put("a", 1, 4);
    cache.put("a", 2, 6);
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(cache.totalCost(), 6u);
    ASSERT_NE(cache.find("a"), nullptr);
    EXPECT_EQ(*cache.find("a"), 2);
}

TEST(OidCache, EntryLargerThanTheCacheIsNotStored) {
    OidCache<int> cache(5);
    cache.put("a", 1, 2);
    cache.put("huge", 2, 6);
    EXPECT_EQ(cache.find("huge"), nullptr);
    EXPECT_NE(cache.find("a"), nullptr);  // nothing was evicted to make room for it
}

TEST(OidCache, CostIsTrackedAcrossEviction) {
    OidCache<int> cache(10);
    cache.put("a", 1, 6);
    cache.put("b", 2, 6);                 // 12 > 10: "a" goes
    EXPECT_EQ(cache.size(), 1u);
    EXPECT_EQ(cache.totalCost(), 6u);
}

TEST(OidCache, ClearEmptiesEverything) {
    OidCache<int> cache(10);
    cache.put("a", 1, 3);
    cache.clear();
    EXPECT_EQ(cache.size(), 0u);
    EXPECT_EQ(cache.totalCost(), 0u);
    EXPECT_EQ(cache.find("a"), nullptr);
}
