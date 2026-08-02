# Scheme Python to Java Batch Conversion Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Convert the remaining 15 `Scheme*.py` implementations into behavior-compatible, single-file Java programs modeled on `SchemeAAIBME.java`.

**Architecture:** Every target remains a self-contained Java source file with its own parser, saver, JPBC adapter, scheme state, typed result records, experiment runner, and `main`. Symmetric schemes are completed first against Type A parameters; asymmetric schemes add an explicit curve-availability layer and never substitute a different curve under a Python curve name.

**Tech Stack:** Java 17+ source-file mode, JPBC 2.0.0, Apache POI, POSIX shell commands, GitHub Actions, Markdown.

---

## File Map

**Create:**

- `SchemeAAIBME/SchemeFuzzyME.java` — fuzzy match encryption.
- `SchemeIBMEMR/SchemeIBMEMR.java` — identity-based matching encryption with receiver verification.
- `SchemeIBMETR/SchemeAIBE.java` — anonymous identity-based encryption.
- `SchemeIBMETR/SchemeARES.java` — anonymous receiver-encrypted signature.
- `SchemeIBMETR/SchemeIBME.java` — identity-based matching encryption.
- `SchemeIBMETR/SchemeIBMETR.java` — identity-based matching encryption with third-party verification.
- `SchemeIBPRME/SchemeIBPRME.java` — identity-based proxy re-encryption with matching.
- `SchemeIBPRME/SchemePBAC.java` — pairing-based access control/proxy encryption.
- `SchemeCANIFPPCT/SchemeCANIFPPCT.java` — accountable/non-interactive fair privacy-preserving contact tracing.
- `SchemeHIBME/SchemeAnonymousME.java` — anonymous hierarchical matching encryption.
- `SchemeHIBME/SchemeHIBME.java` — hierarchical identity-based matching encryption.
- `SchemeIBMEMR/SchemeIBBME.java` — identity-based broadcast matching encryption.
- `SchemeIBMETR/SchemeIBMECH.java` — identity-based matching encryption with chameleon-hash matrix logic.
- `SchemeIBPRME/SchemeIBPME.java` — identity-based proxy matching encryption.
- `SchemeVLPSICA/SchemeVLPSICA.java` — verifiable lightweight private set intersection/cardinality.

**Modify:**

- `.github/workflows/runJava.yml` — expose and execute all 16 Java programs.
- `README.md` — list all Java implementations and document execution/curve behavior.

**Reference without changing unless a discovered defect requires it:**

- `SchemeAAIBME/SchemeAAIBME.java` — canonical Java structure and utility behavior.
- Each target's same-directory `.py` file — algorithm and console-output authority.
- `lib/fetchJavaDependencies.sh` — class-path authority.

## Common Per-File Contract

For every target Java file, implement the same block order shown by this representative `SchemeAIBE` layout:

```java
import java...; // standard-library imports, alphabetically sorted

import it.unisa.dia.gas...; // external imports, alphabetically sorted
import org.apache.poi...;

final class Parser { /* command-line parsing and source-relative paths */ }
final class Saver { /* CSV/HTML/JSON/TeX/text/XML/YAML/XLS/XLSX output */ }
public final class SchemeAIBE {
	/* private state, private helpers, public algorithm methods, result records, main */
}
```

The parser and saver behavior must match `SchemeAAIBME.java`: relative output paths resolve against the Java source directory, integral spreadsheet values use numeric cells without decimal text, overwriting is checked, and all variables are initialized. Copying is allowed only as a starting point; scheme names, defaults, columns, prompts, and result records must be reconciled with the target Python file.

For each algorithm method, the first test is a state/shape failure call made before its prerequisites. The expected result is the Python-equivalent failure value rather than an exception. The success test executes the exact stage sequence listed in the task and asserts recovered messages or verification booleans.

Use this class path in all commands:

```bash
dependencies="$(./lib/fetchJavaDependencies.sh)"
classpath="lib/jpbc-api-2.0.0.jar:lib/jpbc-plaf-2.0.0.jar:${dependencies}"
```

Strict compilation and the minimum dynamic check use a helper that accepts an exact source path. The temporary output directory prevents `.class` files from entering the repository:

