# SchemeCANIPSI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add independently executable, single-file Java and Python implementations of the CA-NI-PSI baseline and execute both from the existing language workflows.

**Architecture:** Adapt the validated `SchemeCANIFPPCT` runtime structure into a separate `SchemeCANIPSI` program while retaining the attachment's B-prefixed and complete CA-NI-PSI procedure names. Behavioral regression tests exercise matching and mismatching queries, tracing, `conductScheme`, command-line parsing, and workflow registration before completion is claimed.

**Tech Stack:** Java 25 source-file execution, JPBC 2.0.0, Apache POI 5.5.1, Python 3, Charm-Crypto, unittest, Bash, GitHub Actions YAML

---

## File Map

- Create `SchemeCANIFPPCT/SchemeCANIPSI.java`: standalone Java parser, saver, CA-NI-PSI implementation, experiment driver, and main entry point.
- Create `SchemeCANIFPPCT/SchemeCANIPSI.py`: standalone Python parser, saver, CA-NI-PSI implementation, experiment driver, and main entry point.
- Create `tests/java/SchemeCANIPSITest.java`: Java behavioral contract for both flows and the experiment driver.
- Create `tests/python/test_scheme_canipsi.py`: Python behavioral contract for both flows and the experiment driver.
- Create `tests/scheme-canipsi/runSchemeCANIPSIRegressionTest.sh`: repeatable compilation, syntax, CLI, formatting, and workflow-registration checks.
- Modify `.github/workflows/runJava.yml`: add the Java baseline beside `SchemeCANIFPPCT.java`.
- Modify `.github/workflows/runPython.yml`: add the Python baseline beside `SchemeCANIFPPCT.py`.

### Task 1: Add the Java behavior contract

**Files:**
- Create: `tests/java/SchemeCANIPSITest.java`
- Create: `tests/scheme-canipsi/runSchemeCANIPSIRegressionTest.sh`

- [ ] **Step 1: Write the Java test before the implementation exists**

Create a default-package test with a `require(boolean, String)` helper. Its `main` must construct `new SchemeCANIPSI(new SchemeCANIPSI.CurveParameter("SS512", 128))` and execute these assertions:

```java
final SchemeCANIPSI scheme = new SchemeCANIPSI(new SchemeCANIPSI.CurveParameter("SS512", 128));
final byte[] keyword = "shared-keyword".getBytes(StandardCharsets.UTF_8);
final byte[] otherKeyword = "different-keyword".getBytes(StandardCharsets.UTF_8);
final List<Element> secrets = scheme.randomSecrets(3);
final Element identity = scheme.randomSecrets(1).get(0);

scheme.BSetup(3, 1);
final Element basicUserKey = scheme.BKGen(identity);
final SchemeCANIPSI.BasicCipherText basicCipherText = scheme.BEncryption(keyword, secrets, secrets.get(0));
require(scheme.BQuery(basicCipherText, scheme.BTokenGen(keyword, basicUserKey)), "The matching basic query must succeed.");
require(!scheme.BQuery(basicCipherText, scheme.BTokenGen(otherKeyword, basicUserKey)), "The mismatching basic query must fail.");

final List<SchemeCANIPSI.TraceEntry> tracingList = new ArrayList<>();
scheme.Setup(3, 1);
final SchemeCANIPSI.UserKeys userKeys = scheme.KGen(identity, tracingList);
final SchemeCANIPSI.CipherText cipherText = scheme.Encryption(keyword, userKeys.secretKey(), userKeys.encryptionKey(), secrets, secrets.get(0));
require(scheme.Query(cipherText, scheme.TokenGen(keyword, userKeys.secretKey()), secrets), "The matching complete query must succeed.");
require(!scheme.Query(cipherText, scheme.TokenGen(otherKeyword, userKeys.secretKey()), secrets), "The mismatching complete query must fail.");
final SchemeCANIPSI.TraceEntry traced = scheme.Trace(cipherText, tracingList);
require(traced != null && identity.isEqual((Element)traced.identity()), "Trace must recover the registered identity.");

final SchemeCANIPSI.RunResult result = SchemeCANIPSI.conductScheme(new SchemeCANIPSI.CurveParameter("SS512", 128), 3, 1, Integer.valueOf(1), false);
require(result.systemValid(), "conductScheme must create a supported system.");
require(Boolean.TRUE.equals(result.basicSchemeCorrect()), "conductScheme must validate the basic flow.");
require(result.schemeCorrect(), "conductScheme must validate the complete flow.");
require(result.tracingVerified(), "conductScheme must validate tracing.");
```

