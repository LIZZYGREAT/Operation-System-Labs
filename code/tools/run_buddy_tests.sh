#!/usr/bin/env bash
# lab2 independent Buddy validation - 2412712
# Usage: bash tools/run_buddy_tests.sh /path/to/qemu-system-riscv64-4.1.1
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
qemu=${1:-${QEMU:-qemu-system-riscv64}}
version=$("$qemu" --version | sed -n '1p')
if [[ "$version" != 'QEMU emulator version 4.1.1' ]]; then
    printf 'Expected QEMU 4.1.1, got: %s\n' "$version" >&2
    exit 1
fi
log_dir=../report/buddy_tests
mkdir -p "$log_dir"
printf '%s\n' "$version" >"$log_dir/environment.log"
printf 'QEMU=%s\n' "$qemu" >>"$log_dir/environment.log"
riscv64-unknown-elf-gcc --version >>"$log_dir/environment.log"
: >"$log_dir/summary.log"

pass() {
    printf 'PASS %s\n' "$1" | tee -a "$log_dir/summary.log"
}

run_config() {
    local label=$1 backend=$2 enabled=$3 case_id=$4
    local args=("BUDDY_TEST=$enabled" "BUDDY_TEST_CASE=$case_id")
    if [[ "$backend" != default ]]; then
        args+=("PMM=$backend")
    fi
    make clean >"$log_dir/${label}_build.log" 2>&1
    make -j2 "${args[@]}" >>"$log_dir/${label}_build.log" 2>&1
    local marker='check_alloc_page() succeeded!'
    if ((case_id > 0)); then
        marker="assertion failed: ${conditions[case_id-1]}"
    fi
    : >"$log_dir/${label}.raw"
    # Wait for the result before Ctrl+A, X; timeout remains a failure guard.
    (
        deadline=$((SECONDS + 15))
        while ((SECONDS < deadline)); do
            if grep -Fq "$marker" "$log_dir/${label}.raw" 2>/dev/null; then
                sleep 0.1
                printf '\001x'
                exit 0
            fi
            sleep 0.1
        done
        exit 1
    ) | timeout 15s make --no-print-directory qemu \
        "${args[@]}" QEMU="$qemu" >"$log_dir/${label}.raw" 2>&1
    tr -d '\r' <"$log_dir/${label}.raw" | sed '${/^$/d;}' >"$log_dir/${label}.log"
    rm -f "$log_dir/${label}.raw"
    grep -Fq 'OpenSBI v0.4' "$log_dir/${label}.log"
}

run_config normal buddy 1 0
normal_log="$log_dir/normal.log"
for name in init_aligned init_unaligned_13 init_adjacent_regions init_reserved_gap \
    split_merge_nonbuddy rounding_original_length failure_preserves_state reuse_16_rounds; do
    grep -Fq "[Buddy test] PASS $name" "$normal_log"
done
grep -Fq '[Buddy test] ALL PASS; fixture restored; continuing real page_init' "$normal_log"
grep -Fq 'buddy_check: passed' "$normal_log"
grep -Fq 'check_alloc_page() succeeded!' "$normal_log"
if grep -Fq 'kernel panic' "$normal_log"; then
    exit 1
fi
pass 'normal: 8 scenarios, fixture restoration, real page_init and buddy_check'

names=(alloc_zero double_free wrong_length interior_free duplicate_register register_out_of_range)
conditions=(
    'n > 0'
    '!PageReserved(base) && !PageProperty(base)'
    '(base->flags & BUDDY_ALLOCATED) && base->property == n'
    '(base->flags & BUDDY_ALLOCATED) && base->property == n'
    'PageReserved(base + i)'
    'n <= npage - nbase - index'
)
for case_id in 1 2 3 4 5 6; do
    run_config "invalid_$case_id" buddy 1 "$case_id"
    log="$log_dir/invalid_${case_id}.log"
    name=${names[case_id-1]}
    condition=${conditions[case_id-1]}
    grep -Fq "[Buddy test] EXPECT_PANIC $case_id $name" "$log"
    grep -A1 -E '^kernel panic at kern/mm/buddy_pmm.c:[0-9]+:' "$log" \
        | grep -Fq "assertion failed: $condition"
    if grep -Eq 'BUDDY_TEST_FAIL|ALL PASS|check_alloc_page\(\) succeeded!' "$log"; then
        exit 1
    fi
    pass "invalid_$case_id: $name triggered the expected allocator assertion"
done

for backend in buddy first_fit best_fit default; do
    run_config "regression_$backend" "$backend" 0 0
    log="$log_dir/regression_${backend}.log"
    case "$backend" in
        buddy) manager=buddy_pmm_manager ;;
        first_fit) manager=default_pmm_manager ;;
        best_fit|default) manager=best_fit_pmm_manager ;;
    esac
    grep -Fq "memory management: $manager" "$log"
    grep -Fq 'check_alloc_page() succeeded!' "$log"
    if grep -Eq '\[Buddy test\]|kernel panic' "$log"; then
        exit 1
    fi
    pass "regression_$backend: tests disabled, original self-check passed"
done

reject_config() {
    local label=$1 expected=$2
    shift 2
    if make -n "$@" >"$log_dir/${label}.log" 2>&1; then
        printf 'Unexpectedly accepted configuration: %s\n' "$label" >&2
        exit 1
    fi
    grep -Fq "$expected" "$log_dir/${label}.log"
    pass "$label: rejected at build configuration"
}
reject_config reject_backend 'BUDDY_TEST=1 requires PMM=buddy' PMM=best_fit BUDDY_TEST=1
reject_config reject_case 'Invalid BUDDY_TEST_CASE' PMM=buddy BUDDY_TEST=1 BUDDY_TEST_CASE=7
reject_config reject_flag 'Invalid BUDDY_TEST' PMM=buddy BUDDY_TEST=2
reject_config reject_disabled_case 'BUDDY_TEST_CASE requires BUDDY_TEST=1' BUDDY_TEST_CASE=1
printf 'ALL VALIDATION PASSED; default Best-Fit build restored.\n' \
    | tee -a "$log_dir/summary.log"
