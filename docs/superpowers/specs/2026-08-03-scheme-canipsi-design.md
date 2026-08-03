# SchemeCANIPSI Design

## Goal

Implement the CA-NI-PSI baseline as standalone Java and Python programs at `SchemeCANIFPPCT/SchemeCANIPSI.java` and `SchemeCANIFPPCT/SchemeCANIPSI.py`, then add both programs to their language workflows.

## Sources of Truth

The attached `CA-NI-PSI.zip` defines the baseline scheme boundary and its B-prefixed and non-B procedures. `SchemeCANIFPPCT/SchemeCANIFPPCT.java` and `SchemeCANIFPPCT/SchemeCANIFPPCT.py` define the repository's single-file organization, command-line behavior, output handling, experiment structure, defensive validation, and normalized pairing operations.

The implementation must preserve the attachment's intended algorithms without copying known experimental defects. In particular, it must not treat exceptions as successful queries, hash Java array object identities, rely on invalid mixed-group operations, or retain unused external AES, JNA, ClassMexer, and JUnit dependencies.

## File Architecture

`SchemeCANIFPPCT/SchemeCANIPSI.java` is one source file containing the package-private `Parser` and `Saver` helpers and the public `SchemeCANIPSI` class. The public class contains its private state and helpers, public cryptographic procedures, result records, `conductScheme`, and `main`, ordered and formatted consistently with the existing Java scheme files.

`SchemeCANIFPPCT/SchemeCANIPSI.py` is one source file containing imports, `Parser`, `Saver`, `SchemeCANIPSI`, `conductScheme`, and `main`. Except for scheme-specific help text and required libraries, the code before `conductScheme` follows the dominant structure and style of existing `Scheme*/Scheme*.py` files.

Neither implementation shares source files with `SchemeCANIFPPCT`; each remains independently executable.

## Cryptographic API

The basic flow exposes these procedures:

- `BSetup(n, m)`
- `BKGen(identity)`
- `BEncryption(keyword, secrets, selectedSecret)`
- `BTokenGen(queryKeyword, basicUserKey)`
- `BQuery(cipherText, token)`

The complete CA-NI-PSI flow exposes these procedures:

- `Setup(n, m)`
- `KGen(identity, tracingList)`
- `Encryption(keyword, userSecretKey, encryptionKey, secrets, selectedSecret)`
- `TokenGen(queryKeyword, userSecretKey)`
- `Query(cipherText, token, secrets)`
- `Trace(cipherText, tracingList)`

The `TokenGen` names follow the attachment. Input validation and safe fallback behavior follow the existing scheme conventions, but invalid ciphertexts, tokens, and queries must never become successful merely because an exception occurred.

The B flow runs only on symmetric pairings. The complete flow supports the symmetric and asymmetric curve families already exercised by `SchemeCANIFPPCT`. Java uses only the repository's existing JPBC dependencies, while Python uses Charm-Crypto.

## Experiment Driver and Results

`conductScheme` validates the curve, `n`, `m`, and run identifier; constructs the pairing; executes both supported flows; times each public procedure; records serialized sizes; and returns one flat result row.

The result identifies the curve, security parameter, `n`, `m`, and run count. Validators report system validity, B-flow correctness when applicable, complete-flow correctness, and tracing verification. Metrics report procedure times, element sizes, public and secret key sizes, ciphertext sizes, and token sizes.

The default matrix matches `SchemeCANIFPPCT`: MNT201, MNT224, BN254, three SS512 security-parameter entries, and two SS1024 entries; `n` ranges from 10 through 30 in increments of 5; `m` ranges from 5 to less than `n` in increments of 5; and the command-line run count controls repetitions. Expected unavailable JPBC curves are represented consistently rather than failing the whole Java program.

The command-line parser, script-relative output resolution, supported formats, overwrite handling, averaging, integer formatting, exit status, and console messages follow the corresponding existing language implementation.

## Dynamic Correctness Requirements

Tests must demonstrate behavior rather than only compilation:

- A matching keyword returns `true` in each supported B and complete flow.
- A different keyword returns `false`.
- `Trace` returns the registered identity entry for a valid complete-flow ciphertext.
- Malformed inputs do not become successful through exception handling.
- Java and Python `conductScheme` return valid rows with positive timing and size metrics on a supported small symmetric case.
- Help and noninteractive output-path execution work for both languages.

Java must compile with the dependency classpath used by `runJava.yml`. Python must pass syntax compilation and run with the dependencies used by `runPython.yml`. Java source formatting must retain the repository's tab indentation, Allman braces, sorted imports, English comments, and an ending at the final closing brace without a trailing line-feed character.

## Workflow Integration

`.github/workflows/runJava.yml` adds `SchemeCANIFPPCT/SchemeCANIPSI.java` next to `SchemeCANIFPPCT/SchemeCANIFPPCT.java` in the existing `executeSchemeCANIFPPCT` matrix branch.

`.github/workflows/runPython.yml` adds `SchemeCANIFPPCT/SchemeCANIPSI.py` at the corresponding position. No new workflow inputs or dependencies are introduced.

## Scope Exclusions

This change does not refactor existing scheme files, add shared runtime modules, introduce the attachment's obsolete JAR files, reproduce its multithreaded benchmark harness, or modify README documentation unless implementation reveals an existing description that becomes factually incorrect.
