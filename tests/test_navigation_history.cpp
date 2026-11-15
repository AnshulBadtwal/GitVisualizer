#include <gtest/gtest.h>
#include "core/NavigationHistory.h"

using Git::NavigationHistory;

TEST(NavigationHistory, StartsEmpty) {
    NavigationHistory history;
    EXPECT_FALSE(history.canGoBack());
    EXPECT_FALSE(history.canGoForward());
    EXPECT_EQ(history.current(), std::string());
    EXPECT_FALSE(history.back().has_value());
    EXPECT_FALSE(history.forward().has_value());
}

TEST(NavigationHistory, FirstVisitHasNowhereToGoBackTo) {
    NavigationHistory history;
    history.visit("a");
    EXPECT_EQ(history.current(), std::string("a"));
    EXPECT_FALSE(history.canGoBack());
}

TEST(NavigationHistory, BackAndForwardWalkTheVisitedCommits) {
    NavigationHistory history;
    history.visit("a");
    history.visit("b");
    history.visit("c");

    EXPECT_EQ(*history.back(), std::string("b"));
    EXPECT_EQ(*history.back(), std::string("a"));
    EXPECT_FALSE(history.canGoBack());
    EXPECT_TRUE(history.canGoForward());

    EXPECT_EQ(*history.forward(), std::string("b"));
    EXPECT_EQ(*history.forward(), std::string("c"));
    EXPECT_FALSE(history.canGoForward());
    EXPECT_EQ(history.current(), std::string("c"));
}

TEST(NavigationHistory, VisitingTheCurrentCommitAgainChangesNothing) {
    NavigationHistory history;
    history.visit("a");
    history.visit("a");
    history.visit("b");
    history.visit("b");
    EXPECT_EQ(*history.back(), std::string("a"));
    EXPECT_FALSE(history.canGoBack());
}

TEST(NavigationHistory, VisitingAfterGoingBackDropsTheForwardPath) {
    NavigationHistory history;
    history.visit("a");
    history.visit("b");
    history.visit("c");
    EXPECT_EQ(*history.back(), std::string("b"));   // now on b, with c ahead

    history.visit("d");                             // a new path
    EXPECT_FALSE(history.canGoForward());
    EXPECT_EQ(*history.back(), std::string("b"));
    EXPECT_EQ(*history.back(), std::string("a"));
}

TEST(NavigationHistory, EmptyOidIsIgnored) {
    NavigationHistory history;
    history.visit("a");
    history.visit("");
    EXPECT_EQ(history.current(), std::string("a"));
    EXPECT_FALSE(history.canGoBack());
}

TEST(NavigationHistory, BackSkipsCommitsThatNoLongerExist) {
    NavigationHistory history;
    history.visit("a");
    history.visit("b");
    history.visit("c");
    history.visit("d");

    const auto exists = [](const std::string &oid) { return oid != "c"; };   // a reload removed "c"
    EXPECT_EQ(*history.back(exists), std::string("b"));   // skipped "c"
    EXPECT_EQ(history.current(), std::string("b"));
    EXPECT_EQ(*history.forward(), std::string("d"));      // "c" was dropped, not kept for Forward
}

TEST(NavigationHistory, BackWithOnlyMissingCommitsGivesNothingAndKeepsTheCurrentOne) {
    NavigationHistory history;
    history.visit("a");
    history.visit("b");
    history.visit("c");

    const auto nothingExists = [](const std::string &) { return false; };
    EXPECT_FALSE(history.back(nothingExists).has_value());
    EXPECT_EQ(history.current(), std::string("c"));
    EXPECT_FALSE(history.canGoBack());                    // the missing entries were dropped
}

TEST(NavigationHistory, ForwardSkipsCommitsThatNoLongerExist) {
    NavigationHistory history;
    history.visit("a");
    history.visit("b");
    history.visit("c");
    EXPECT_EQ(*history.back(), std::string("b"));
    EXPECT_EQ(*history.back(), std::string("a"));

    const auto exists = [](const std::string &oid) { return oid != "b"; };
    EXPECT_EQ(*history.forward(exists), std::string("c"));
    EXPECT_EQ(history.current(), std::string("c"));
}

TEST(NavigationHistory, ClearForgetsEverything) {
    NavigationHistory history;
    history.visit("a");
    history.visit("b");
    EXPECT_EQ(*history.back(), std::string("a"));
    history.clear();
    EXPECT_FALSE(history.canGoBack());
    EXPECT_FALSE(history.canGoForward());
    EXPECT_EQ(history.current(), std::string());
}
