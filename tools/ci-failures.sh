#!/usr/bin/env bash
# Prints why a CI run failed: per failed job, the failed step, the Catch FAILED blocks, aborts,
# the random seed and the test count. On a crash it names the test that ran before, from the
# local build's test order for that seed.
# Usage: tools/ci-failures.sh [run id]   (default: the latest run of the current branch)
set -euo pipefail

run="${1:-$(gh run list -b "$(git rev-parse --abbrev-ref HEAD)" -L 1 --json databaseId -q '.[0].databaseId')}"
repo="$(gh repo view --json nameWithOwner -q .nameWithOwner)"
sha="$(gh run view "$run" --json headSha -q .headSha)"
echo "run $run: $(gh run view "$run" --json status,conclusion,headSha -q '"\(.status) \(.conclusion) \(.headSha[0:7])"')"

tests=""
for exe in build/pencil2d_tests.exe build/pencil2d_tests; do [ -x "$exe" ] && tests="$exe" && break; done

gh run view "$run" --json jobs -q '.jobs[] | select(.conclusion == "failure") | "\(.databaseId)\t\(.name)\t\([.steps[] | select(.conclusion == "failure") | .name] | join(", "))"' |
while IFS=$'\t' read -r job name steps; do
    echo
    echo "== $name (failed step: $steps)"
    # Job logs are ready per job, before the whole run ends.
    log="$(gh api --allow-escape-sequences "repos/$repo/actions/jobs/$job/logs" 2>/dev/null | sed -E 's/\x1b\[[0-9;]*m//g; s/^[0-9T:.-]+Z //')"
    printf '%s\n' "$log" | grep -E -A4 'FAILED:' | grep -vE '^-+$|^\.+$' | head -60 || true
    printf '%s\n' "$log" | grep -E 'fatal error condition|SIG[A-Z]+|terminating due to|Aborted|Abort trap|ASSERT|runtime error|error: ' | sort -u | head -15 || true
    seed="$(printf '%s\n' "$log" | grep -oE 'rng-seed [0-9]+' | head -1 | cut -d' ' -f2 || true)"
    count="$(printf '%s\n' "$log" | grep -E '^test cases:' | tail -1 || true)"
    echo "seed: ${seed:-?}  ${count:-no test summary}"
    ran="$(printf '%s\n' "$count" | grep -oE '[0-9]+' | head -1 || true)"
    if [ -n "$tests" ] && [ -n "$seed" ] && [ -n "$ran" ] && grep -q 'fatal error condition' <<<"$log"; then
        # Catch orders tests the same on every platform for one seed and one test set.
        [ "$(git rev-parse HEAD)" = "$sha" ] || echo "(local HEAD is not ${sha:0:7}: build that commit for the true order)"
        order="$("$tests" --list-test-names-only --order rand --rng-seed "$seed" 2>/dev/null | tr -d '\r' || true)"
        printf '%s\n' "$order" | sed -n "$((ran > 1 ? ran - 1 : 1)),${ran}p" | sed '1s/^/ran before the crash: /; 2s/^/crashed in:           /'
    fi
done
