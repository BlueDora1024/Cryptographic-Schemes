# PSI-CA C++ Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the seven supplied PSI-CA C++ programs, give every program a Python/Java-style command-line parser and result saver, run every program in GitHub Actions, and document C++ as README section 3.

**Architecture:** Keep the seven algorithms as independent translation units under `PSI-CA/`. Put the shared standard-library-only `Parser`, `Saver`, option/result value types, and exit-wait helper in `PSI-CA/ParserSaver.hpp`, then include that header from every program. Save CSV, TSV, or TXT without third-party libraries and resolve relative output paths against the executable directory.

**Tech Stack:** C++17 standard library, GNU g++, Bash, GitHub Actions YAML, Markdown.

---

### Task 1: Import the seven supplied programs

**Files:**
- Create: `PSI-CA/OPSI-CA.cpp`
- Create: `PSI-CA/PSI-CA.cpp`
- Create: `PSI-CA/SPSI-CA(1).cpp`
- Create: `PSI-CA/VPSI-CA-Alg2.cpp`
- Create: `PSI-CA/VPSI-CA-Alg3.cpp`
- Create: `PSI-CA/VPSI-CA-Alg4.cpp`
- Create: `PSI-CA/VPSI-CA-Alg5.cpp`
- Create: `tests/cpp/runCPPRegressionTest.sh`

- [ ] **Step 1: Write the failing source-inventory test**

Create a shell test that declares the exact seven expected file names, checks that each exists under `PSI-CA/`, and fails with the missing path.

- [ ] **Step 2: Run the inventory test and verify RED**

Run: `bash tests/cpp/runCPPRegressionTest.sh`

Expected: FAIL because `PSI-CA/OPSI-CA.cpp` does not exist.

- [ ] **Step 3: Copy the supplied files without changing their algorithms**

Copy the exact files from `/Users/felix_project/yuers/cpp/` into `PSI-CA/`, preserving all seven names.

- [ ] **Step 4: Run the inventory test and verify GREEN**

Run: `bash tests/cpp/runCPPRegressionTest.sh`

Expected: PASS for the inventory section.

### Task 2: Implement the shared Parser and Saver

**Files:**
- Create: `PSI-CA/ParserSaver.hpp`
- Create: `tests/cpp/ParserSaverTest.cpp`
- Modify: `tests/cpp/runCPPRegressionTest.sh`

- [ ] **Step 1: Write failing Parser/Saver contract tests**

The C++ test must instantiate `Parser` with mixed-case aliases, verify `-o`, `-p`, `-q`, `-r`, `-t`, and `-y`, confirm relative output paths use the executable directory, reject protected output extensions, and verify CSV quoting plus decimal formatting from `Saver`.

- [ ] **Step 2: Run the contract test and verify RED**

Run: `g++ -std=c++17 -Wall -Wextra -Wpedantic tests/cpp/ParserSaverTest.cpp -o /tmp/ParserSaverTest`

Expected: FAIL because `PSI-CA/ParserSaver.hpp` does not exist.

- [ ] **Step 3: Implement the minimal standard-library header**

Define `ParserOptions`, `Parser`, `SaverValue`, `Saver`, and `finishExecution`. Match the Python/Java aliases for encoding, help, output, decimal place, quiet mode, run count, waiting time, and overwrite confirmation. Limit encoding to UTF-8, support CSV/TSV/TXT, create parent directories, and never overwrite an existing file without `-y` or interactive confirmation.

- [ ] **Step 4: Run the contract test and verify GREEN**

Run: `g++ -std=c++17 -Wall -Wextra -Wpedantic tests/cpp/ParserSaverTest.cpp -o /tmp/ParserSaverTest && /tmp/ParserSaverTest`

Expected: exit 0 with no diagnostics.

### Task 3: Connect every algorithm to Parser and Saver

**Files:**
- Modify: all seven `PSI-CA/*.cpp` files
- Modify: `tests/cpp/runCPPRegressionTest.sh`

- [ ] **Step 1: Extend the regression test and verify RED**

For every source, require `#include "ParserSaver.hpp"`, `int main(int argc, char* argv[])`, a `Parser` instance, a `Saver` instance, and use of parsed run count instead of `TimeToTest` in execution and average-result calculations.

