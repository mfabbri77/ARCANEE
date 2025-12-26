<!-- _blueprint/knowledge/knowledge_manifest.md -->

# Knowledge Manifest (SHA256)

This manifest lists the generated knowledge files and their SHA256 hashes, to support deterministic mirroring into `/_blueprint/_knowledge/` and CI integrity checks.

## Files

| File | Size (bytes) | SHA256 |
|---|---:|---|
| blueprint_schema.md | 15501 | 548faf9ffe54364557aca9220745cf1aba273b84749fe989c7ab1cecfafbac6f |
| prompt meta rules v1 1.md | 10206 | 33dc0d615f7b7f8619c59cf907e87df25edf327ef3ee6a75dddbb9dbf27c74af |
| decision_log_template.md | 7748 | 5146ff39850756ba62221eae1d5dbf1040918069faa81bc5de631ff144bae8e0 |
| cr_template.md | 6373 | b3a9e3939ca78cb9892a63d1c1e3cd3f1b25d044ccb1209c51b8cdf033d43804 |
| implementation_checklist_schema.md | 7406 | 6b43639818eb8a97089005a4891b766a58f2bba9bad0458a3112e2860786cbb6 |
| repo_layout_and_targets.md | 7187 | 03f19e09a75712dcb0db2a07b0af52bcc9c3ab2a8e3d9dd77d7883ebb540952e |
| ci_reference.md | 8097 | 2ab329a88292c7f8dbfdb5938ef882fa9cbedf5d41c87cc46cac60dea080d9aa |
| logging_observability_policy_new.md | 8851 | 9aa7089a34e5304abdccd0da09179567eca96825d0fe3fead7e1e944aa89fdae |
| temp_dbg_policy.md | 4120 | 95b4f551ff6835818068468fd5619561cdf270d666b3831d65c4a2a878474a5a |
| cmake_playbook.md | 6706 | 17c03c89224d9a50da06d7578c90bec5f6457e67f3d24755b323adb6fa921a09 |
| dependency_policy.md | 7199 | eb089cab349067b3ba1cae42d3b6fc4c317f9d66dae5e1346d6f197efc858adc |
| testing_strategy.md | 6946 | ea9a4c057a4c2e84dd1ec64633b9adf99eeffcf9aea636a9057fa889401c0f1f |
| code_style_and_tooling.md | 5458 | eafdf0b312c55062aa7ab8ca5be71b01241c8867c02c87176ace40db1cc18177 |
| error_handling_playbook.md | 5870 | 67fda6304bc71390c11993d1e409b4ee8558e865915b764981c9fc73a2a8b781 |
| performance_benchmark_harness.md | 6083 | dd39e36700a465b7bd23f97f03e199b75fd207f4d32a99574cf1805354e836de |
| walkthrough_template.md | 5360 | 54b4dccd04e75c09118f3d50dce6e9da8ab50af64a6e3c75ab1dfc3bb7f2865e |
| cpp_api_abi_rules.md | 7890 | 033d5b4f7074fec7526b7c8716135ecf1f04f2ba996ff96c66b2901d28197379 |
| GPU_APIs_rules.md | 6987 | a0062af5d85fac345e2a81add3de1ed44adc1bacbf1ec05a39addb0fe6ab139e |
| multibackend_graphics_abstraction_rules.md | 7292 | acf1c18e78d03d63a9b69750c2b7402f5bbe23417906c171f2e5728b0a53eb8b |
| knowledge_index.md | 4949 | acf82d8a2ae0e8e981b6ed955996d49f2735405a0814e360cedce0c2a9aa2e3c |


## Recommended placement

Copy all files into:

- `/_blueprint/_knowledge/`

Optionally, mirror or symlink enforceable policies into:

- `/_blueprint/rules/`

(Exact layout depends on your validator’s path resolution; keep it deterministic.)

## Notes

- If any file is regenerated, its hash will change; treat that as a CR-worthy update in the blueprint system.
