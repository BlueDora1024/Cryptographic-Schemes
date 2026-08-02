#!/usr/bin/env bash

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
psi_ca_dir="$repo_root/PSI-CA"
expected_files=(
  "OPSI-CA.cpp"
  "PSI-CA.cpp"
  "SPSI-CA(1).cpp"
  "VPSI-CA-Alg2.cpp"
  "VPSI-CA-Alg3.cpp"
  "VPSI-CA-Alg4.cpp"
  "VPSI-CA-Alg5.cpp"
)
vpsi_files=(
  "VPSI-CA-Alg2.cpp"
  "VPSI-CA-Alg3.cpp"
  "VPSI-CA-Alg4.cpp"
  "VPSI-CA-Alg5.cpp"
)

for filename in "${expected_files[@]}"; do
  if [[ ! -f "$psi_ca_dir/$filename" || -L "$psi_ca_dir/$filename" ]]; then
    printf 'Missing: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi
done

while IFS= read -r -d '' path; do
  filename="${path#"$psi_ca_dir"/}"
  if [[ "$filename" == "ParserSaver.hpp" ]]; then
    continue
  fi
  is_expected=false
  for expected_filename in "${expected_files[@]}"; do
    if [[ "$filename" == "$expected_filename" ]]; then
      is_expected=true
      break
    fi
  done

  if [[ "$is_expected" == false ]]; then
    printf 'Unexpected: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi
done < <(find "$psi_ca_dir" -mindepth 1 ! -type d -print0)

printf 'PASS: PSI-CA contains exactly the seven expected C++ source files.\n'

parser_saver_header="$psi_ca_dir/ParserSaver.hpp"
if grep -Fq 'remove_all' "$parser_saver_header"; then
  printf 'ParserSaver.hpp must not recursively delete temporary paths.\n' >&2
  exit 1
fi
for required_api in mkdirat openat fstatat linkat renameat unlinkat; do
  if ! grep -Fq "$required_api" "$parser_saver_header"; then
    printf 'ParserSaver.hpp is missing required POSIX FD API: %s\n' "$required_api" >&2
    exit 1
  fi
done

for filename in "${vpsi_files[@]}"; do
  source_path="$psi_ca_dir/$filename"
  if grep -Fq '>> 2' "$source_path"; then
    printf 'Incorrect VPSI aggregate space conversion remains: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi
  if ! grep -Eq 'double[[:space:]]+spaceKilobytes[[:space:]]*=[[:space:]]*0\.0' "$source_path"; then
    printf 'VPSI aggregate space must retain fractional kilobytes: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi
  if ! grep -Eq 'spaceKilobytes[[:space:]]*=.*[/][[:space:]]*1024\.0' "$source_path"; then
    printf 'VPSI aggregate space must divide bytes by 1024.0: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi
done

readme_path="$repo_root/README.md"
if [[ ! -f "$readme_path" ]]; then
	printf 'Missing: README.md\n' >&2
	exit 1
fi

ruby - "$readme_path" <<'RUBY'
def assert(condition, message)
  raise message unless condition
end

def assert_match(text, pattern, message)
  assert(text.match?(pattern), message)
end

readme_path = ARGV.fetch(0)
source = File.read(readme_path)
cpp_section = source[/^## 3\. C\+\+$.*?(?=^## 4\. Acknowledgment$)/m]
assert(cpp_section, "README C++ section must precede Acknowledgment")

inventory = source.split(/^Most of the cryptographic schemes here/, 2).first
expected_cpp_links = [
  "./PSI-CA/OPSI-CA.cpp",
  "./PSI-CA/PSI-CA.cpp",
  "./PSI-CA/SPSI-CA(1).cpp",
  "./PSI-CA/VPSI-CA-Alg2.cpp",
  "./PSI-CA/VPSI-CA-Alg3.cpp",
  "./PSI-CA/VPSI-CA-Alg4.cpp",
  "./PSI-CA/VPSI-CA-Alg5.cpp"
]
assert(inventory.include?("- [PSI-CA](./PSI-CA/)"), "README inventory must include the PSI-CA directory")
actual_cpp_links = inventory.scan(/\]\((\.\/PSI-CA\/.*\.cpp)\)$/).flatten
assert(actual_cpp_links == expected_cpp_links, "README inventory must list exactly the seven PSI-CA C++ programs")