- [ ] **Step 2: Add minimal per-program integration**

Parse options at the start of `main`, support help/invalid-argument exits, honor quiet mode, execute the requested run count, save one summary row with scheme parameters, elapsed CPU milliseconds, actor timers, and space size, and call `finishExecution` for `-t` behavior. Replace the four legacy `dump` functions with `Saver` calls.

- [ ] **Step 3: Compile all sources with the CI flags**

Run: `find PSI-CA -maxdepth 1 -type f -name '*.cpp' -print0 | while IFS= read -r -d '' source; do g++ -std=c++17 -Wall -Wextra -Wpedantic "$source" -o "/tmp/$(basename "${source%.cpp}")"; done`

Expected: seven successful compilations. Existing algorithm warnings may remain because the requested flags do not include `-Werror`.

- [ ] **Step 4: Execute all sources through the regression test**

Run every binary with `-q -r 1 -t 0 -y -o <temporary CSV>`, require exit 0, require a non-empty result file, and validate a CSV header and data row.

Expected: seven successful executions and seven result files.

### Task 4: Add the C++ GitHub Actions workflow

**Files:**
- Create: `.github/workflows/runCPP.yml`
- Modify: `tests/cpp/runCPPRegressionTest.sh`

- [ ] **Step 1: Add failing workflow assertions**

Require pull-request, push, and workflow-dispatch triggers; an exact seven-file matrix; compilation containing `g++ -std=c++17 -Wall -Wextra -Wpedantic`; execution with parser options; and artifact upload.

- [ ] **Step 2: Run the assertions and verify RED**

Run: `bash tests/cpp/runCPPRegressionTest.sh`

Expected: FAIL because `.github/workflows/runCPP.yml` does not exist.

- [ ] **Step 3: Implement `runCPP.yml`**

Model its inputs and preparation steps after `runJava.yml` and `runPython.yml`. Use an exact matrix of seven source paths, compile each source to a temporary binary with the required warning flags, run help, execute with output/precision/run/time/overwrite arguments, and upload each CSV output.

- [ ] **Step 4: Validate workflow syntax and assertions**

Parse the YAML with the available Python YAML library when present and rerun the shell regression test.

Expected: workflow assertions pass and YAML parsing succeeds.

### Task 5: Document C++ as README section 3

**Files:**
- Modify: `README.md`
- Modify: `tests/cpp/runCPPRegressionTest.sh`

- [ ] **Step 1: Add failing README assertions**

Require `## 3. C++`, environment/build instructions, Parser arguments, CSV/TSV/TXT Saver formats, exit codes, and a `runCPP.yml` link; require Acknowledgment to become section 4.

- [ ] **Step 2: Run assertions and verify RED**

Run: `bash tests/cpp/runCPPRegressionTest.sh`

Expected: FAIL because section 3 is currently Acknowledgment.

- [ ] **Step 3: Update README**

Add the seven C++ files to the repository inventory, insert section 3 after Java, explain Ubuntu/g++ requirements, give build/run examples with quoted paths, document the standard-library-only output formats and command-line defaults, link the workflow, explain timing/space output, and renumber Acknowledgment to section 4.

- [ ] **Step 4: Run README assertions and inspect the diff**

Run: `bash tests/cpp/runCPPRegressionTest.sh && git diff --check`

Expected: PASS and no whitespace errors.

### Task 6: Final verification

**Files:**
- Verify all changed files

- [ ] **Step 1: Run the complete regression test from a clean temporary output directory**

Run: `bash tests/cpp/runCPPRegressionTest.sh`

Expected: PASS for all seven files.

- [ ] **Step 2: Inspect repository state and diff**

Run: `git status --short && git diff --stat && git diff --check`

Expected: only intended PSI-CA, workflow, README, tests, and plan changes; no generated binaries or result files.

- [ ] **Step 3: Commit the completed change**

Run: `git add .github/workflows/runCPP.yml README.md PSI-CA tests/cpp docs/superpowers/plans/2026-08-03-psi-ca-cpp-integration.md && git commit -m "Add PSI-CA C++ workflow support"`

Expected: one English commit on `codex/add-psi-ca-cpp`.
