# StackAlign framework migration provenance

This branch replaces the former DawChecker audio/VST3 application with the StackAlign Bash/Linux framework.

- Upstream repository: https://github.com/ljrseals-dev/StackAlign
- Honey Trap source branch: feat/honeytrap-audit-dispatch-correlation
- Exact Honey Trap source commit: de828ecaafb4bd7a0b24fc2bd3f2d2ad60d2788c
- Upstream PR: https://github.com/ljrseals-dev/StackAlign/pull/92
- Imported source tree: de828ecaafb4bd7a0b24fc2bd3f2d2ad60d2788c
- Imported source files: 144 text/source/configuration files. Upstream binary PDF and ZIP reference artifacts were intentionally excluded.
- License: MIT; upstream LICENSE and copyright/SPDX attribution are retained.
- Git blob IDs are content-addressed and happen to remain identical for imported content; commit IDs are repository-history-specific and are not expected to match.

Validation boundary: CI must run ShellCheck, bash syntax/security lint, header checks, and make test on Ubuntu, AlmaLinux 9, and CentOS Stream 9 before the draft PR is marked ready. No such checks have been executed by this migration operation. Fixture tests do not prove real-host auditd or Fail2Ban enforcement.

The framework import and removal of the legacy DawChecker application are separate commits on migrate/stackalign-framework. Dawchecker main remains unchanged.
