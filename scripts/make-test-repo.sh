#!/usr/bin/env bash
# Builds a small repository with branches, merges, an octopus merge, a long-lived branch,
# tags and remote-tracking branches, for testing the graph by eye.
#
#   bash scripts/make-test-repo.sh [target-dir]      (default: /tmp/git-viz-test-repo)
set -euo pipefail

dir="${1:-/tmp/git-viz-test-repo}"
rm -rf "$dir"
git init -q -b main "$dir"
cd "$dir"
git config user.name "Test User"
git config user.email "test@example.com"

n=0
tick() { n=$((n + 1)); export GIT_AUTHOR_DATE="$((1760000000 + n * 60)) +0000"; export GIT_COMMITTER_DATE="$GIT_AUTHOR_DATE"; }
c() { tick; git commit --allow-empty -q -m "$1"; }
merge() { tick; git merge -q --no-ff "$@"; }

c "Initial commit"
c "Add README"

# A feature branch merged back into main
git switch -q -c feature/login
c "Login form"
c "Login validation"
git switch -q main
c "Fix typo"
merge feature/login -m "Merge feature/login"

# A long-lived branch: its line has to stay visible while main moves on for 40 commits
git switch -q -c long-lived
c "Start long-lived work"
git switch -q main
for i in $(seq 1 40); do c "Main work $i"; done
git switch -q long-lived
c "More long-lived work"
git switch -q main
merge long-lived -m "Merge long-lived"

# Two branches that fork from the same commit
git switch -q -c hotfix
c "Hotfix"
git switch -q main
git switch -q -c experiment
c "Experiment A"
c "Experiment B"
git switch -q main
merge hotfix -m "Merge hotfix"

# An octopus merge: three parents at once
for b in a b c; do
  git switch -q -c "topic-$b" main
  c "Topic $b"
  git switch -q main
done
merge topic-a topic-b topic-c -m "Octopus merge of topics a, b and c"

c "Release prep"
git tag v1.0
git switch -q -c release/1.1 main
c "Release 1.1 fixes"
git switch -q main
c "After release"
git tag -a v1.1 -m "Annotated tag"

# Remote-tracking branches (simulated)
git update-ref refs/remotes/origin/main "main~3"
git update-ref refs/remotes/origin/feature/login "feature/login"

echo "Created $dir with $(git rev-list --all --count) commits."
git --no-pager log --graph --oneline --all | head -20
