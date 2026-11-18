# Exact canonical receipts

Counts are CTest entries. Mutants deliberately fail; their executed legal controls remain separate.

## cache/wildlife-1a/controls-baseline-01.log

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=2 executed=2 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

## cache/wildlife-1a/dev-focused-01.log

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=4 executed=4 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

## cache/wildlife-1a/asan-focused-01.log

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=4 executed=4 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

## cache/wildlife-1a/pins-partial-01.log

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=6 executed=6 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
PINS check: PASS; 0 pins moved; 0 fixture byte changes; scope=partial; selection=^anno_sim_tests\.(wildlife |recorder replay reproduces checkpoints$|write_log matches the documented frame byte for byte$); evidence=cache/wildlife-1a/pins-partial-01.log
```

## cache/wildlife-1a/pins-partial-02.log

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=3 executed=3 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
PINS check: PASS; 0 pins moved; 0 fixture byte changes; scope=partial; selection=^anno_sim_tests\.(core_hash equals the v1 golden$|golden G0 empty world seed 1234 advance 10 ms x 1000$|world core stream layout round trip and rejects$); evidence=cache/wildlife-1a/pins-partial-02.log
```

## cache/wildlife-1a/mutant-density_14.log

```text
COUNTS: selected=2 executed=1 failed=1 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
GATE_END {"status": 1, "ctest_rc": 8}
```

## cache/wildlife-1a/mutant-collision_draw.log

```text
COUNTS: selected=2 executed=1 failed=1 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
GATE_END {"status": 1, "ctest_rc": 8}
```

## cache/wildlife-1a/mutant-swap_pop.log

```text
COUNTS: selected=2 executed=1 failed=1 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
GATE_END {"status": 1, "ctest_rc": 8}
```

## cache/wildlife-1a/mutant-facing_wrap.log

```text
COUNTS: selected=2 executed=1 failed=1 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
GATE_END {"status": 1, "ctest_rc": 8}
```

## cache/wildlife-1a/mutant-attempt_wrap.log

```text
COUNTS: selected=2 executed=1 failed=1 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
GATE_END {"status": 1, "ctest_rc": 8}
```

## cache/wildlife-1a/mutant-idle_one_draw.log

```text
COUNTS: selected=2 executed=1 failed=1 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
GATE_END {"status": 1, "ctest_rc": 8}
```

## cache/wildlife-1a/mutant-quantity_before_draw.log

```text
COUNTS: selected=2 executed=1 failed=1 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
GATE_END {"status": 1, "ctest_rc": 8}
```

## cache/wildlife-1a/mutant-predator_503_extended.log

```text
COUNTS: selected=2 executed=1 failed=1 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
GATE_END {"status": 1, "ctest_rc": 8}
```

## Narrow lint follow-up

cache/wildlife-1a/lint-correction-01.log

```text
COUNTS: selected=6 executed=6 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
GATE_END {"status": 0, "ctest_rc": 0}
```

Original CI macOS artifact, retained failure (no local rerun):

```text
COUNTS: selected=1 executed=1 failed=0 empty=0 skipped=0 (required-asset=0 platform=0 opt-in=0 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: PASS
COUNTS: selected=2779 executed=2299 failed=1 empty=0 skipped=479 (required-asset=445 platform=9 opt-in=25 gpu=0 other=0) incomplete=0 sanitizer_lines=0
VERDICT: FAIL failed tests
GATE_END {"status": 1, "ctest_rc": 8}
```
