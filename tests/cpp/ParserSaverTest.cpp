#include "../../PSI-CA/ParserSaver.hpp"

#include <chrono>
#include <clocale>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace
{
	static_assert(
		sizeof(ScopedOutputSilencer) < sizeof(std::ostringstream),
		"ScopedOutputSilencer must use a non-retaining discard buffer.");

	int failures = 0;

	void expect(const bool condition, const std::string &message)
	{
		if (!condition)
		{
			std::cerr << "FAIL: " << message << '\n';
			++failures;
		}
	}

	std::string readFile(const std::filesystem::path &path)
	{
		std::ifstream input(path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
	}

	bool hasSaverTemporaryArtifact(const std::filesystem::path &root)
	{
		std::error_code error;
		std::filesystem::recursive_directory_iterator iterator(root, error);
		if (error)
		{
			throw std::runtime_error("Could not inspect test artifacts: " + error.message());
		}
		const std::filesystem::recursive_directory_iterator end;
		while (iterator != end)
		{
			const std::string filename = iterator->path().filename().string();
			if (filename.rfind(".parser-saver-", 0U) == 0U)
			{
				return true;
			}
			iterator.increment(error);
			if (error)
			{
				throw std::runtime_error("Could not inspect test artifacts: " + error.message());
			}
		}
		return false;
	}

	std::size_t countOpenDescriptors()
	{
		std::size_t count = 0U;
		for (int descriptor = 0; descriptor < 1024; ++descriptor)
		{
			if (::fcntl(descriptor, F_GETFD) != -1)
			{
				++count;
			}
		}
		return count;
	}

	class Arguments
	{
	public:
		explicit Arguments(std::vector<std::string> values) : values_(std::move(values))
		{
			for (std::string &value : values_)
			{
				pointers_.push_back(value.data());
			}
		}

		int count() const
		{
			return static_cast<int>(pointers_.size());
		}

		char **data()
		{
			return pointers_.data();
		}

	private:
		std::vector<std::string> values_;
		std::vector<char *> pointers_;
	};

	class TemporaryDirectory
	{
	public:
		TemporaryDirectory()
		{
			static std::uint64_t counter = 0U;
			for (std::uint64_t attempt = 0U; attempt < 1000U; ++attempt)
			{
				const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
				path_ = std::filesystem::temp_directory_path() / ("parser_saver_cpp_test_" + std::to_string(timestamp) + "_" + std::to_string(++counter));
				std::error_code error;
				if (std::filesystem::create_directory(path_, error))
				{
					return;
				}
				if (error && error != std::errc::file_exists)
				{
					throw std::runtime_error("Could not create test directory: " + error.message());
				}
			}
			throw std::runtime_error("Could not allocate a unique test directory.");
		}

		~TemporaryDirectory()
		{
			std::error_code error;
			std::filesystem::remove_all(path_, error);
		}

		TemporaryDirectory(const TemporaryDirectory &) = delete;
		TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

		const std::filesystem::path &path() const
		{
			return path_;
		}

	private:
		std::filesystem::path path_;
	};

	class ScopedUmask
	{
	public:
		explicit ScopedUmask(const mode_t mask) : previous_(::umask(mask))
		{
		}

		~ScopedUmask()
		{
			::umask(previous_);
		}

		ScopedUmask(const ScopedUmask &) = delete;
		ScopedUmask &operator=(const ScopedUmask &) = delete;

	private:
		mode_t previous_;
	};

	ParserOptions parse(
		std::vector<std::string> arguments,
		const std::string &schemeName,
		std::istream &input,
		std::ostream &output,
		std::ostream &error,
		const bool interactiveInput = false)
	{
		Arguments values(std::move(arguments));
		Parser parser(values.count(), values.data(), schemeName, interactiveInput, input, output, error);
		return parser.parse();
	}

	void testParserDefaultsAndAliases(const std::filesystem::path &root)
	{
		std::istringstream input;
		std::ostringstream output;
		std::ostringstream error;
		const std::filesystem::path executable = root / "bin" / "tool";
		std::filesystem::create_directories(executable.parent_path());

		const ParserOptions defaults = parse({executable.string()}, "Demo", input, output, error);
		expect(defaults.flag == 1, "default parse succeeds");
		expect(defaults.encoding == "utf-8", "encoding defaults to UTF-8");
		expect(defaults.outputPath == (executable.parent_path() / "Demo.csv").lexically_normal(), "default output is beside executable");
		expect(defaults.decimalPlace == 9, "decimal place defaults to nine");
		expect(!defaults.quiet, "quiet defaults to false");
		expect(defaults.runCount == 10U, "run count defaults to ten");
		expect(std::isinf(defaults.waitingTime), "waiting time defaults to infinity");
		expect(!defaults.overwriteConfirmed, "overwrite defaults to false");

		const ParserOptions aliases = parse(
			{executable.string(), "/EnCoDiNg", "UTF8", "--OuTpUt", "results/MiXeD.CsV", "/PlAcE", "microsecond", "--QuIeT", "/RuN", "7", "--TiMe", "1.25", "/YeS"},
			"Demo",
			input,
			output,
			error);
		expect(aliases.flag == 1, "mixed-case long aliases parse");
		expect(aliases.encoding == "utf-8", "UTF8 spelling is accepted");
		expect(aliases.outputPath == (executable.parent_path() / "results" / "MiXeD.CsV").lexically_normal(), "relative output uses executable directory");
		expect(aliases.decimalPlace == 6, "place translation parses");
		expect(aliases.quiet, "quiet alias parses");
		expect(aliases.runCount == 7U, "run count parses");
		expect(aliases.waitingTime == 1.25, "waiting time parses");
		expect(aliases.overwriteConfirmed, "overwrite alias parses");

		const ParserOptions shortOptions = parse(
			{executable.string(), "-o", "short.tsv", "-p", "3", "-q", "-r", "2", "-t", "0", "-y"},
			"Demo",
			input,
			output,
			error);
		expect(shortOptions.flag == 1 && shortOptions.decimalPlace == 3 && shortOptions.quiet && shortOptions.runCount == 2U && shortOptions.waitingTime == 0.0 && shortOptions.overwriteConfirmed, "all principal short options parse");
	}

	void testParserPathsAndValidation(const std::filesystem::path &root)
	{
		const std::filesystem::path executable = root / "app" / "runner";
		const std::filesystem::path outputDirectory = executable.parent_path() / "reports";
		std::filesystem::create_directories(outputDirectory);
		std::istringstream input;
		std::ostringstream output;
		std::ostringstream error;

		const ParserOptions directory = parse({executable.string(), "-o", "reports", "-y"}, "Scheme", input, output, error);
		expect(directory.outputPath == outputDirectory / "Scheme.csv", "directory output appends default filename");

		const ParserOptions protectedPath = parse({executable.string(), "-o", "source.CpP", "-y"}, "Scheme", input, output, error);
		expect(protectedPath.outputPath == executable.parent_path() / "source.csv", "protected extension is reset to CSV");

		const ParserOptions consoleOutput = parse({executable.string(), "-o", ""}, "Scheme", input, output, error);
		expect(consoleOutput.flag == 1 && consoleOutput.outputPath.empty(), "empty output path selects console mode");

		Arguments noArguments(std::vector<std::string>{});
		Parser noArgvParser(0, noArguments.data(), "NoArgv", false, input, output, error);
		const ParserOptions noArgv = noArgvParser.parse();
		expect(noArgv.outputPath == (std::filesystem::current_path() / "NoArgv.csv").lexically_normal(), "argc zero places default output in the current directory");

		std::ostringstream helpOutput;
		const ParserOptions help = parse({executable.string(), "/H"}, "Scheme", input, helpOutput, error);
		expect(help.flag == 0 && helpOutput.str().find("Usage") != std::string::npos, "help returns zero and prints usage");

		const std::vector<std::vector<std::string>> invalidArguments = {
			{executable.string(), "-o"},
			{executable.string(), "-e", "latin1"},
			{executable.string(), "-p", "unknown"},
			{executable.string(), "-p", "-1"},
			{executable.string(), "-p", "19"},
			{executable.string(), "-p", "999999999999999999999999999999"},
			{executable.string(), "-r", "0"},
			{executable.string(), "-r", "abc"},
			{executable.string(), "-t", "-0.1"},
			{executable.string(), "--mystery"}};
		for (const std::vector<std::string> &arguments : invalidArguments)
		{
			std::ostringstream diagnostic;
			const ParserOptions invalid = parse(arguments, "Scheme", input, output, diagnostic);
			expect(invalid.flag < 0, "invalid or missing option value returns a negative flag");
			expect(!diagnostic.str().empty(), "invalid option emits a diagnostic");
		}
		const ParserOptions maximumPlace = parse({executable.string(), "-p", "18"}, "Scheme", input, output, error);
		expect(maximumPlace.flag == 1 && maximumPlace.decimalPlace == Parser::MAX_DECIMAL_PLACE, "maximum decimal precision is accepted");

		const std::string longFilename(300U, 'a');
		std::ostringstream longPathDiagnostic;
		const ParserOptions longPath = parse({executable.string(), "-o", longFilename}, "Scheme", input, output, longPathDiagnostic);
		expect(longPath.flag < 0 && !longPathDiagnostic.str().empty(), "filesystem inspection errors return a negative flag and diagnostic");

		const ParserOptions infiniteTime = parse({executable.string(), "-t", "inf"}, "Scheme", input, output, error);
		expect(infiniteTime.flag == 1 && std::isinf(infiniteTime.waitingTime), "time parser accepts infinity explicitly");
		const char *const currentLocale = std::setlocale(LC_NUMERIC, nullptr);
		const std::string savedLocale = currentLocale == nullptr ? "C" : currentLocale;
		const std::vector<const char *> localeCandidates = {"de_DE.UTF-8", "de_DE.utf8", "fr_FR.UTF-8", "fr_FR.utf8"};
		for (const char *candidate : localeCandidates)
		{
			const char *const selected = std::setlocale(LC_NUMERIC, candidate);
			if (selected != nullptr && std::string(selected) != "C")
			{
				const ParserOptions localeIndependent = parse({executable.string(), "-t", "1.5"}, "Scheme", input, output, error);
				expect(localeIndependent.flag == 1 && localeIndependent.waitingTime == 1.5, "time parsing uses the classic locale");
				break;
			}
		}
		std::setlocale(LC_NUMERIC, savedLocale.c_str());

		const std::filesystem::path existing = executable.parent_path() / "existing.csv";
		std::filesystem::create_directories(existing.parent_path());
		{
			std::ofstream file(existing);
			file << "old";
		}
		std::istringstream noAnswer("no\n");
		ParserOptions denied = parse({executable.string(), "-o", "existing.csv"}, "Scheme", noAnswer, output, error, true);
		expect(denied.flag < 0 && !denied.overwriteConfirmed, "existing output is denied without affirmative confirmation");
		std::istringstream yesAnswer("YeS\n");
		ParserOptions confirmed = parse({executable.string(), "-o", "existing.csv"}, "Scheme", yesAnswer, output, error, true);
		expect(confirmed.flag == 1 && confirmed.overwriteConfirmed, "interactive yes permits overwrite");
		std::istringstream pipedYes("yes\n");
		ParserOptions noninteractive = parse({executable.string(), "-o", "existing.csv"}, "Scheme", pipedYes, output, error);
		expect(noninteractive.flag < 0 && !noninteractive.overwriteConfirmed, "noninteractive input never authorizes overwrite");
		expect(pipedYes.peek() == 'y', "noninteractive overwrite check does not read piped input");
	}

	void testExecutablePathResolution(const std::filesystem::path &root)
	{
		const std::filesystem::path currentDirectory = root / "working";
		const std::filesystem::path firstPathEntry = root / "first-bin";
		const std::filesystem::path secondPathEntry = root / "second-bin";
		std::filesystem::create_directories(currentDirectory);
		std::filesystem::create_directories(firstPathEntry);
		std::filesystem::create_directories(secondPathEntry);
		{
			std::ofstream executable(firstPathEntry / "bare-tool");
			executable << "not executable";
		}
		{
			std::ofstream executable(secondPathEntry / "bare-tool");
			executable << "tool";
		}
		std::error_code permissionError;
		std::filesystem::permissions(
			firstPathEntry / "bare-tool",
			std::filesystem::perms::group_exec,
			std::filesystem::perm_options::replace,
			permissionError);
		expect(!permissionError, "non-executable PATH fixture permissions are applied");
		std::filesystem::permissions(
			secondPathEntry / "bare-tool",
			std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec,
			std::filesystem::perm_options::replace,
			permissionError);
		expect(!permissionError, "executable PATH fixture permissions are applied");
		const std::string searchPath = firstPathEntry.string() + ":" + secondPathEntry.string();
#if defined(_WIN32)
		const std::filesystem::path expectedPathDirectory = firstPathEntry;
#else
		expect(::access((firstPathEntry / "bare-tool").c_str(), X_OK) != 0, "group-only execute mode is not executable by the owning user");
		const std::filesystem::path expectedPathDirectory = secondPathEntry;
#endif
		expect(
			parser_saver_detail::resolveExecutableDirectory("bare-tool", currentDirectory, searchPath, ':') == expectedPathDirectory.lexically_normal(),
			"bare argv zero resolves through the first executable PATH entry for the current identity");
		expect(
			parser_saver_detail::resolveExecutableDirectory("missing-tool", currentDirectory, searchPath, ':') == currentDirectory.lexically_normal(),
			"missing bare argv zero falls back to the current directory");
		expect(
			parser_saver_detail::resolveExecutableDirectory("relative/bin/tool", currentDirectory, secondPathEntry.string(), ':') == (currentDirectory / "relative" / "bin").lexically_normal(),
			"relative argv zero with a parent component does not use PATH");
		const std::filesystem::path absoluteExecutable = root / "absolute" / "bin" / "tool";
		expect(
			parser_saver_detail::resolveExecutableDirectory(absoluteExecutable, currentDirectory, secondPathEntry.string(), ':') == absoluteExecutable.parent_path().lexically_normal(),
			"absolute argv zero does not use PATH");
	}

	void testSaver(const std::filesystem::path &root)
	{
		const std::vector<std::string> columns = {"name", "value", "enabled", "empty"};
		const std::vector<std::vector<SaverValue>> rows = {
			{std::string("comma, quote \" and\nline"), 1.23456, true, std::monostate{}},
			{std::string("plain"), std::int64_t{-7}, false, std::uint64_t{42}}};
		std::ostringstream output;
		std::ostringstream error;
#if !defined(_WIN32)
		int pipeDescriptors[2] = {-1, -1};
		expect(::pipe(pipeDescriptors) == 0, "POSIX write helper test pipe is created");
		std::error_code writeError;
		const std::string descriptorPayload = "descriptor payload";
		expect(parser_saver_detail::writeAllToDescriptor(pipeDescriptors[1], descriptorPayload, writeError) && !writeError, "descriptor helper writes the complete payload");
		expect(::close(pipeDescriptors[1]) == 0, "POSIX write helper test closes its writer");
		char descriptorBuffer[32] = {};
		const ssize_t descriptorBytes = ::read(pipeDescriptors[0], descriptorBuffer, sizeof(descriptorBuffer));
		expect(descriptorBytes == static_cast<ssize_t>(descriptorPayload.size()) && std::string(descriptorBuffer, static_cast<std::size_t>(descriptorBytes)) == descriptorPayload, "descriptor helper preserves payload bytes");
		expect(::close(pipeDescriptors[0]) == 0, "POSIX write helper test closes its reader");
		int closePipeDescriptors[2] = {-1, -1};
		expect(::pipe(closePipeDescriptors) == 0, "checked close helper test pipe is created");
		std::error_code closeError;
		expect(parser_saver_detail::closeDescriptorChecked(closePipeDescriptors[1], closeError) && !closeError, "checked close helper closes an owned descriptor");
		expect(::fcntl(closePipeDescriptors[1], F_GETFD) == -1 && errno == EBADF, "checked close helper relinquishes descriptor ownership after one close call");
		expect(::close(closePipeDescriptors[0]) == 0, "checked close helper test closes its reader");
#endif

		const std::size_t descriptorsBeforeUmask = countOpenDescriptors();
		const std::filesystem::path umaskPath = root / "umask-denied.csv";
		Saver umaskSaver(umaskPath, {"value"}, 1, false, output, error);
		bool umaskSaved = false;
		{
			const ScopedUmask restrictiveUmask(0777);
			umaskSaved = umaskSaver.save({{std::string("blocked")}});
		}
		expect(!umaskSaved, "restrictive process umask produces an explicit save failure");
		expect(!std::filesystem::exists(umaskPath) && !hasSaverTemporaryArtifact(root), "restrictive umask failure leaves no output or temporary artifact");
		expect(countOpenDescriptors() == descriptorsBeforeUmask, "restrictive umask failure leaks no file descriptors");

		const std::filesystem::path realParent = root / "real-parent";
		const std::filesystem::path linkedParent = root / "linked-parent";
		std::filesystem::create_directories(realParent);
		std::error_code parentSymlinkError;
		std::filesystem::create_directory_symlink(realParent, linkedParent, parentSymlinkError);
		if (!parentSymlinkError)
		{
			Saver linkedParentSaver(linkedParent / "linked.csv", {"value"}, 1, false, output, error);
			expect(linkedParentSaver.save({{std::string("saved")}}), "Saver accepts a legitimate final parent-directory symlink");
			expect(readFile(realParent / "linked.csv") == "value\nsaved\n", "parent symlink output is anchored in the resolved directory");
		}

		const std::filesystem::path csvPath = root / "nested" / "data.CSV";
		Saver csv(csvPath, columns, 3, false, output, error);
		expect(csv.save(rows), "CSV save succeeds");
		const std::string csvText = readFile(csvPath);
		expect(csvText.find("\"comma, quote \"\" and\nline\"") != std::string::npos, "CSV special fields are quoted and quotes doubled");
		expect(csvText.find("1.235") != std::string::npos, "double is formatted to configured places");
		expect(csvText.find("-7.000") == std::string::npos && csvText.find(",-7,") != std::string::npos, "signed integer has no decimal suffix");
		expect(csvText.find(",42\n") != std::string::npos, "unsigned integer has no decimal suffix");
		expect(std::filesystem::is_directory(csvPath.parent_path()), "Saver creates parent directories");

		const std::filesystem::path tsvPath = root / "data.tSv";
		Saver tsv(tsvPath, {"field", "number"}, 2, false, output, error);
		expect(tsv.save({{std::string("tab\there"), 2.0}}), "TSV save succeeds case-insensitively");
		const std::string tsvText = readFile(tsvPath);
		expect(tsvText.find("\"tab\there\"\t2.00") != std::string::npos, "TSV quoting and decimal formatting are correct");

		const std::filesystem::path txtPath = root / "data.txt";
		Saver txt(txtPath, {"left", "right"}, 1, false, output, error);
		expect(txt.save({{std::string("alpha"), std::int64_t{9}}}), "TXT save succeeds");
		expect(readFile(txtPath) == "left\tright\nalpha\t9\n", "TXT content is tab-separated and preserves integers");
		const std::filesystem::path escapedTxtPath = root / "escaped.txt";
		Saver escapedTxt(escapedTxtPath, {"value"}, 1, false, output, error);
		expect(escapedTxt.save({{std::string("tab\tcr\rline\nslash\\")}}), "TXT save escapes structural characters");
		expect(readFile(escapedTxtPath) == "value\ntab\\tcr\\rline\\nslash\\\\\n", "TXT escaping preserves row and column structure");

		const std::filesystem::path fallbackPath = root / "data.bin";
		Saver fallback(fallbackPath, {"value"}, 1, false, output, error);
		expect(fallback.save({{std::string("text")}}), "unsupported extension falls back successfully");
		expect(std::filesystem::exists(fallbackPath) && readFile(fallbackPath) == "value\ntext\n", "fallback preserves path and writes TXT content");
		expect(output.str().find("TXT") != std::string::npos && output.str().find(fallbackPath.string()) != std::string::npos, "unsupported extension reports that TXT content was saved");

		std::ostringstream table;
		table << std::hex << std::setfill('*') << std::setprecision(4);
		table.width(11);
		const std::ios::fmtflags originalFlags = table.flags();
		const char originalFill = table.fill();
		const std::streamsize originalPrecision = table.precision();
		const std::streamsize originalWidth = table.width();
		Saver console({}, {"a", "b"}, 2, false, table, error);
		expect(console.save({{std::int64_t{1}, 2.5}}), "empty output path prints a table");
		expect(table.str().find("a") != std::string::npos && table.str().find("2.50") != std::string::npos, "printed table is readable and formatted");
		expect(table.flags() == originalFlags && table.fill() == originalFill && table.precision() == originalPrecision && table.width() == originalWidth, "console table restores stream formatting state");

		const std::filesystem::path existingPath = root / "existing-target.csv";
		{
			std::ofstream existing(existingPath);
			existing << "original\n";
		}
		Saver unauthorized(existingPath, {"value"}, 1, false, output, error);
		expect(!unauthorized.save({{std::string("replacement")}}), "Saver refuses an existing file without overwrite authorization");
		expect(readFile(existingPath) == "original\n", "unauthorized save preserves existing content");
		expect(!hasSaverTemporaryArtifact(root), "rejected save leaves no temporary artifact");
		Saver authorized(existingPath, {"value"}, 1, true, output, error);
		expect(authorized.save({{std::string("replacement")}}), "Saver replaces an existing regular file when authorized");
		expect(readFile(existingPath) == "value\nreplacement\n", "authorized save installs complete replacement content");
		expect(!hasSaverTemporaryArtifact(root), "successful save leaves no temporary artifact");
		bool artifactInspectionFailed = false;
		try
		{
			hasSaverTemporaryArtifact(root / "missing-artifact-root");
		}
		catch (const std::runtime_error &)
		{
			artifactInspectionFailed = true;
		}
		expect(artifactInspectionFailed, "artifact walker reports inspection errors instead of treating them as clean");

		const std::filesystem::path protectedPath = root / "protected.cpp";
		Saver protectedSaver(protectedPath, {"value"}, 1, true, output, error);
		expect(!protectedSaver.save({{std::string("blocked")}}) && !std::filesystem::exists(protectedPath), "Saver rejects protected output extensions");

		const std::filesystem::path directoryTarget = root / "directory.csv";
		std::filesystem::create_directories(directoryTarget);
		Saver directorySaver(directoryTarget, {"value"}, 1, true, output, error);
		expect(!directorySaver.save({{std::string("blocked")}}) && std::filesystem::is_directory(directoryTarget), "Saver rejects existing non-regular targets");

		const std::filesystem::path symlinkDestination = root / "symlink-destination.csv";
		{
			std::ofstream destination(symlinkDestination);
			destination << "destination\n";
		}
		const std::filesystem::path symlinkPath = root / "symlink.csv";
		std::error_code symlinkError;
		std::filesystem::create_symlink(symlinkDestination, symlinkPath, symlinkError);
		if (!symlinkError)
		{
			Saver symlinkSaver(symlinkPath, {"value"}, 1, true, output, error);
			expect(!symlinkSaver.save({{std::string("blocked")}}), "Saver rejects a symlink target");
			expect(readFile(symlinkDestination) == "destination\n", "symlink rejection preserves its destination");
			expect(!hasSaverTemporaryArtifact(root), "symlink rejection leaves no temporary artifact");
		}
	}

	void testFinishExecutionDoesNotBlock()
	{
		const auto start = std::chrono::steady_clock::now();
		expect(finishExecution(0.0, 3, 7) == 7, "zero waiting time returns the exit code");
		std::istringstream pipedInput("not consumed\n");
		std::ostringstream noninteractiveOutput;
		expect(finishExecution(std::numeric_limits<double>::infinity(), 3, 8, false, pipedInput, noninteractiveOutput) == 8, "infinity is non-blocking in a noninteractive test");
		expect(pipedInput.peek() == 'n' && noninteractiveOutput.str().empty(), "noninteractive infinity neither reads nor prompts");
		const auto elapsed = std::chrono::steady_clock::now() - start;
		expect(elapsed < std::chrono::seconds(2), "zero and noninteractive infinity return promptly");

		std::istringstream enter("\n");
		std::ostringstream interactiveOutput;
		expect(finishExecution(std::numeric_limits<double>::infinity(), 3, 9, true, enter, interactiveOutput) == 9, "interactive infinity returns after Enter");
		expect(interactiveOutput.str().find("Press Enter") != std::string::npos && enter.peek() == std::char_traits<char>::eof(), "interactive infinity prompts and consumes Enter");

		std::ostringstream countdownOutput;
		const auto countdownStart = std::chrono::steady_clock::now();
		expect(finishExecution(0.02, 3, 10, false, pipedInput, countdownOutput) == 10, "finite positive waiting time returns the exit code");
		const auto countdownElapsed = std::chrono::steady_clock::now() - countdownStart;
		expect(countdownElapsed >= std::chrono::milliseconds(10), "finite positive waiting time actually waits");
		expect(countdownOutput.str().find("Please wait") != std::string::npos && countdownOutput.str().find("automatic exit") != std::string::npos, "finite positive waiting time reports the countdown");
	}

	void testScopedOutputSilencer()
	{
		std::ostringstream output;
		output << "before";
		{
			ScopedOutputSilencer silencer(output, true);
			output << "hidden";
		}
		output << "after";
		expect(output.str() == "beforeafter", "enabled output silencer hides scoped output and restores the stream");

		std::ostringstream visibleOutput;
		{
			ScopedOutputSilencer silencer(visibleOutput, false);
			visibleOutput << "visible";
		}
		expect(visibleOutput.str() == "visible", "disabled output silencer leaves output visible");

		std::ostringstream largeOutput;
		{
			ScopedOutputSilencer silencer(largeOutput, true);
			largeOutput << std::string(4U * 1024U * 1024U, 'x');
		}
		expect(largeOutput.str().empty(), "large suppressed output is discarded instead of retained");

		std::ostringstream unwoundOutput;
		unwoundOutput << "before";
		try
		{
			ScopedOutputSilencer silencer(unwoundOutput, true);
			unwoundOutput << "hidden";
			throw std::runtime_error("test unwind");
		}
		catch (const std::runtime_error &)
		{
		}
		unwoundOutput << "after";
		expect(unwoundOutput.str() == "beforeafter", "output silencer restores the stream during exception unwinding");
	}

	void testLegacyRandomShufflePreservesPermutation()
	{
		std::vector<int> values = {5, 1, 5, 2, 9, 2, 7};
		std::vector<int> expected = values;
		std::srand(12345);
		legacyRandomShuffle(values.begin(), values.end());
		std::sort(values.begin(), values.end());
		std::sort(expected.begin(), expected.end());
		expect(values == expected, "legacy random shuffle preserves every permutation element");
	}
}

int main()
{
	const TemporaryDirectory temporaryDirectory;
	const std::filesystem::path &root = temporaryDirectory.path();

	testParserDefaultsAndAliases(root);
	testParserPathsAndValidation(root);
	testExecutablePathResolution(root);
	testSaver(root);
	testFinishExecutionDoesNotBlock();
	testScopedOutputSilencer();
	testLegacyRandomShufflePreservesPermutation();

	if (failures != 0)
	{
		std::cerr << failures << " test(s) failed.\n";
		return 1;
	}
	std::cout << "PASS: ParserSaver C++ tests.\n";
	return 0;
}