The test must print `SchemeCANIPSI Java tests passed.` only after every assertion succeeds.

- [ ] **Step 2: Add a regression runner that discovers the current dependency classpath**

The shell runner must use `set -euo pipefail`, create a private temporary directory with `mktemp -d`, remove it through `trap`, and build the classpath from sorted `lib/*.jar` paths without modifying `lib`:

```bash
dependencies="$(find lib -maxdepth 1 -type f -name '*.jar' -print | LC_ALL=C sort | paste -sd: -)"
test -n "${dependencies}"
javac -Xlint:all -cp "${dependencies}" -d "${temporaryDirectory}" \
	SchemeCANIFPPCT/SchemeCANIPSI.java tests/java/SchemeCANIPSITest.java
java -cp "${temporaryDirectory}:${dependencies}" SchemeCANIPSITest
```

The same runner will later hold Python, CLI, formatting, and workflow checks.

- [ ] **Step 3: Run the test and verify the red state**

Run:

```bash
bash tests/scheme-canipsi/runSchemeCANIPSIRegressionTest.sh
```

Expected: failure because `SchemeCANIFPPCT/SchemeCANIPSI.java` does not exist.

- [ ] **Step 4: Commit the failing behavioral contract**

```bash
git add tests/java/SchemeCANIPSITest.java tests/scheme-canipsi/runSchemeCANIPSIRegressionTest.sh
git commit -m "Add SchemeCANIPSI Java behavior contract"
```

### Task 2: Implement the standalone Java scheme

**Files:**
- Create: `SchemeCANIFPPCT/SchemeCANIPSI.java`

- [ ] **Step 1: Establish the single-file runtime structure**

Copy the Java parser and saver behavior from `SchemeCANIFPPCT.java`, then make these scheme-specific replacements:

```text
Parser.SCHEME_NAME = "SchemeCANIPSI"
public class name = SchemeCANIPSI
default output name = SchemeCANIPSI.xlsx
help description = CA-NI-PSI baseline implementation
```

Keep standard-library imports before external imports, sort each group alphabetically, retain tab indentation and Allman braces, and initialize every field at declaration or construction.

- [ ] **Step 2: Add pairing and value helpers**

Implement the same supported curve mapping and cached pairing creation used by `SchemeCANIFPPCT.java`:

```java
MNT201, MNT224 -> CurveUnavailableException
BN254 -> TypeFCurveGenerator(254)
SS512 -> TypeACurveGenerator(160, 512, false)
SS1024 -> TypeACurveGenerator(160, 1024, false)
```

Add immutable arithmetic, hashing, polynomial coefficient generation, Horner-style polynomial evaluation, serialization-length calculation, metric averaging, and expected-unavailable-curve helpers. `PairingFactory.getInstance().setUsePBCWhenPossible(false)` remains explicit because only the repository's JPBC API and platform JARs are available.

- [ ] **Step 3: Implement the B-prefixed procedures**

Expose these exact signatures and immutable records:

```java
public BasicSetupResult BSetup(int requestedN, int requestedM)
public Element BKGen(Object identity)
public BasicCipherText BEncryption(byte[] keyword, List<Element> sourceSecrets, Element selectedSecret)
public Token BTokenGen(byte[] queryKeyword, Element basicUserKey)
public boolean BQuery(BasicCipherText cipherText, Token token)
```