```bash
verify_java() {
	readonly target="$1"
	readonly scheme="$(basename "${target}" .java)"
	rm -rf /tmp/cryptographic-schemes-java-classes
	mkdir -p /tmp/cryptographic-schemes-java-classes
	javac -Xlint:all -Werror -cp "${classpath}" \
		-d /tmp/cryptographic-schemes-java-classes "${target}"
	java -cp "${classpath}" "${target}" -o "/tmp/${scheme}.xlsx" -r 1
	test -s "/tmp/${scheme}.xlsx"
}
```

After every Java edit, check tabs/imports/end-of-file with:

```bash
TARGET="SchemeIBMETR/SchemeAIBE.java" python3 - <<'PY'
import os
from pathlib import Path

path = Path(os.environ["TARGET"])
data = path.read_bytes()
assert data and not data.endswith(b"\n")
for number, line in enumerate(data.decode().splitlines(), 1):
	indent = line[:len(line) - len(line.lstrip())]
	assert " " not in indent, (number, repr(indent))
PY
```

### Task 1: Preserve the Baseline and Record the Shared Contract

**Files:**
- Reference: `SchemeAAIBME/SchemeAAIBME.java`
- Reference: `SchemeAAIBME/SchemeAAIBME.py`

- [ ] Confirm the working branch is `feat/convert-schemes-to-java` and record `git status --short` so the pre-existing `README.md`, `SchemeAAIBME.java`, and `.DS_Store` changes are not lost.
- [ ] Build the dependency class path with the common command and strictly compile `SchemeAAIBME/SchemeAAIBME.java`; expect exit code 0.
- [ ] Run `SchemeAAIBME.java -o /tmp/SchemeAAIBME-baseline.xlsx -r 1`; expect a nonempty workbook and Python-compatible `Is EKey Sanity?`, `Is DKey Sanity?`, `Is Trace 1?`, and `Is Trace 2?` prompts.
- [ ] Record the reusable parser/saver ranges and the scheme-specific boundary; do not create a shared Java source file.

### Task 2: Convert SchemeAIBE

**Files:**
- Create: `SchemeIBMETR/SchemeAIBE.java`
- Reference: `SchemeIBMETR/SchemeAIBE.py`

- [ ] Create the single-file skeleton from the common contract with class name `SchemeAIBE`, AIBE defaults, columns, and prompts.
- [ ] Port `Setup`, `Extract`, `Encrypt`, and `Decrypt` in that order; map Python tuple returns to immutable records and copy every JPBC `Element` before mutation.
- [ ] Add failure probes for `Extract` before `Setup`, malformed identity, and `Decrypt` with malformed ciphertext.
- [ ] Run the strict compile and minimum dynamic commands; expect the decrypted message to equal the generated message.
- [ ] Run the formatting/end-of-file check and commit only `SchemeIBMETR/SchemeAIBE.java`.

### Task 3: Convert SchemeIBME

**Files:**
- Create: `SchemeIBMETR/SchemeIBME.java`
- Reference: `SchemeIBMETR/SchemeIBME.py`

- [ ] Create the self-contained parser, saver, scheme class, typed records, runner, and `main` using the Python defaults and output columns.
- [ ] Port `Setup`, `SKGen`, `RKGen`, `Enc`, and `Dec`; preserve the sender/receiver identity hashing and byte-to-message conversion exactly.
- [ ] Exercise `SKGen` and `RKGen` before setup, then run `Setup -> SKGen -> RKGen -> Enc -> Dec`; expect the failure probes to be rejected and the success message to be recovered.
- [ ] Strictly compile, run one SS512 experiment, inspect the workbook, run the style check, and commit `SchemeIBMETR/SchemeIBME.java`.

### Task 4: Convert SchemeARES

**Files:**
- Create: `SchemeIBMETR/SchemeARES.java`
- Reference: `SchemeIBMETR/SchemeARES.py`

- [ ] Implement the common shell with the ARES-specific result columns and console strings.
- [ ] Port `Setup`, `Extract`, `TSK`, `Encrypt`, `Decrypt`, and `TVerify`, preserving the trapdoor/signature equations and failure values.
- [ ] Test the invalid setup order and malformed ciphertext; then run `Setup -> Extract -> TSK -> Encrypt -> Decrypt -> TVerify` and require message recovery plus successful third-party verification.
- [ ] Strictly compile, run one SS512 experiment, validate workbook integer cells, run the style check, and commit `SchemeIBMETR/SchemeARES.java`.