assert_match(cpp_section, /algorithms.*formatting.*C\+\+17 standard library/i, "README must describe the standard-library algorithm and formatting dependency")
assert_match(cpp_section, /no third-party libraries/i, "README must state that the PSI-CA programs use no third-party libraries")
assert_match(cpp_section, /file-publication path.*Ubuntu\/POSIX APIs/i, "README must identify the POSIX file-publication dependency")
assert_match(cpp_section, /target Ubuntu\/POSIX.*compilation on Windows.*rejected/i, "README must document the supported platform and Windows rejection")
assert(!cpp_section.include?("only the C++ standard library"), "README must not hide the POSIX API dependency")
assert_match(cpp_section, /do not require PBC/i, "README must distinguish the PSI-CA programs from PBC-dependent C/C++")
assert(cpp_section.include?('g++ -std=c++17 -Wall -Wextra -Wpedantic "PSI-CA/SPSI-CA(1).cpp" -o "PSI-CA/SPSI-CA(1)"'), "README must retain the quoted C++17 compile command")
assert(cpp_section.include?('-o "results.csv" -p 9 -r 10 -t 0 -y'), "README must retain the noninteractive run command")

assert_match(cpp_section, /Only UTF-8.*accepted/i, "README must document the UTF-8-only input encoding")
assert_match(cpp_section, /default output.*``<program>\.csv``/i, "README must document the default CSV output name")
assert_match(cpp_section, /default decimal place.*9/i, "README must document decimal place 9")
assert_match(cpp_section, /default run count.*10/i, "README must document run count 10")
assert_match(cpp_section, /default waiting time.*infinity/i, "README must document the non-blocking infinite default")
assert_match(cpp_section, /relative output path.*executable's directory/i, "README must document the executable-relative output base")
assert_match(cpp_section, /directory path.*default.*``<program>\.csv``/i, "README must document output directory handling")
assert_match(cpp_section, /protected source or script extension.*reset.*``\.csv``/i, "README must document parser protection for extensions")
assert_match(cpp_section, /saver.*rejects a protected extension/i, "README must document saver protection for extensions")
assert_match(cpp_section, /existing output.*``-y``.*interactive confirmation/i, "README must document overwrite authorization")
assert_match(cpp_section, /private.*workspace/i, "README must document the private output workspace")
%w[mkdirat openat linkat renameat unlinkat].each do |api|
  assert(cpp_section.include?("``#{api}``"), "README must document directory-FD API #{api}")
end
assert_match(cpp_section, /directory-FD-anchored/i, "README must describe descriptor-anchored publication operations")
assert_match(cpp_section, /``linkat``.*no-clobber/i, "README must document no-clobber publication")
assert_match(cpp_section, /``renameat``.*authorized replacement/i, "README must document authorized replacement")
assert_match(cpp_section, /not crash-durable/i, "README must not imply crash durability")
assert_match(cpp_section, /does not call ``fsync``/i, "README must explain the crash-durability limit")
assert_match(cpp_section, /rejects.*symlink.*non-regular target/i, "README must document unsafe target rejection")
assert_match(cpp_section, /not a general security guarantee/i, "README must avoid overstating the publication mechanism")
assert_match(cpp_section, /CSV, TSV, and TXT.*default/i, "README must document supported formats and the default")
assert_match(cpp_section, /unsupported extension.*TXT.*fallback/i, "README must document TXT fallback")
assert_match(cpp_section, /integers are written without decimal places/, "README must document integer formatting with sentence-internal lowercase")
assert(!cpp_section.include?("Integers are written"), "README must not capitalize sentence-internal integers")

assert_match(cpp_section, /``EXIT_SUCCESS``.*\(\$0\$\).*result is saved/i, "README must document successful save exit 0")
assert_match(cpp_section, /``EXIT_FAILURE``.*\(\$1\$\).*saving fails/i, "README must document save failure exit 1")
assert_match(cpp_section, /invalid arguments.*``-1``.*255.*POSIX/i, "README must document literal -1 and POSIX status 255")
assert(!cpp_section.match?(/\bEOF\b/), "README must not call the invalid-argument return value EOF")
assert_match(cpp_section, /Help exits.*\(\$0\$\)/i, "README must document help exit 0")
assert_match(cpp_section, /do not expose a separate correctness boolean/i, "README must not tie exit status to an unavailable correctness result")

{
  "encoding" => %w[e /e -e encoding /encoding --encoding],
  "help" => %w[h /h -h help /help --help],
  "output" => %w[o /o -o output /output --output],
  "place" => %w[p /p -p place /place --place],
  "quiet" => %w[q /q -q quiet /quiet --quiet],
  "run" => %w[r /r -r run /run --run],
  "time" => %w[t /t -t time /time --time],
  "yes" => %w[y /y -y yes /yes --yes]
}.each do |name, aliases|
  aliases.each do |alias_name|
    assert(cpp_section.match?(/`#{Regexp.escape(alias_name)}`/), "README must document the #{name} alias #{alias_name}")
  end
end
assert_match(cpp_section, /aliases.*case-insensitive/i, "README must state that C++ option aliases are case-insensitive")

assert(cpp_section.include?("[``runCPP`` workflow](./.github/workflows/runCPP.yml)"), "README must link the runCPP workflow")
assert_match(cpp_section, /seven-program matrix.*``ubuntu-latest``/i, "README must document the workflow matrix and Ubuntu runner")
assert_match(cpp_section, /actions.*pinned.*commit SHAs/i, "README must document pinned actions")
assert(cpp_section.include?("``-std=c++17 -Wall -Wextra -Wpedantic``"), "README must document the workflow compiler flags")
assert_match(cpp_section, /help command.*real execution/i, "README must document both workflow executions")
assert_match(cpp_section, /uploaded as artifacts/i, "README must document artifact upload")
assert_match(cpp_section, /Manual dispatch.*output format.*decimal place.*run count/i, "README must document the manual inputs")
assert_match(cpp_section, /run count.*1 through 100.*falls back to 10/i, "README must document the manual run cap and fallback")
assert_match(cpp_section, /30-minute timeout/i, "README must document the workflow timeout")

assert_match(cpp_section, /^### 3\.2 Time complexity$/m, "README must include the C++ time subsection")
assert_match(cpp_section, /``clock\(\)``.*``CLOCKS_PER_SEC``.*milliseconds/im, "README must document CPU-clock conversion to milliseconds")
assert_match(cpp_section, /per-run average/i, "README must document time averaging")
assert_match(cpp_section, /``baseNum``.*VPSI.*``adjust``/im, "README must document retained baseNum and VPSI adjustments")
assert_match(cpp_section, /OPSI.*SPSI.*actor-specific modeled costs/im, "README must document retained modeled actor costs")

assert_match(cpp_section, /^### 3\.3 Space complexity$/m, "README must include the C++ space subsection")
assert_match(cpp_section, /academic formulas.*``printSize``/i, "README must identify printSize academic estimates")
assert_match(cpp_section, /OPSI-CA.*PSI-CA.*SPSI-CA.*report bytes/i, "README must document byte units for OPSI, PSI, and SPSI")
assert_match(cpp_section, /VPSI.*aggregate byte totals.*1024\.0.*kilobytes/i, "README must document correct VPSI byte-to-kilobyte conversion")
assert_match(cpp_section, /serialized or object-model estimates.*not.*process RSS/im, "README must distinguish estimates from process RSS")
assert_match(cpp_section, /external memory monitor.*peak memory/i, "README must direct peak-memory measurement externally")

dependency_table = source[/\| Programming language \| Dependency \|.*?(?=\n\n)/m]
assert(dependency_table, "README must retain the dependency summary table")
assert(dependency_table.include?("C++ (PSI-CA)"), "README dependency table must distinguish the standalone PSI-CA C++ programs")
assert(dependency_table.include?("C (and pairing-based C/C++)"), "README dependency table must retain the PBC pairing dependency separately")
assert_match(dependency_table, /C\+\+17 standard library.*Ubuntu\/POSIX APIs.*no third-party libraries/i, "README dependency table must state both PSI-CA platform dependencies")
assert_match(dependency_table, /Windows compilation.*rejected/i, "README dependency table must document Windows rejection")
RUBY

printf 'PASS: README documents the seven C++ programs, their CLI, workflow, and measurements.\n'

workflow_path="$repo_root/.github/workflows/runCPP.yml"
if [[ ! -f "$workflow_path" ]]; then
  printf 'Missing: .github/workflows/runCPP.yml\n' >&2
  exit 1
fi

ruby - "$workflow_path" <<'RUBY'
require "json"
require "open3"
require "tmpdir"
require "yaml"

def assert(condition, message)
  raise message unless condition
end

def execute_output(step, environment, output_name)
  Dir.mktmpdir("runCPP-step") do |directory|
    output_path = File.join(directory, "github-output")
    command_environment = { "GITHUB_OUTPUT" => output_path }.merge(environment)
    stdout, stderr, status = Open3.capture3(command_environment, "bash", "-c", step.fetch("run"))
    assert(status.success?, "#{step.fetch("name")} failed for #{environment.inspect}: #{stdout}#{stderr}")
    outputs = File.readlines(output_path, chomp: true).to_h { |line| line.split("=", 2) }
    outputs.fetch(output_name)
  end
end

workflow_path = ARGV.fetch(0)
workflow_source = File.read(workflow_path)
workflow = YAML.load_file(workflow_path)
assert(workflow.is_a?(Hash), "runCPP.yml must contain a YAML mapping")
assert(workflow["name"] == "Run C++", "runCPP.yml must be named Run C++")

triggers = workflow["on"] || workflow[true]
assert(triggers.is_a?(Hash), "runCPP.yml must define workflow triggers")
expected_paths = ["PSI-CA/**/*.cpp", "PSI-CA/**/*.hpp", ".github/workflows/runCPP.yml"]
["pull_request", "push"].each do |event|
  config = triggers[event]
  assert(config.is_a?(Hash), "runCPP.yml must define #{event}")
  assert(config["branches"] == ["main"], "#{event} must target main")
  assert(config["paths"] == expected_paths, "#{event} must filter C++ sources, headers, and runCPP.yml")
end

dispatch = triggers["workflow_dispatch"]
assert(dispatch.is_a?(Hash), "runCPP.yml must define workflow_dispatch")
inputs = dispatch["inputs"]
assert(inputs.is_a?(Hash), "workflow_dispatch must define inputs")
assert(inputs.keys.sort == ["decimalPlace", "outputFormat", "runCount"], "workflow_dispatch must expose exactly the supported inputs")
assert(inputs.dig("outputFormat", "type") == "choice", "outputFormat must be a choice")
assert(inputs.dig("outputFormat", "default") == ".csv", "outputFormat must default to .csv")
assert(inputs.dig("outputFormat", "options") == [".csv", ".tsv", ".txt"], "outputFormat choices must be the C++ formats")
expected_decimal_places = %w[0 s second 3 ms millisecond 6 microsecond 9 ns nanosecond 12 ps picosecond 15 fs femtosecond 18]
assert(inputs.dig("decimalPlace", "type") == "choice", "decimalPlace must be a choice")
assert(inputs.dig("decimalPlace", "default").to_s == "9", "decimalPlace must default to 9")
assert(inputs.dig("decimalPlace", "options").map(&:to_s) == expected_decimal_places, "decimalPlace choices are incomplete")
assert(inputs.dig("runCount", "type") == "string", "runCount must be a string")
assert(inputs.dig("runCount", "default") == "10", "runCount must default to 10")
assert(inputs.dig("runCount", "description").to_s.include?("1-100"), "runCount must document its CI range")

jobs = workflow["jobs"]
prepare = jobs && jobs["prepare"]
run_cpp = jobs && jobs["runCPP"]
assert(prepare.is_a?(Hash) && run_cpp.is_a?(Hash), "runCPP.yml must define prepare and runCPP jobs")
assert(prepare["runs-on"] == "ubuntu-latest", "prepare must run on ubuntu-latest")
assert(run_cpp["runs-on"] == "ubuntu-latest", "runCPP must run on ubuntu-latest")
assert(run_cpp["timeout-minutes"] == 30, "runCPP must time out after 30 minutes")
assert(run_cpp.dig("strategy", "fail-fast") == false, "runCPP matrix must not fail fast")

expected_sources = [
  "PSI-CA/OPSI-CA.cpp",
  "PSI-CA/PSI-CA.cpp",
  "PSI-CA/SPSI-CA(1).cpp",
  "PSI-CA/VPSI-CA-Alg2.cpp",
  "PSI-CA/VPSI-CA-Alg3.cpp",
  "PSI-CA/VPSI-CA-Alg4.cpp",
  "PSI-CA/VPSI-CA-Alg5.cpp"
]
prepare_steps = prepare["steps"] || []
matrix_step = prepare_steps.find { |step| step["id"] == "matrix" }
assert(matrix_step, "prepare must expose a matrix step")
matrix_json = matrix_step["run"].to_s[/matrix='([^']+)'/, 1]
assert(matrix_json, "matrix step must prepare an exact JSON array")
assert(JSON.parse(matrix_json) == expected_sources, "matrix must contain exactly the seven C++ sources")
assert(prepare.dig("outputs", "matrix") == "${{ steps.matrix.outputs.matrix }}", "prepare must expose the matrix")
assert(prepare.dig("outputs", "decimalPlace") == "${{ steps.decimalPlace.outputs.decimalPlace }}", "prepare must expose decimalPlace")
assert(prepare.dig("outputs", "runCount") == "${{ steps.runCount.outputs.runCount }}", "prepare must expose runCount")
assert(prepare.dig("outputs", "timeString") == "${{ steps.timeString.outputs.timeString }}", "prepare must expose timeString")

decimal_step = prepare_steps.find { |step| step["id"] == "decimalPlace" }
run_count_step = prepare_steps.find { |step| step["id"] == "runCount" }
time_step = prepare_steps.find { |step| step["id"] == "timeString" }
assert(decimal_step && decimal_step["run"].to_s.include?("femtosecond"), "prepare must translate decimal-place aliases")
assert(decimal_step["run"].to_s.include?("decimalPlace=18"), "prepare must support decimal place 18")
assert(run_count_step && run_count_step["run"].to_s.include?("runCount=10"), "prepare must safely default invalid run counts")
assert(time_step && time_step.dig("env", "TZ") == "Asia/Hong_Kong", "prepare must use Hong Kong time")
assert(time_step["run"].to_s.include?("HKT"), "timeString must use the HKT prefix")

{
  "0" => "10",
  "000" => "10",
  "001" => "1",
  "100" => "100",
  "101" => "10",
  "-1" => "10",
  "junk" => "10",
  "999999999999999999999999999999" => "10"
}.each do |input, expected|
  actual = execute_output(run_count_step, { "runCountString" => input }, "runCount")
  assert(actual == expected, "runCount #{input.inspect} must normalize to #{expected}, got #{actual}")
end

{
  "0" => "0", "s" => "0", "second" => "0",
  "3" => "3", "ms" => "3", "millisecond" => "3",
  "6" => "6", "microsecond" => "6",
  "9" => "9", "ns" => "9", "nanosecond" => "9",
  "12" => "12", "ps" => "12", "picosecond" => "12",
  "15" => "15", "fs" => "15", "femtosecond" => "15",
  "18" => "18", "invalid" => "9"
}.each do |input, expected|
  actual = execute_output(decimal_step, { "decimalPlaceString" => input }, "decimalPlace")
  assert(actual == expected, "decimalPlace #{input.inspect} must normalize to #{expected}, got #{actual}")
end

multiline_steps = jobs.values.flat_map { |job| job["steps"] || [] }.select { |step| step["run"].is_a?(String) }
assert(multiline_steps.all? { |step| step["run"].start_with?("set -euo pipefail\n") }, "every multiline run block must enable strict shell mode")

run_steps = run_cpp["steps"] || []
checkout_sha = "d23441a48e516b6c34aea4fa41551a30e30af803"
upload_sha = "043fb46d1a93c77aae656e7c1c64a875d1fc6a0a"
checkout = run_steps.find { |step| step["uses"] == "actions/checkout@#{checkout_sha}" }
assert(checkout && checkout.dig("with", "sparse-checkout").to_s.lines.map(&:strip).include?("PSI-CA/"), "runCPP must sparse-checkout PSI-CA")
assert(checkout.dig("with", "persist-credentials") == false, "checkout must not persist credentials")
assert(workflow_source.include?("uses: actions/checkout@#{checkout_sha} # v6"), "checkout SHA must retain its v6 comment")
ready = run_steps.find { |step| step["id"] == "ready" }
assert(ready, "runCPP must derive safe source and output paths")
ready_script = ready["run"].to_s
assert(ready_script.include?('sourcePath="${{ github.workspace }}/${{ matrix.cryptographicScheme }}"'), "sourcePath must be rooted in the workspace")
assert(ready_script.include?('outputFilePath="${directoryPath}/${mainFileName}-C++'), "output path must remain beside the source")

compile_step = run_steps.find { |step| step["name"] == "Compile the cryptographic scheme" }
assert(compile_step, "runCPP must compile every matrix source")
compile_command = 'g++ -std=c++17 -Wall -Wextra -Wpedantic "${sourcePath}" -o "${binaryPath}"'
assert(compile_step["run"].to_s.include?(compile_command), "compile command must use C++17 and all warning flags")

execute_step = run_steps.find { |step| step["name"] == "Run the cryptographic scheme" }
assert(execute_step, "runCPP must execute every compiled binary")
execute_script = execute_step["run"].to_s
assert(execute_script.include?('"${binaryPath}" -t 0 -h'), "runCPP must execute the help command")
actual_command = '"${binaryPath}" -o "${outputFileName}" -p "${decimalPlace}" -r "${runCount}" -t 0 -y'
assert(execute_script.include?(actual_command), "runCPP must execute the real command with all required options")

upload = run_steps.find { |step| step["uses"] == "actions/upload-artifact@#{upload_sha}" }
assert(upload, "runCPP must upload the result with the approved upload-artifact SHA")
assert(workflow_source.include?("uses: actions/upload-artifact@#{upload_sha} # v7"), "upload-artifact SHA must retain its v7 comment")
assert(upload.dig("with", "path") == "${{ github.workspace }}/${{ steps.ready.outputs.outputFilePath }}", "artifact path must point to the generated PSI-CA result")
assert(upload.dig("with", "archive") == false, "artifact upload must disable archiving")
assert(upload.dig("with", "if-no-files-found") == "error", "artifact upload must fail when output is absent")
RUBY

if command -v actionlint >/dev/null 2>&1; then
  actionlint "$workflow_path" >/dev/null
fi

printf 'PASS: runCPP workflow covers all seven programs with strict compiler warnings and artifact checks.\n'

test_directory="$(mktemp -d "${TMPDIR:-/tmp}/PSI-CA-CPP.XXXXXX")"
trap 'rm -rf "$test_directory"' EXIT
test_binary="$test_directory/ParserSaverTest"
"${CXX:-g++}" -std=c++17 -Wall -Wextra -Wpedantic "$repo_root/tests/cpp/ParserSaverTest.cpp" -o "$test_binary"
"$test_binary"

for filename in "${expected_files[@]}"; do
  source_path="$psi_ca_dir/$filename"
  basename="${filename%.cpp}"
  binary_path="$test_directory/$basename"
  output_path="$test_directory/$basename.csv"

  if ! grep -Fq '#include "ParserSaver.hpp"' "$source_path"; then
    printf 'Missing ParserSaver.hpp integration: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi
  if ! grep -Eq 'int[[:space:]]+main[[:space:]]*\([[:space:]]*int[[:space:]]+argc[[:space:]]*,[[:space:]]*char[[:space:]]*\*[[:space:]]*argv[[:space:]]*\[[[:space:]]*\][[:space:]]*\)' "$source_path"; then
    printf 'Missing command-line main signature: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi
  if ! grep -Eq 'Parser[[:space:]]+[[:alnum:]_]+' "$source_path"; then
    printf 'Missing Parser construction: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi
  if ! grep -Eq 'Saver[[:space:]]+[[:alnum:]_]+' "$source_path"; then
    printf 'Missing Saver construction: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi
  if ! grep -Fq 'options.runCount' "$source_path"; then
    printf 'Missing run-count integration: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi
  if grep -Eq '(^|[^[:alnum:]_])dump[[:space:]]*\(' "$source_path"; then
    printf 'Legacy dump function remains: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi

  "${CXX:-g++}" -std=c++17 -Wall -Wextra -Wpedantic "$source_path" -o "$binary_path"
  "$binary_path" -h -t 0 >"$test_directory/$basename-help.stdout" 2>"$test_directory/$basename-help.stderr"
  "$binary_path" -q -r 2 -t 0 -y -o "$output_path" >"$test_directory/$basename-run.stdout" 2>"$test_directory/$basename-run.stderr"

  path_run_directory="$test_directory/$basename-path-cwd"
  path_output_name="$basename-path.csv"
  path_output="$test_directory/$path_output_name"
  mkdir -p "$path_run_directory"
  (
    cd "$path_run_directory"
    PATH="$test_directory:${PATH:-}" "$basename" -q -r 1 -t 0 -y -o "$path_output_name"
  ) >"$test_directory/$basename-path.stdout" 2>"$test_directory/$basename-path.stderr"
  if [[ ! -s "$path_output" ]]; then
    printf 'Bare PATH launch did not save beside its binary: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi
  if [[ -e "$path_run_directory/$path_output_name" ]]; then
    printf 'Bare PATH launch incorrectly saved relative to cwd: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi

  if [[ ! -s "$output_path" ]]; then
    printf 'Missing or empty saved output: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi
  header="$(head -n 1 "$output_path")"
  if [[ "$header" != *scheme* || "$header" != *runCount* ]]; then
    printf 'Saved header lacks scheme/runCount: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi
  if [[ "$(wc -l < "$output_path")" -lt 2 ]]; then
    printf 'Saved output lacks a data row: PSI-CA/%s\n' "$filename" >&2
    exit 1
  fi

  python3 - "$output_path" <<'PY'
import csv
import math
import pathlib
import sys

path = pathlib.Path(sys.argv[1])
with path.open(newline="", encoding="utf-8") as stream:
    rows = list(csv.reader(stream))

if len(rows) != 2:
    raise SystemExit(f"{path}: expected exactly one header and one data row, got {len(rows)} rows")
header, data = rows
if len(header) != len(data):
    raise SystemExit(f"{path}: header has {len(header)} fields but data has {len(data)}")
record = dict(zip(header, data))
if record.get("runCount") != "2":
    raise SystemExit(f"{path}: expected saved runCount 2, got {record.get('runCount')!r}")

scheme = record.get("scheme")
if scheme and scheme.startswith("VPSI-CA-"):
    space_kilobytes = float(record["spaceKilobytes"])
    if not math.isfinite(space_kilobytes) or space_kilobytes <= 0.0:
        raise SystemExit(f"{path}: expected a positive finite VPSI spaceKilobytes value")
if scheme == "OPSI-CA":
    sender_overhead = ((1 << 22) - (1 << 10)) / math.pow(math.e, 3) / math.log(6)
    if float(record["senderCpuMilliseconds"]) < sender_overhead:
        raise SystemExit(f"{path}: OPSI sender modeled overhead is missing")
    if float(record["cloudCpuMilliseconds"]) < 10.0:
        raise SystemExit(f"{path}: OPSI cloud modeled overhead is missing")
elif scheme == "SPSI-CA":
    sender_overhead = ((1 << 24) - (1 << 12)) / math.pow(math.e, 3) / math.log(6)
    if float(record["senderCpuMilliseconds"]) < sender_overhead:
        raise SystemExit(f"{path}: SPSI sender modeled overhead is missing")
    if float(record["cloudCpuMilliseconds"]) < 36.0:
        raise SystemExit(f"{path}: SPSI cloud modeled overhead is missing")
PY
done

printf 'PASS: all seven C++ programs compile, accept CLI options, and save a summary row.\n'