Use the normalized equations from the B-prefixed portion of `SchemeCANIFPPCT`: ciphertext generation binds `keyword` through `H1`, token generation binds `queryKeyword`, and `BQuery` hashes the product of five pairings and evaluates the ciphertext polynomial at that value. Return `false` for null, malformed, wrong-field, or mismatching values; do not catch `Throwable` and return success.

- [ ] **Step 4: Implement the complete procedures**

Expose these exact signatures and records:

```java
public SetupResult Setup(int requestedN, int requestedM)
public UserKeys KGen(Object identity, List<TraceEntry> tracingList)
public CipherText Encryption(byte[] keyword, Element userSecretKey, EncryptionKey encryptionKey, List<Element> sourceSecrets, Element selectedSecret)
public Token TokenGen(byte[] queryKeyword, Element userSecretKey)
public boolean Query(CipherText cipherText, Token token, List<Element> sourceSecrets)
public TraceEntry Trace(CipherText cipherText, List<TraceEntry> tracingList)
```

`KGen` appends `(identity, secretKey, tag)` to the supplied tracing list. `Encryption` creates the five searchable components and five tracing/proof components. `Query` reconstructs the membership polynomial from `sourceSecrets` and rejects a mismatching token. `Trace` recomputes the ciphertext tag and returns the matching list entry or `null`.

- [ ] **Step 5: Implement `conductScheme` and `main`**

The result row and output columns must use these validators and metrics in this order:

```text
curveParameter, secparam, n, m, runCount,
isSystemValid, isBSchemeCorrect, isSchemeCorrect, isTracingVerified,
BSetup (s), BKGen (s), BEncryption (s), BTokenGen (s), BQuery (s),
Setup (s), KGen (s), Encryption (s), TokenGen (s), Query (s), Trace (s),
elementOfZR (B), elementOfG1 (B), elementOfG2 (B), elementOfGT (B),
bpk (B), bsk (B), bsk_IDs (B), BCT_TPs (B), BTokens (B),
mpk (B), msk (B), sk_IDs (B), ek_IDs (B), CT_TPs (B), Tokens (B)
```

Use the six curve entries currently verified in `SchemeCANIFPPCT.java` (BN254, three SS512 entries, and two SS1024 entries), `n = 10..30` by 5, `m = 5..<n` by 5, and the parsed run count. Save after each averaged parameter group so interrupted experiments retain results.

- [ ] **Step 6: Run the Java contract until green**

Run:

```bash
bash tests/scheme-canipsi/runSchemeCANIPSIRegressionTest.sh
```

Expected: `SchemeCANIPSI Java tests passed.` and exit code 0.

- [ ] **Step 7: Enforce the Java source style**

Run checks for spaces used as leading indentation, carriage returns, trailing whitespace, unsorted import groups, and the final byte:

```bash
test -z "$(sed -n '/^  /=' SchemeCANIFPPCT/SchemeCANIPSI.java)"
! LC_ALL=C grep -n $'\r' SchemeCANIFPPCT/SchemeCANIPSI.java
! grep -n '[[:blank:]]$' SchemeCANIFPPCT/SchemeCANIPSI.java
test "$(tail -c 1 SchemeCANIFPPCT/SchemeCANIPSI.java | od -An -tuC | tr -d ' ')" = "125"
```

Expected: every command succeeds; decimal 125 is the final `}` byte.

- [ ] **Step 8: Commit the Java implementation**

```bash
git add SchemeCANIFPPCT/SchemeCANIPSI.java
git commit -m "Implement CA-NI-PSI baseline in Java"
```

### Task 3: Add the Python behavior contract

**Files:**
- Create: `tests/python/test_scheme_canipsi.py`
- Modify: `tests/scheme-canipsi/runSchemeCANIPSIRegressionTest.sh`

- [ ] **Step 1: Write the Python test before the Python implementation exists**