### Task 5: Convert SchemeFuzzyME

**Files:**
- Create: `SchemeAAIBME/SchemeFuzzyME.java`
- Reference: `SchemeAAIBME/SchemeFuzzyME.py`

- [ ] Implement the common shell with `n` and threshold `d` parsing and Python-identical prompts.
- [ ] Port polynomial/product helpers plus `Setup`, `EKGen`, `DKGen`, `Encryption`, and `Decryption`; use deterministic index ordering when a Java set feeds polynomial or tuple positions.
- [ ] Test invalid thresholds and attribute-set cardinalities, then execute `Setup -> EKGen -> DKGen -> Encryption -> Decryption` for both sufficient and insufficient overlap.
- [ ] Strictly compile, run one SS512 experiment, run the style check, and commit `SchemeAAIBME/SchemeFuzzyME.java`.

### Task 6: Convert SchemeIBMEMR

**Files:**
- Create: `SchemeIBMEMR/SchemeIBMEMR.java`
- Reference: `SchemeIBMEMR/SchemeIBMEMR.py`

- [ ] Implement the common shell with the Python seed, dimension, run, and output behavior.
- [ ] Port coefficient/concatenation/polynomial helpers plus `Setup`, `EKGen`, `DKGen`, `TDKGen`, `Enc`, `Dec`, and `ReceiverVerify`.
- [ ] Verify state failures and run `Setup -> EKGen -> DKGen -> TDKGen -> Enc -> Dec -> ReceiverVerify`; require recovered message and `true` receiver verification.
- [ ] Strictly compile, run one SS512 experiment, run the style check, and commit `SchemeIBMEMR/SchemeIBMEMR.java`.

### Task 7: Convert SchemeIBMETR

**Files:**
- Create: `SchemeIBMETR/SchemeIBMETR.java`
- Reference: `SchemeIBMETR/SchemeIBMETR.py`

- [ ] Implement the parser/saver and typed records for encryption, decryption, and testing keys.
- [ ] Port product helper plus `Setup`, `EKGen`, `DKGen`, `TKGen`, `Enc`, `Dec`, and `TVerify` with exact identity order.
- [ ] Test invalid key/ciphertext shapes and run `Setup -> EKGen -> DKGen -> TKGen -> Enc -> Dec -> TVerify`; require message recovery and successful verification.
- [ ] Strictly compile, run one SS512 experiment, run the style check, and commit `SchemeIBMETR/SchemeIBMETR.java`.

### Task 8: Convert SchemeIBPRME

**Files:**
- Create: `SchemeIBPRME/SchemeIBPRME.java`
- Reference: `SchemeIBPRME/SchemeIBPRME.py`

- [ ] Implement the common shell and immutable records for original ciphertext, re-encryption key, and transformed ciphertext.
- [ ] Port `Setup`, `DKGen`, `EKGen`, `ReEKGen`, `Enc`, `ReEnc`, `Dec1`, and `Dec2` without reusing mutable elements across the two decryption paths.
- [ ] Test malformed re-encryption inputs, then run direct and proxy sequences; require `Dec1` and `Dec2` to recover the same original message.
- [ ] Strictly compile, run one SS512 experiment, run the style check, and commit `SchemeIBPRME/SchemeIBPRME.java`.

### Task 9: Convert SchemePBAC

**Files:**
- Create: `SchemeIBPRME/SchemePBAC.java`
- Reference: `SchemeIBPRME/SchemePBAC.py`

- [ ] Implement the common shell and PBAC-specific key/ciphertext records.
- [ ] Port `Setup`, `SKGen`, `RKGen`, `Enc`, `PKGen`, `ProxyEnc`, `Dec1`, and `Dec2`, retaining the Python byte identity encoding.
- [ ] Test invalid proxy-key and ciphertext shapes, then execute direct and proxy paths; require both decryptions to recover the generated message.
- [ ] Strictly compile, run one SS512 experiment, run the style check, and commit `SchemeIBPRME/SchemePBAC.java`.

### Task 10: Establish Exact Asymmetric Curve Availability

**Files:**
- Modify later within each asymmetric Java target; no shared production file.
- Inspect: `lib/jpbc-plaf-2.0.0.jar`