Load the target by file path through `importlib.util.spec_from_file_location`. In a `unittest.TestCase`, create `PairingGroup("SS512", secparam=128)` and assert the same matching, mismatching, tracing, and `conductScheme` behavior as the Java test:

```python
scheme = module.SchemeCANIPSI(group)
keyword = b"shared-keyword"
other_keyword = b"different-keyword"
secrets = tuple(group.random(ZR) for _ in range(3))
identity = group.random(ZR)

scheme.BSetup(3, 1)
basic_user_key = scheme.BKGen(identity)
basic_ciphertext = scheme.BEncryption(keyword, secrets, secrets[0])
self.assertTrue(scheme.BQuery(basic_ciphertext, scheme.BTokenGen(keyword, basic_user_key)))
self.assertFalse(scheme.BQuery(basic_ciphertext, scheme.BTokenGen(other_keyword, basic_user_key)))

tracing_list = []
scheme.Setup(3, 1)
secret_key, encryption_key = scheme.KGen(identity, tracing_list)
ciphertext = scheme.Encryption(keyword, secret_key, encryption_key, secrets, secrets[0])
self.assertTrue(scheme.Query(ciphertext, scheme.TokenGen(keyword, secret_key), secrets))
self.assertFalse(scheme.Query(ciphertext, scheme.TokenGen(other_keyword, secret_key), secrets))
self.assertEqual(identity, scheme.Trace(ciphertext, tracing_list)[0])

result = module.conductScheme(("SS512", 128), n=3, m=1, run=1, isVerbose=False)
self.assertTrue(result[5])
self.assertTrue(result[6])
self.assertTrue(result[7])
self.assertTrue(result[8])
```

- [ ] **Step 2: Add Python checks to the regression runner**

Append:

```bash
python -m py_compile SchemeCANIFPPCT/SchemeCANIPSI.py
python -m unittest tests/python/test_scheme_canipsi.py
```

- [ ] **Step 3: Run the Python test and verify the red state**

Run:

```bash
python -m unittest tests/python/test_scheme_canipsi.py
```

Expected: failure because `SchemeCANIFPPCT/SchemeCANIPSI.py` does not exist.

- [ ] **Step 4: Commit the failing Python contract**

```bash
git add tests/python/test_scheme_canipsi.py tests/scheme-canipsi/runSchemeCANIPSIRegressionTest.sh
git commit -m "Add SchemeCANIPSI Python behavior contract"
```

### Task 4: Implement the standalone Python scheme

**Files:**
- Create: `SchemeCANIFPPCT/SchemeCANIPSI.py`

- [ ] **Step 1: Establish the Python runtime structure**

Adapt `SchemeCANIFPPCT.py` into a standalone module and replace the scheme identity consistently:

```text
Parser.__SchemeName = "SchemeCANIPSI"
class name = SchemeCANIPSI
default output name = SchemeCANIPSI.xlsx
help description = CA-NI-PSI baseline implementation
```

Preserve the standard import order and the existing `Parser` and `Saver` behavior. The code preceding `conductScheme` must retain the repository's dominant sequence: imports, environment fallback, constants, `Parser`, `Saver`, and scheme class.

- [ ] **Step 2: Translate the verified Java API and equations**

Provide the same public procedure names as Java:

```python
BSetup, BKGen, BEncryption, BTokenGen, BQuery
Setup, KGen, Encryption, TokenGen, Query, Trace
```

Use Charm `Element` type checks and `pair`, preserve B-flow symmetric-group gating, and translate Java's normalized polynomial and hash inputs byte-for-byte at the logical data level. Return `False` for malformed or mismatching queries and never convert an exception into a successful validator.

- [ ] **Step 3: Translate `conductScheme` and `main`**

Return the same 35-column row order specified for Java. Keep the existing Python averaging behavior, numeric integer normalization, partial-result saving, output messages, exit-code calculation, the eight-entry Python curve matrix including MNT201 and MNT224, and the `n/m` loops.