- [ ] Enumerate bundled JPBC pairing generators/resources and repository parameter files using `jar tf` and `rg --files`.
- [ ] For `MNT201`, `MNT224`, `BN254`, `SS512`, and `SS1024`, record whether an exact parameter source exists and whether it is symmetric or asymmetric.
- [ ] Define the same private `CurveParameter`/availability factory inside each asymmetric target: exact matches create a pairing; unavailable names return a structured unavailable result carrying the original name.
- [ ] Test every curve label; require supported labels to instantiate with the expected symmetry and unsupported labels to remain named but never instantiate a substitute pairing.

### Task 11: Convert SchemeAnonymousME

**Files:**
- Create: `SchemeHIBME/SchemeAnonymousME.java`
- Reference: `SchemeHIBME/SchemeAnonymousME.py`

- [ ] Implement the common shell and the asymmetric availability factory.
- [ ] Port product helper plus `Setup`, `KGen`, `DerivedKGen`, `Enc`, and `Dec`; keep hierarchy depth and parent-prefix checks explicit.
- [ ] Test direct and derived key generation at valid depths plus over-depth/mismatched-parent failures; require decryption recovery on supported curves.
- [ ] Strictly compile, run one experiment across the Python curve matrix, verify unavailable rows, run the style check, and commit `SchemeHIBME/SchemeAnonymousME.java`.

### Task 12: Convert SchemeHIBME

**Files:**
- Create: `SchemeHIBME/SchemeHIBME.java`
- Reference: `SchemeHIBME/SchemeHIBME.py`

- [ ] Implement the common shell, asymmetric availability, hierarchy records, and sender/receiver identities.
- [ ] Port product helper plus `Setup`, `EKGen`, `DerivedEKGen`, `DKGen`, `DerivedDKGen`, `Enc`, and `Dec` with prefix/depth validation.
- [ ] Exercise direct and derived encryption/decryption keys, invalid hierarchy shapes, and supported-curve message recovery.
- [ ] Strictly compile, run the Python curve matrix once, inspect unavailable rows, run the style check, and commit `SchemeHIBME/SchemeHIBME.java`.

### Task 13: Convert SchemeIBBME

**Files:**
- Create: `SchemeIBMEMR/SchemeIBBME.java`
- Reference: `SchemeIBMEMR/SchemeIBBME.py`

- [ ] Implement the common shell, asymmetric availability, ordered recipient-set representation, and polynomial records.
- [ ] Port coefficient/product/polynomial helpers plus `Setup`, `EKGen`, `DKGen`, `Enc`, and `Dec`.
- [ ] Test recipient membership, nonmember rejection, invalid set sizes, and successful supported-curve broadcast decryption.
- [ ] Strictly compile, run the Python curve matrix once, inspect unavailable rows, run the style check, and commit `SchemeIBMEMR/SchemeIBBME.java`.

### Task 14: Convert SchemeIBMECH

**Files:**
- Create: `SchemeIBMETR/SchemeIBMECH.java`
- Reference: `SchemeIBMETR/SchemeIBMECH.py`

- [ ] Implement the common shell and asymmetric availability.
- [ ] Port product helper, the Python `GaussEliminationinGroups` behavior as private Java matrix routines, then `Setup`, `SKGen`, `RKGen`, `Enc`, and `Dec`.
- [ ] Test singular/wrong-sized matrices and valid group-valued elimination before testing the full encryption/decryption sequence.
- [ ] Strictly compile, run the Python curve matrix once, inspect unavailable rows, run the style check, and commit `SchemeIBMETR/SchemeIBMECH.java`.

### Task 15: Convert SchemeIBPME

**Files:**
- Create: `SchemeIBPRME/SchemeIBPME.java`
- Reference: `SchemeIBPRME/SchemeIBPME.py`

- [ ] Implement the common shell, asymmetric availability, hashing helper, and direct/proxy ciphertext records.
- [ ] Port `Setup`, `SKGen`, `RKGen`, `PKGen`, `Enc`, `ProxyDec`, `Dec1`, and `Dec2`.
- [ ] Exercise malformed proxy inputs and both decryption routes; require direct and proxy recovery to match the original message on supported curves.
- [ ] Strictly compile, run the Python curve matrix once, inspect unavailable rows, run the style check, and commit `SchemeIBPRME/SchemeIBPME.java`.

### Task 16: Convert SchemeCANIFPPCT

**Files:**
- Create: `SchemeCANIFPPCT/SchemeCANIFPPCT.java`
- Reference: `SchemeCANIFPPCT/SchemeCANIFPPCT.py`

- [ ] Implement the common shell, asymmetric availability, tracing-list records, and polynomial helpers.
- [ ] Port `BSetup`, `BKGen`, `BEncryption`, `BTrapdoorGen`, `BQuery`, `Setup`, `KGen`, `Encryption`, `TrapdoorGen`, `Query`, and `Trace` in Python order.
- [ ] Test baseline and accountable query true/false cases plus tracing with valid and invalid lists on supported curves.
- [ ] Strictly compile, run the Python curve matrix once, inspect unavailable rows, run the style check, and commit `SchemeCANIFPPCT/SchemeCANIFPPCT.java`.

### Task 17: Convert SchemeVLPSICA

**Files:**
- Create: `SchemeVLPSICA/SchemeVLPSICA.java`
- Reference: `SchemeVLPSICA/SchemeVLPSICA.py`

- [ ] Implement the common shell, asymmetric availability, vector records, and deterministic vector-length checks.
- [ ] Port product/Lagrange helpers plus `Setup`, `Sender`, `Receiver`, `Cloud1`, `Cloud2`, and `Verify`.
- [ ] Test wrong dimensions, empty intersections, known intersections, and supported-curve verification/cardinality results.
- [ ] Strictly compile, run the Python curve matrix once, inspect unavailable rows, run the style check, and commit `SchemeVLPSICA/SchemeVLPSICA.java`.

### Task 18: Expand Java CI Coverage

**Files:**
- Modify: `.github/workflows/runJava.yml`

- [ ] Add one boolean `workflow_dispatch` input per Java scheme, retaining `executeSchemeAAIBME` and using names `executeSchemeAIBE` through `executeSchemeVLPSICA`.
- [ ] Populate the matrix with all 16 exact Java paths when their switches are true; on push and pull request events default every switch to true.
- [ ] Preserve sparse checkout, dependency fetching, source-relative output filename use, artifact upload, Java-version matrix behavior, and alphabetically order scheme paths.
- [ ] Validate YAML parsing and locally reproduce the matrix list; require 16 unique entries.
- [ ] Commit only `.github/workflows/runJava.yml`.

### Task 19: Update README Java Documentation

**Files:**
- Modify: `README.md`

- [ ] Add Java links beside all 16 existing Python scheme entries without removing Python or SageMath links.
- [ ] Generalize the Java run example from only `SchemeAAIBME.java` to a shell-variable example using `schemePath="SchemeAAIBME/SchemeAAIBME.java"`, while retaining the pure-shell dependency command.
- [ ] Document source-directory-relative output paths, integral spreadsheet cells, JDK 17+, exact curve availability, and explicit unavailable rows.
- [ ] Check every new relative link with a filesystem existence loop and commit only `README.md`.

### Task 20: Repository-Wide Verification

**Files:**
- Verify: all 16 `Scheme*.java` files, `.github/workflows/runJava.yml`, `README.md`

- [ ] Fetch dependencies once and strictly compile every Java source into `/tmp/cryptographic-schemes-java-classes`; expect 16 successes and no repository `.class` files.
- [ ] Run every scheme with `-r 1` and an absolute `/tmp` XLSX output; require each process to exit successfully and each workbook to be nonempty.
- [ ] Run selected schemes from a different working directory with relative `-o` values; require outputs under their Java source directories, then remove only those generated test outputs.
- [ ] Compare every Java console label against string literals in its Python source and correct all differences.
- [ ] Open every generated workbook programmatically with POI, require readable sheets/headers, and require integral metrics to be numeric integral cells.
- [ ] Verify all Java files use tabs for indentation, sorted import groups, Allman braces, initialized declarations, English comments, and no newline byte at EOF.
- [ ] Run `git diff --check`, verify `.DS_Store` is untracked and unstaged, and review the complete diff against the design spec.
- [ ] Commit remaining verification-only corrections in scheme-specific commits; do not push or open a Pull Request.