- [ ] **Step 4: Run Python syntax and behavior tests until green**

Run:

```bash
python -m py_compile SchemeCANIFPPCT/SchemeCANIPSI.py
python -m unittest tests/python/test_scheme_canipsi.py
```

Expected: one passing test module and exit code 0.

- [ ] **Step 5: Commit the Python implementation**

```bash
git add SchemeCANIFPPCT/SchemeCANIPSI.py
git commit -m "Implement CA-NI-PSI baseline in Python"
```

### Task 5: Register both implementations in GitHub Actions

**Files:**
- Modify: `.github/workflows/runJava.yml`
- Modify: `.github/workflows/runPython.yml`
- Modify: `tests/scheme-canipsi/runSchemeCANIPSIRegressionTest.sh`

- [ ] **Step 1: Add failing workflow assertions**

Append exact-count checks to the regression runner:

```bash
test "$(rg -o 'SchemeCANIFPPCT/SchemeCANIPSI\.java' .github/workflows/runJava.yml | wc -l | tr -d ' ')" = "1"
test "$(rg -o 'SchemeCANIFPPCT/SchemeCANIPSI\.py' .github/workflows/runPython.yml | wc -l | tr -d ' ')" = "1"
```

- [ ] **Step 2: Run the assertions and verify the red state**

Run the regression script and confirm it fails at the new exact-count checks because neither workflow contains the baseline.

- [ ] **Step 3: Update the two matrices**

Use these exact matrix fragments:

```bash
matrix+=("SchemeCANIFPPCT/SchemeCANIFPPCT.java" "SchemeCANIFPPCT/SchemeCANIPSI.java")
matrix+=("SchemeCANIFPPCT/SchemeCANIFPPCT.py" "SchemeCANIFPPCT/SchemeCANIPSI.py")
```

Do not add inputs, dependencies, or separate jobs.

- [ ] **Step 4: Run the complete regression script**

Run:

```bash
bash tests/scheme-canipsi/runSchemeCANIPSIRegressionTest.sh
```

Expected: Java behavior, Python behavior, CLI, formatting, and workflow assertions all pass.

- [ ] **Step 5: Commit the workflow integration**

```bash
git add .github/workflows/runJava.yml .github/workflows/runPython.yml tests/scheme-canipsi/runSchemeCANIPSIRegressionTest.sh
git commit -m "Run SchemeCANIPSI in Java and Python workflows"
```

### Task 6: Perform final static and dynamic verification

**Files:**
- Verify all files changed by Tasks 1 through 5

- [ ] **Step 1: Run repository-level whitespace and diff checks**

```bash
git diff origin/main...HEAD --check
git status --short
```

Expected: no whitespace errors; only intentional files are present.

- [ ] **Step 2: Run the standalone Java CLI help path**

```bash
dependencies="$(find lib -maxdepth 1 -type f -name '*.jar' -print | LC_ALL=C sort | paste -sd: -)"
java -cp "${dependencies}" SchemeCANIFPPCT/SchemeCANIPSI.java -t 0 -h
```

Expected: help names `SchemeCANIPSI` and exits 0.

- [ ] **Step 3: Run the standalone Python CLI help path**

```bash
python SchemeCANIFPPCT/SchemeCANIPSI.py -t 0 -h
```

Expected: help names `SchemeCANIPSI` and exits 0.

- [ ] **Step 4: Run the complete regression suite once more**

```bash
bash tests/scheme-canipsi/runSchemeCANIPSIRegressionTest.sh
```

Expected: exit code 0 with both language success messages.

- [ ] **Step 5: Review the final diff for scope and source ordering**

```bash
git diff --stat origin/main...HEAD
git diff -- .github/workflows/runJava.yml .github/workflows/runPython.yml
git log --oneline origin/main..HEAD
```

Confirm the diff contains exactly two implementation files, three test files, two workflow edits, the approved design, and this plan. Confirm the Java source ends at `}` without a trailing newline.
