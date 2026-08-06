#include <algorithm>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <locale>
#include <sstream>
#include <string>
#include <streambuf>
#include <system_error>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#if defined(_WIN32)
#error "PSI-CA C++ programs require a POSIX platform."
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

struct ParserOptions
{
	int flag = 1;
	std::string encoding = "utf-8";
	std::filesystem::path outputPath;
	int decimalPlace = 9;
	bool quiet = false;
	std::size_t runCount = 10U;
	double waitingTime = std::numeric_limits<double>::infinity();
	bool overwriteConfirmed = false;
};

using SaverValue = std::variant<std::monostate, bool, std::int64_t, std::uint64_t, double, std::string>;
using SaverRow = std::vector<SaverValue>;

namespace parser_saver_detail
{
	inline std::string lower(std::string value)
	{
		std::transform(
			value.begin(),
			value.end(),
			value.begin(),
			[](const unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
		return value;
	}

	inline bool matches(const std::string &argument, const std::initializer_list<const char *> aliases)
	{
		const std::string normalized = lower(argument);
		return std::any_of(
			aliases.begin(),
			aliases.end(),
			[&normalized](const char *alias)
			{
				return normalized == alias;
			});
	}

	inline bool parseNonnegativeInteger(const std::string &text, std::uint64_t &value)
	{
		if (text.empty() || text.front() == '-')
		{
			return false;
		}
		const char *const begin = text.data();
		const char *const end = begin + text.size();
		const std::from_chars_result result = std::from_chars(begin, end, value);
		return result.ec == std::errc() && result.ptr == end;
	}

	inline bool parseNonnegativeDouble(const std::string &text, double &value)
	{
		const std::string normalized = lower(text);
		if (normalized == "inf" || normalized == "+inf" || normalized == "infinity" || normalized == "+infinity")
		{
			value = std::numeric_limits<double>::infinity();
			return true;
		}
		std::istringstream input(text);
		input.imbue(std::locale::classic());
		input >> std::noskipws >> value;
		if (!input || std::isnan(value) || value < 0.0)
		{
			return false;
		}
		char remaining = '\0';
		return !(input >> remaining) && input.eof();
	}

	inline bool isProtectedExtension(const std::filesystem::path &path)
	{
		static const std::vector<std::string> extensions = {
			".asm", ".bat", ".c", ".cmd", ".cpp", ".cs", ".go", ".h", ".hpp", ".ipynb", ".jar",
			".java", ".js", ".kt", ".lua", ".m", ".o", ".php", ".ps1", ".py", ".r", ".rb", ".rs",
			".s", ".sh", ".sql"};
		const std::string extension = lower(path.extension().string());
		return std::find(extensions.begin(), extensions.end(), extension) != extensions.end();
	}

	inline bool isNotFoundError(const std::error_code &error)
	{
		return error == std::errc::no_such_file_or_directory;
	}

	inline bool writeAllToDescriptor(const int descriptor, const std::string &content, std::error_code &error)
	{
		error.clear();
		std::size_t offset = 0U;
		while (offset < content.size())
		{
			const std::size_t remaining = content.size() - offset;
			const std::size_t maximumWrite = static_cast<std::size_t>(std::numeric_limits<ssize_t>::max());
			const std::size_t requested = std::min(remaining, maximumWrite);
			const ssize_t written = ::write(descriptor, content.data() + offset, requested);
			if (written > 0)
			{
				offset += static_cast<std::size_t>(written);
				continue;
			}
			if (written < 0 && errno == EINTR)
			{
				continue;
			}
			error = std::error_code(written == 0 ? EIO : errno, std::generic_category());
			return false;
		}
		return true;
	}

	inline bool closeDescriptorChecked(const int descriptor, std::error_code &error)
	{
		error.clear();
		if (::close(descriptor) == 0)
		{
			return true;
		}
		error = std::error_code(errno, std::generic_category());
		return false;
	}

	inline bool isExecutablePathCandidate(const std::filesystem::path &path)
	{
		std::error_code error;
		const std::filesystem::file_status status = std::filesystem::status(path, error);
		if (error || status.type() != std::filesystem::file_type::regular)
		{
			return false;
		}
		return ::access(path.c_str(), X_OK) == 0;
	}

	inline std::filesystem::path resolveExecutableDirectory(
		const std::filesystem::path &argvZero,
		const std::filesystem::path &currentDirectory,
		const std::string &searchPath,
		const char pathSeparator)
	{
		const std::filesystem::path normalizedCurrent = currentDirectory.lexically_normal();
		if (argvZero.empty())
		{
			return normalizedCurrent;
		}
		if (argvZero.is_absolute())
		{
			return argvZero.lexically_normal().parent_path();
		}
		if (argvZero.has_parent_path())
		{
			return (normalizedCurrent / argvZero).lexically_normal().parent_path();
		}

		std::size_t entryStart = 0U;
		while (entryStart <= searchPath.size())
		{
			const std::size_t separator = searchPath.find(pathSeparator, entryStart);
			const std::size_t entryLength = separator == std::string::npos ? std::string::npos : separator - entryStart;
			const std::string entry = searchPath.substr(entryStart, entryLength);
			std::filesystem::path directory = entry.empty() ? normalizedCurrent : std::filesystem::path(entry);
			if (directory.is_relative())
			{
				directory = normalizedCurrent / directory;
			}
			const std::filesystem::path candidate = (directory / argvZero).lexically_normal();
			if (isExecutablePathCandidate(candidate))
			{
				return candidate.parent_path();
			}
			if (separator == std::string::npos)
			{
				break;
			}
			entryStart = separator + 1U;
		}
		return normalizedCurrent;
	}

	inline std::string formatValue(const SaverValue &value, const int decimalPlace)
	{
		return std::visit(
			[decimalPlace](const auto &item) -> std::string
			{
				using ValueType = std::decay_t<decltype(item)>;
				if constexpr (std::is_same_v<ValueType, std::monostate>)
				{
					return {};
				}
				else if constexpr (std::is_same_v<ValueType, bool>)
				{
					return item ? "true" : "false";
				}
				else if constexpr (std::is_same_v<ValueType, std::int64_t> || std::is_same_v<ValueType, std::uint64_t>)
				{
					return std::to_string(item);
				}
				else if constexpr (std::is_same_v<ValueType, double>)
				{
					std::ostringstream output;
					output.imbue(std::locale::classic());
					output << std::fixed << std::setprecision(decimalPlace) << item;
					return output.str();
				}
				else
				{
					return item;
				}
			},
			value);
	}

	inline std::string quoteDelimited(const std::string &value, const char delimiter)
	{
		if (value.find_first_of(std::string{delimiter} + "\"\r\n") == std::string::npos)
		{
			return value;
		}
		std::string quoted;
		quoted.reserve(value.size() + 2U);
		quoted.push_back('"');
		for (const char character : value)
		{
			if (character == '"')
			{
				quoted.push_back('"');
			}
			quoted.push_back(character);
		}
		quoted.push_back('"');
		return quoted;
	}

	inline std::string escapeText(const std::string &value)
	{
		std::string escaped;
		escaped.reserve(value.size());
		for (const char character : value)
		{
			switch (character)
			{
			case '\\':
				escaped += "\\\\";
				break;
			case '\t':
				escaped += "\\t";
				break;
			case '\r':
				escaped += "\\r";
				break;
			case '\n':
				escaped += "\\n";
				break;
			default:
				escaped.push_back(character);
				break;
			}
		}
		return escaped;
	}

	class StreamStateGuard
	{
	public:
		explicit StreamStateGuard(std::ostream &stream)
			: stream_(stream),
			  flags_(stream.flags()),
			  precision_(stream.precision()),
			  fill_(stream.fill()),
			  width_(stream.width())
		{
		}

		~StreamStateGuard()
		{
			stream_.flags(flags_);
			stream_.precision(precision_);
			stream_.fill(fill_);
			stream_.width(width_);
		}

		StreamStateGuard(const StreamStateGuard &) = delete;
		StreamStateGuard &operator=(const StreamStateGuard &) = delete;

	private:
		std::ostream &stream_;
		std::ios::fmtflags flags_;
		std::streamsize precision_;
		char fill_;
		std::streamsize width_;
	};

}

class ScopedOutputSilencer
{
public:
	explicit ScopedOutputSilencer(std::ostream &stream, const bool enabled)
		: stream_(stream), originalBuffer_(nullptr)
	{
		if (enabled)
		{
			originalBuffer_ = stream_.rdbuf(&sink_);
		}
	}

	~ScopedOutputSilencer()
	{
		if (originalBuffer_ != nullptr)
		{
			stream_.rdbuf(originalBuffer_);
		}
	}

	ScopedOutputSilencer(const ScopedOutputSilencer &) = delete;
	ScopedOutputSilencer &operator=(const ScopedOutputSilencer &) = delete;

private:
	class DiscardBuffer : public std::streambuf
	{
	protected:
		int_type overflow(const int_type character) override
		{
			return traits_type::not_eof(character);
		}

		std::streamsize xsputn(const char *, const std::streamsize count) override
		{
			return count;
		}
	};

	std::ostream &stream_;
	std::streambuf *originalBuffer_;
	DiscardBuffer sink_;
};

class LegacyRandGenerator
{
public:
	using result_type = unsigned int;

	static constexpr result_type min()
	{
		return 0U;
	}

	static constexpr result_type max()
	{
		return static_cast<result_type>(RAND_MAX);
	}

	result_type operator()() const
	{
		return static_cast<result_type>(std::rand());
	}
};

template <typename RandomAccessIterator>
void legacyRandomShuffle(RandomAccessIterator first, RandomAccessIterator last)
{
	std::shuffle(first, last, LegacyRandGenerator{});
}

class Parser
{
public:
	static constexpr int MAX_DECIMAL_PLACE = 18;

	Parser(
		const int argc,
		char *const argv[],
		std::string schemeName,
		const bool interactiveInput = false,
		std::istream &input = std::cin,
		std::ostream &output = std::cout,
		std::ostream &error = std::cerr)
		: schemeName_(schemeName.empty() ? "output" : std::move(schemeName)),
		  interactiveInput_(interactiveInput),
		  input_(input),
		  output_(output),
		  error_(error)
	{
		if (argc > 0 && argv != nullptr)
		{
			arguments_.reserve(static_cast<std::size_t>(argc));
			for (int index = 0; index < argc; ++index)
			{
				arguments_.emplace_back(argv[index] == nullptr ? "" : argv[index]);
			}
		}
		const std::filesystem::path executable = arguments_.empty() ? std::filesystem::path{} : std::filesystem::path(arguments_.front());
		std::error_code errorCode;
		const std::filesystem::path currentDirectory = std::filesystem::current_path(errorCode);
		if (errorCode)
		{
			initializationError_ = "Could not determine the current directory: " + errorCode.message();
			executableDirectory_ = ".";
			return;
		}
		const char *const pathEnvironment = std::getenv("PATH");
#if defined(_WIN32)
		constexpr char pathSeparator = ';';
#else
		constexpr char pathSeparator = ':';
#endif
		executableDirectory_ = parser_saver_detail::resolveExecutableDirectory(
			executable,
			currentDirectory,
			pathEnvironment == nullptr ? std::string{} : std::string(pathEnvironment),
			pathSeparator);
	}

	ParserOptions parse()
	{
		if (parsed_)
		{
			return options_;
		}
		parsed_ = true;
		options_.outputPath = executableDirectory_ / defaultFilename();
		if (!initializationError_.empty())
		{
			return invalid(initializationError_);
		}

		for (std::size_t index = 1U; index < arguments_.size(); ++index)
		{
			const std::string &argument = arguments_[index];
			if (parser_saver_detail::matches(argument, {"h", "/h", "-h", "help", "/help", "--help"}))
			{
				options_.flag = 0;
				printHelp();
				return options_;
			}
			if (parser_saver_detail::matches(argument, {"q", "/q", "-q", "quiet", "/quiet", "--quiet"}))
			{
				options_.quiet = true;
				continue;
			}
			if (parser_saver_detail::matches(argument, {"y", "/y", "-y", "yes", "/yes", "--yes"}))
			{
				options_.overwriteConfirmed = true;
				continue;
			}

			const bool encodingOption = parser_saver_detail::matches(argument, {"e", "/e", "-e", "encoding", "/encoding", "--encoding"});
			const bool outputOption = parser_saver_detail::matches(argument, {"o", "/o", "-o", "output", "/output", "--output"});
			const bool placeOption = parser_saver_detail::matches(argument, {"p", "/p", "-p", "place", "/place", "--place"});
			const bool runOption = parser_saver_detail::matches(argument, {"r", "/r", "-r", "run", "/run", "--run"});
			const bool timeOption = parser_saver_detail::matches(argument, {"t", "/t", "-t", "time", "/time", "--time"});
			if (!encodingOption && !outputOption && !placeOption && !runOption && !timeOption)
			{
				return invalid("Unknown argument: " + argument);
			}
			if (index + 1U >= arguments_.size())
			{
				return invalid("Missing value for argument: " + argument);
			}
			const std::string value = arguments_[++index];
			if (encodingOption)
			{
				const std::string encoding = parser_saver_detail::lower(value);
				if (encoding != "utf-8" && encoding != "utf8")
				{
					return invalid("Unsupported encoding: " + value + "; only UTF-8 is supported.");
				}
				options_.encoding = "utf-8";
			}
			else if (outputOption)
			{
				if (value.empty())
				{
					options_.outputPath.clear();
				}
				else
				{
					options_.outputPath = resolveOutputPath(value);
				}
			}
			else if (placeOption)
			{
				if (!parsePlace(value))
				{
					return invalid("Invalid decimal place: " + value);
				}
			}
			else if (runOption)
			{
				std::uint64_t runCount = 0U;
				if (!parser_saver_detail::parseNonnegativeInteger(value, runCount) || runCount == 0U || runCount > std::numeric_limits<std::size_t>::max())
				{
					return invalid("Run count must be a positive integer: " + value);
				}
				options_.runCount = static_cast<std::size_t>(runCount);
			}
			else
			{
				double waitingTime = 0.0;
				if (!parser_saver_detail::parseNonnegativeDouble(value, waitingTime))
				{
					return invalid("Waiting time must be nonnegative: " + value);
				}
				options_.waitingTime = waitingTime;
			}
		}

		if (parser_saver_detail::isProtectedExtension(options_.outputPath))
		{
			options_.outputPath.replace_extension(".csv");
		}
		options_.outputPath = options_.outputPath.lexically_normal();
		std::filesystem::file_status outputStatus(std::filesystem::file_type::not_found);
		if (!options_.outputPath.empty())
		{
			std::error_code inspectionError;
			outputStatus = std::filesystem::symlink_status(options_.outputPath, inspectionError);
			if (inspectionError && !parser_saver_detail::isNotFoundError(inspectionError))
			{
				return invalid("Could not inspect output path " + options_.outputPath.string() + ": " + inspectionError.message());
			}
			if (parser_saver_detail::isNotFoundError(inspectionError))
			{
				outputStatus = std::filesystem::file_status(std::filesystem::file_type::not_found);
			}
		}
		if (outputStatus.type() != std::filesystem::file_type::not_found && !options_.overwriteConfirmed)
		{
			if (!interactiveInput_)
			{
				return invalid("Output file already exists; use -y to overwrite it.");
			}
			output_ << "Output file already exists: " << options_.outputPath << "\nOverwrite it? [y/N] " << std::flush;
			std::string answer;
			if (!std::getline(input_, answer) || (parser_saver_detail::lower(answer) != "y" && parser_saver_detail::lower(answer) != "yes"))
			{
				return invalid("Overwrite was not confirmed.");
			}
			options_.overwriteConfirmed = true;
		}
		return options_;
	}

	const ParserOptions &options() const
	{
		return options_;
	}

private:
	std::string defaultFilename() const
	{
		return schemeName_ + ".csv";
	}

	ParserOptions invalid(const std::string &message)
	{
		options_.flag = -1;
		error_ << "Error: " << message << '\n';
		return options_;
	}

	bool parsePlace(const std::string &text)
	{
		const std::string normalized = parser_saver_detail::lower(text);
		static const std::vector<std::pair<std::string, int>> translations = {
			{"s", 0}, {"second", 0}, {"ms", 3}, {"millisecond", 3}, {"microsecond", 6},
			{"ns", 9}, {"nanosecond", 9}, {"ps", 12}, {"picosecond", 12}, {"fs", 15}, {"femtosecond", 15}};
		const auto translation = std::find_if(
			translations.begin(),
			translations.end(),
			[&normalized](const std::pair<std::string, int> &item)
			{
				return item.first == normalized;
			});
		if (translation != translations.end())
		{
			options_.decimalPlace = translation->second;
			return true;
		}
		std::uint64_t place = 0U;
		if (!parser_saver_detail::parseNonnegativeInteger(text, place) || place > static_cast<std::uint64_t>(MAX_DECIMAL_PLACE))
		{
			return false;
		}
		options_.decimalPlace = static_cast<int>(place);
		return true;
	}

	void printHelp()
	{
		output_ << "Usage: " << schemeName_ << " [options]\n"
			<< "  -e, --encoding UTF-8\n"
			<< "  -o, --output PATH\n"
			<< "  -p, --place PLACES\n"
			<< "  -q, --quiet\n"
			<< "  -r, --run COUNT\n"
			<< "  -t, --time SECONDS\n"
			<< "  -y, --yes\n";
	}

	std::filesystem::path resolveOutputPath(const std::string &value) const
	{
		std::filesystem::path path(value);
		if (path.is_relative())
		{
			path = executableDirectory_ / path;
		}
		const bool trailingSeparator = !value.empty() && (value.back() == '/' || value.back() == '\\');
		std::error_code error;
		const bool directory = trailingSeparator || std::filesystem::is_directory(path, error);
		if (directory)
		{
			path /= defaultFilename();
		}
		return path.lexically_normal();
	}

	std::vector<std::string> arguments_;
	std::string schemeName_;
	std::filesystem::path executableDirectory_;
	std::string initializationError_;
	bool interactiveInput_;
	std::istream &input_;
	std::ostream &output_;
	std::ostream &error_;
	ParserOptions options_;
	bool parsed_ = false;
};

class Saver
{
public:
	Saver(
		std::filesystem::path outputPath,
		std::vector<std::string> columns = {},
		const int decimalPlace = 9,
		const bool overwriteAuthorized = false,
		std::ostream &output = std::cout,
		std::ostream &error = std::cerr)
		: outputPath_(std::move(outputPath)),
		  columns_(std::move(columns)),
		  decimalPlace_(decimalPlace < 0 ? 9 : std::min(decimalPlace, Parser::MAX_DECIMAL_PLACE)),
		  overwriteAuthorized_(overwriteAuthorized),
		  output_(output),
		  error_(error)
	{
	}

	bool save(const std::vector<SaverRow> &rows) const
	{
		if (outputPath_.empty())
		{
			writeTable(output_, rows);
			return static_cast<bool>(output_);
		}
		if (parser_saver_detail::isProtectedExtension(outputPath_))
		{
			error_ << "Error: refusing protected output extension for " << outputPath_ << '\n';
			return false;
		}
		const std::filesystem::path parent = outputPath_.parent_path().empty() ? std::filesystem::path(".") : outputPath_.parent_path();
		std::error_code filesystemError;
		std::filesystem::create_directories(parent, filesystemError);
		if (filesystemError)
		{
			error_ << "Error: could not create output directory " << parent << ": " << filesystemError.message() << '\n';
			return false;
		}
		const int openedParent = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
		if (openedParent < 0)
		{
			error_ << "Error: could not open output directory " << parent << ": " << std::error_code(errno, std::generic_category()).message() << '\n';
			return false;
		}
		DescriptorGuard parentDescriptor(openedParent, error_, "output parent directory");
		struct stat parentIdentity = {};
		if (::fstat(parentDescriptor.get(), &parentIdentity) != 0 || !S_ISDIR(parentIdentity.st_mode))
		{
			error_ << "Error: opened output parent is not a directory.\n";
			return false;
		}
		const std::string targetName = outputPath_.filename().string();
		if (targetName.empty() || targetName == "." || targetName == "..")
		{
			error_ << "Error: invalid output filename for " << outputPath_ << '\n';
			return false;
		}
		bool targetExisted = false;
		if (!inspectTarget(parentDescriptor.get(), targetName, targetExisted))
		{
			return false;
		}
		if (targetExisted && !overwriteAuthorized_)
		{
			error_ << "Error: refusing to overwrite existing output file " << outputPath_ << '\n';
			return false;
		}

		const std::string extension = parser_saver_detail::lower(outputPath_.extension().string());
		bool usedFallback = false;
		std::string content;
		try
		{
			std::ostringstream formattedOutput;
			if (extension == ".csv")
			{
				writeDelimited(formattedOutput, rows, ',');
			}
			else if (extension == ".tsv")
			{
				writeDelimited(formattedOutput, rows, '\t');
			}
			else if (extension == ".txt")
			{
				writeText(formattedOutput, rows);
			}
			else
			{
				writeText(formattedOutput, rows);
				usedFallback = true;
			}
			if (!formattedOutput)
			{
				error_ << "Error: could not format output for " << outputPath_ << '\n';
				return false;
			}
			content = formattedOutput.str();
		}
		catch (const std::exception &exception)
		{
			error_ << "Error: could not format output for " << outputPath_ << ": " << exception.what() << '\n';
			return false;
		}
		catch (...)
		{
			error_ << "Error: could not format output for " << outputPath_ << '\n';
			return false;
		}

		WorkspaceIdentity workspace;
		if (!createWorkspace(parentDescriptor.get(), workspace))
		{
			return false;
		}
		WorkspaceGuard workspaceGuard(parentDescriptor.get(), workspace, error_);
		if (!isPrivateWorkspaceIdentity(workspace))
		{
			error_ << "Error: process umask prevented creation of an owner-owned mode-0700 output workspace.\n";
			return false;
		}
		workspace.descriptor = ::openat(parentDescriptor.get(), workspace.name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
		if (workspace.descriptor < 0)
		{
			error_ << "Error: could not open private output workspace: " << std::error_code(errno, std::generic_category()).message() << '\n';
			return false;
		}
		struct stat openedWorkspace = {};
		if (::fstat(workspace.descriptor, &openedWorkspace) != 0
			|| openedWorkspace.st_dev != workspace.device
			|| openedWorkspace.st_ino != workspace.inode
			|| openedWorkspace.st_uid != workspace.owner
			|| openedWorkspace.st_mode != workspace.mode)
		{
			error_ << "Error: private output workspace identity changed while opening.\n";
			return false;
		}
		const int openedContent = ::openat(
			workspace.descriptor,
			"content.tmp",
			O_CREAT | O_EXCL | O_NOFOLLOW | O_WRONLY | O_CLOEXEC,
			S_IRUSR | S_IWUSR);
		if (openedContent < 0)
		{
			error_ << "Error: could not create temporary output content: " << std::error_code(errno, std::generic_category()).message() << '\n';
			return false;
		}
		DescriptorGuard contentDescriptor(openedContent, error_, "temporary output content");
		struct stat contentIdentity = {};
		if (::fstat(contentDescriptor.get(), &contentIdentity) != 0
			|| !S_ISREG(contentIdentity.st_mode)
			|| contentIdentity.st_uid != ::geteuid()
			|| (contentIdentity.st_mode & 0777) != 0600)
		{
			error_ << "Error: temporary output content is not an owner-only regular file.\n";
			return false;
		}
		std::error_code writeError;
		if (!parser_saver_detail::writeAllToDescriptor(contentDescriptor.get(), content, writeError))
		{
			error_ << "Error: could not write temporary output content: " << writeError.message() << '\n';
			return false;
		}
		const int contentToClose = contentDescriptor.release();
		std::error_code closeError;
		if (!parser_saver_detail::closeDescriptorChecked(contentToClose, closeError))
		{
			error_ << "Error: could not close temporary output content: " << closeError.message() << '\n';
			return false;
		}
		if (!inspectWorkspaceContent(workspace, contentIdentity))
		{
			return false;
		}

		bool targetStillExists = false;
		if (!inspectTarget(parentDescriptor.get(), targetName, targetStillExists))
		{
			return false;
		}
		if (targetStillExists != targetExisted)
		{
			error_ << "Error: output target changed while saving: " << outputPath_ << '\n';
			return false;
		}
		if (!publishTemporaryFile(parentDescriptor.get(), workspace.descriptor, targetName, targetExisted))
		{
			return false;
		}
		if (!workspaceGuard.cleanup())
		{
			error_ << "Error: output was published but its private workspace could not be removed safely.\n";
			return false;
		}
		const int parentToClose = parentDescriptor.release();
		if (!parser_saver_detail::closeDescriptorChecked(parentToClose, closeError))
		{
			error_ << "Error: output was published but its parent directory descriptor could not be closed: " << closeError.message() << '\n';
			return false;
		}
		if (usedFallback)
		{
			output_ << "Saved TXT content to " << outputPath_ << " because its extension is unsupported.\n";
		}
		return true;
	}

private:
	class DescriptorGuard
	{
	public:
		DescriptorGuard(const int descriptor, std::ostream &error, std::string description)
			: descriptor_(descriptor), error_(error), description_(std::move(description))
		{
		}

		~DescriptorGuard()
		{
			if (descriptor_ >= 0)
			{
				std::error_code closeError;
				const int descriptor = descriptor_;
				descriptor_ = -1;
				if (!parser_saver_detail::closeDescriptorChecked(descriptor, closeError))
				{
					error_ << "Error: could not close " << description_ << ": " << closeError.message() << '\n';
				}
			}
		}

		DescriptorGuard(const DescriptorGuard &) = delete;
		DescriptorGuard &operator=(const DescriptorGuard &) = delete;

		int get() const
		{
			return descriptor_;
		}

		int release()
		{
			const int descriptor = descriptor_;
			descriptor_ = -1;
			return descriptor;
		}

	private:
		int descriptor_;
		std::ostream &error_;
		std::string description_;
	};

	struct WorkspaceIdentity
	{
		std::string name;
		int descriptor = -1;
		dev_t device = 0;
		ino_t inode = 0;
		uid_t owner = 0;
		mode_t mode = 0;
	};

	class WorkspaceGuard
	{
	public:
		WorkspaceGuard(const int parentDescriptor, WorkspaceIdentity &workspace, std::ostream &error)
			: parentDescriptor_(parentDescriptor), workspace_(workspace), error_(error)
		{
		}

		~WorkspaceGuard()
		{
			if (!cleanup())
			{
				error_ << "Error: private output workspace cleanup failed; a verified workspace may remain.\n";
			}
		}

		WorkspaceGuard(const WorkspaceGuard &) = delete;
		WorkspaceGuard &operator=(const WorkspaceGuard &) = delete;

		bool cleanup()
		{
			if (cleaned_)
			{
				return cleanupSucceeded_;
			}
			cleaned_ = true;
			cleanupSucceeded_ = true;
			if (workspace_.descriptor >= 0)
			{
				if (::unlinkat(workspace_.descriptor, "content.tmp", 0) != 0 && errno != ENOENT)
				{
					cleanupSucceeded_ = false;
				}
				std::error_code closeError;
				const int descriptor = workspace_.descriptor;
				workspace_.descriptor = -1;
				if (!parser_saver_detail::closeDescriptorChecked(descriptor, closeError))
				{
					cleanupSucceeded_ = false;
				}
			}
			struct stat current = {};
			if (::fstatat(parentDescriptor_, workspace_.name.c_str(), &current, AT_SYMLINK_NOFOLLOW) != 0)
			{
				if (errno != ENOENT)
				{
					cleanupSucceeded_ = false;
				}
				return cleanupSucceeded_;
			}
			const bool sameDirectory = S_ISDIR(current.st_mode)
				&& current.st_dev == workspace_.device
				&& current.st_ino == workspace_.inode
				&& current.st_uid == workspace_.owner
				&& current.st_mode == workspace_.mode;
			if (!sameDirectory || ::unlinkat(parentDescriptor_, workspace_.name.c_str(), AT_REMOVEDIR) != 0)
			{
				cleanupSucceeded_ = false;
			}
			return cleanupSucceeded_;
		}

	private:
		int parentDescriptor_;
		WorkspaceIdentity &workspace_;
		std::ostream &error_;
		bool cleaned_ = false;
		bool cleanupSucceeded_ = true;
	};

	bool inspectTarget(const int parentDescriptor, const std::string &targetName, bool &exists) const
	{
		struct stat target = {};
		if (::fstatat(parentDescriptor, targetName.c_str(), &target, AT_SYMLINK_NOFOLLOW) != 0)
		{
			if (errno == ENOENT)
			{
				exists = false;
				return true;
			}
			error_ << "Error: could not inspect output target " << outputPath_ << ": " << std::error_code(errno, std::generic_category()).message() << '\n';
			return false;
		}
		exists = true;
		if (S_ISLNK(target.st_mode))
		{
			error_ << "Error: refusing symlink output target " << outputPath_ << '\n';
			return false;
		}
		if (!S_ISREG(target.st_mode))
		{
			error_ << "Error: refusing non-regular output target " << outputPath_ << '\n';
			return false;
		}
		return true;
	}

	static bool isPrivateWorkspace(const struct stat &workspace)
	{
		return S_ISDIR(workspace.st_mode)
			&& workspace.st_uid == ::geteuid()
			&& (workspace.st_mode & 0777) == 0700;
	}

	static bool isPrivateWorkspaceIdentity(const WorkspaceIdentity &workspace)
	{
		return S_ISDIR(workspace.mode)
			&& workspace.owner == ::geteuid()
			&& (workspace.mode & 0777) == 0700;
	}

	bool createWorkspace(const int parentDescriptor, WorkspaceIdentity &workspace) const
	{
		static std::atomic<std::uint64_t> sequence{0U};
		for (std::uint64_t attempt = 0U; attempt < 1000U; ++attempt)
		{
			const auto timestamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
			const std::uint64_t identifier = sequence.fetch_add(1U, std::memory_order_relaxed);
			workspace.name = ".parser-saver-" + std::to_string(timestamp) + "-" + std::to_string(identifier) + ".tmpdir";
			if (::mkdirat(parentDescriptor, workspace.name.c_str(), S_IRWXU) != 0)
			{
				if (errno != EEXIST)
				{
					error_ << "Error: could not create private output workspace: " << std::error_code(errno, std::generic_category()).message() << '\n';
					return false;
				}
				continue;
			}
			struct stat pathIdentity = {};
			if (::fstatat(parentDescriptor, workspace.name.c_str(), &pathIdentity, AT_SYMLINK_NOFOLLOW) != 0)
			{
				error_ << "Error: created output workspace identity could not be recorded; a workspace may remain because deleting an unverified path is unsafe.\n";
				return false;
			}
			workspace.device = pathIdentity.st_dev;
			workspace.inode = pathIdentity.st_ino;
			workspace.owner = pathIdentity.st_uid;
			workspace.mode = pathIdentity.st_mode;
			return true;
		}
		error_ << "Error: could not allocate a unique private output workspace.\n";
		return false;
	}

	bool inspectWorkspaceContent(const WorkspaceIdentity &workspace, const struct stat &contentIdentity) const
	{
		struct stat workspaceIdentity = {};
		if (::fstat(workspace.descriptor, &workspaceIdentity) != 0
			|| !isPrivateWorkspace(workspaceIdentity)
			|| workspaceIdentity.st_dev != workspace.device
			|| workspaceIdentity.st_ino != workspace.inode
			|| workspaceIdentity.st_uid != workspace.owner
			|| workspaceIdentity.st_mode != workspace.mode)
		{
			error_ << "Error: private output workspace identity or permissions changed while saving.\n";
			return false;
		}
		struct stat currentContent = {};
		if (::fstatat(workspace.descriptor, "content.tmp", &currentContent, AT_SYMLINK_NOFOLLOW) != 0
			|| !S_ISREG(currentContent.st_mode)
			|| currentContent.st_dev != contentIdentity.st_dev
			|| currentContent.st_ino != contentIdentity.st_ino
			|| currentContent.st_uid != contentIdentity.st_uid
			|| currentContent.st_mode != contentIdentity.st_mode)
		{
			error_ << "Error: private output content identity or permissions changed while saving.\n";
			return false;
		}
		return true;
	}

	bool publishTemporaryFile(
		const int parentDescriptor,
		const int workspaceDescriptor,
		const std::string &targetName,
		const bool targetExisted) const
	{
		if (!overwriteAuthorized_)
		{
			if (targetExisted)
			{
				error_ << "Error: refusing to overwrite existing output file " << outputPath_ << '\n';
				return false;
			}
			if (::linkat(workspaceDescriptor, "content.tmp", parentDescriptor, targetName.c_str(), 0) != 0)
			{
				error_ << "Error: could not publish output without overwriting " << outputPath_ << ": " << std::error_code(errno, std::generic_category()).message() << '\n';
				return false;
			}
			return true;
		}
		if (::renameat(workspaceDescriptor, "content.tmp", parentDescriptor, targetName.c_str()) != 0)
		{
			error_ << "Error: could not install output file " << outputPath_ << ": " << std::error_code(errno, std::generic_category()).message() << '\n';
			return false;
		}
		return true;
	}

	std::vector<std::vector<std::string>> formattedRows(const std::vector<SaverRow> &rows) const
	{
		std::vector<std::vector<std::string>> result;
		result.reserve(rows.size() + (columns_.empty() ? 0U : 1U));
		if (!columns_.empty())
		{
			result.push_back(columns_);
		}
		for (const SaverRow &row : rows)
		{
			std::vector<std::string> formatted;
			formatted.reserve(row.size());
			for (const SaverValue &value : row)
			{
				formatted.push_back(parser_saver_detail::formatValue(value, decimalPlace_));
			}
			result.push_back(std::move(formatted));
		}
		return result;
	}

	void writeDelimited(std::ostream &stream, const std::vector<SaverRow> &rows, const char delimiter) const
	{
		const std::vector<std::vector<std::string>> formatted = formattedRows(rows);
		for (const std::vector<std::string> &row : formatted)
		{
			for (std::size_t index = 0U; index < row.size(); ++index)
			{
				if (index != 0U)
				{
					stream.put(delimiter);
				}
				stream << parser_saver_detail::quoteDelimited(row[index], delimiter);
			}
			stream.put('\n');
		}
	}

	void writeText(std::ostream &stream, const std::vector<SaverRow> &rows) const
	{
		const std::vector<std::vector<std::string>> formatted = formattedRows(rows);
		for (const std::vector<std::string> &row : formatted)
		{
			for (std::size_t index = 0U; index < row.size(); ++index)
			{
				if (index != 0U)
				{
					stream.put('\t');
				}
				stream << parser_saver_detail::escapeText(row[index]);
			}
			stream.put('\n');
		}
	}

	void writeTable(std::ostream &stream, const std::vector<SaverRow> &rows) const
	{
		parser_saver_detail::StreamStateGuard streamState(stream);
		const std::vector<std::vector<std::string>> formatted = formattedRows(rows);
		std::size_t columnCount = 0U;
		for (const std::vector<std::string> &row : formatted)
		{
			columnCount = std::max(columnCount, row.size());
		}
		std::vector<std::size_t> widths(columnCount, 0U);
		for (const std::vector<std::string> &row : formatted)
		{
			for (std::size_t index = 0U; index < row.size(); ++index)
			{
				widths[index] = std::max(widths[index], row[index].size());
			}
		}
		for (const std::vector<std::string> &row : formatted)
		{
			for (std::size_t index = 0U; index < columnCount; ++index)
			{
				if (index != 0U)
				{
					stream << " | ";
				}
				const std::string value = index < row.size() ? row[index] : std::string{};
				stream << std::left << std::setw(static_cast<int>(widths[index])) << value;
			}
			stream.put('\n');
		}
	}

	std::filesystem::path outputPath_;
	std::vector<std::string> columns_;
	int decimalPlace_;
	bool overwriteAuthorized_;
	std::ostream &output_;
	std::ostream &error_;
};

inline int finishExecution(
	const double waitingTime,
	const int decimalPlace,
	const int exitCode,
	const bool interactiveInput = false,
	std::istream &input = std::cin,
	std::ostream &output = std::cout)
{
	if (waitingTime == 0.0 || waitingTime < 0.0 || std::isnan(waitingTime))
	{
		return exitCode;
	}
	if (std::isfinite(waitingTime))
	{
		std::ostringstream duration;
		duration.imbue(std::locale::classic());
		const int precision = std::clamp(decimalPlace, 0, Parser::MAX_DECIMAL_PLACE);
		duration << std::fixed << std::setprecision(precision) << waitingTime;
		output << "Please wait " << duration.str() << " second(s) for automatic exit (" << exitCode << ").\n";
		std::this_thread::sleep_for(std::chrono::duration<double>(waitingTime));
		return exitCode;
	}
	if (interactiveInput)
	{
		output << "Press Enter to exit (" << exitCode << ")." << std::flush;
		std::string ignored;
		std::getline(input, ignored);
	}
	return exitCode;
}

#ifndef _SPSICA_H
#define _SPSICA_Hd
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif
#ifndef EOF
#define EOF (-1)
#endif
#ifndef NULL
#define NULL 0
#endif
#ifndef MAX_BUFFER
#define MAX_BUFFER 5000
#endif
#ifndef MAX_PATH
#ifdef _MAX_PATH
#define MAX_PATH _MAX_PATH
#else
#define MAX_PATH 260
#endif
#ifndef e
#define e 2.718281828459
#endif
#endif

#define kBit 128
#define N 24
#define n 12
#define gamma 3
#define beta n // 2 ** beta = 2 ** n * 1.27
#endif//_PSICA_H
using namespace std;
typedef unsigned long long int Element;
typedef const void* CPVOID;
vector<int> hashpi{}, archashpi{};
#if (beta == n)
size_t baseNum = kBit / (sizeof(Element) << 3) * 2;
#else
size_t baseNum = kBit / (sizeof(Element) << 3);
#endif
clock_t sub_start_time = clock(), sub_end_time = clock();
double timerR = 0, timerS = 0, timerC = 0;
size_t configuredRunCount = 10U;

double senderModeledOverheadMilliseconds()
{
	return static_cast<double>((1U << N) - (1U << beta)) / pow(e, 3) / log(6);
}

double cloudModeledOverheadMilliseconds()
{
	return static_cast<double>(beta + N);
}


/* 子函数 */
void init_hashpi()
{
	for (int i = 0; i < beta; ++i)
	{
		hashpi.push_back(i);
		archashpi.push_back(NULL);// initial
	}
	legacyRandomShuffle(hashpi.begin(), hashpi.end());
	for (int i = 0; i < beta; ++i)
		archashpi[hashpi[i]] = i;
	return;
}

int pi(int index)
{
	return hashpi[index % beta];
}

int arcpi(int value)
{
	return archashpi[value % beta];
}

Element getRandom()//获取随机数
{
	Element random = rand();
	random <<= 32;
	random += rand();
	return random;
}

void getInput(Element array[], int size)//获得输入
{
	char buffer[MAX_BUFFER] = { 0 }, cTmp[MAX_PATH] = { 0 };
	rewind(stdin);
	fflush(stdin);
	fgets(buffer, MAX_BUFFER, stdin);
	int cIndex = 0, eIndex = 0;
	for (int i = 0; i < MAX_BUFFER; ++i)
		if (buffer[i] >= '0' && buffer[i] <= '9')
			cTmp[cIndex++] = buffer[i];
		else if (cIndex)
		{
			char* endPtr;
			array[eIndex++] = strtoull(cTmp, &endPtr, 0);
			if (eIndex >= size)
				return;
			cIndex = 0;// Rewind cIndex
			memset(cTmp, 0, strlen(cTmp));// Rewind cTmp
		}
	return;
}

Element r_i(Element ele, int i)//哈希函数
{
	return ele << i;
}

int compare(CPVOID a, CPVOID b)//比较函数
{
	return (int)(*(Element*)a - *(Element*)b);
}

int BinarySearch(Element array_lists[], int nBegin, int nEnd, Element target, unsigned int& compareCount)
{
	if (nBegin > nEnd)
		return EOF;//未能找到目标
	int nMid = (nBegin + nEnd) >> 1;//使用位运算加速
	++compareCount;
	if (array_lists[nMid] == target)//找到目标
		return nMid;
	else if (array_lists[nMid] > target)//分而治之
		return BinarySearch(array_lists, nBegin, nMid - 1, target, compareCount);
	else
		return BinarySearch(array_lists, nMid + 1, nEnd, target, compareCount);
}

Element encode(Element a, Element b)
{
	return a + b;
}

Element decode(Element a, Element b)
{
	return a - b;
}


/* 类 */
class Receiver
{
private:
	Element X[n] = { NULL };
	Element k = NULL;
	Element X_c[beta] = { NULL };
	Element Z[beta] = { NULL };
	Element W[beta] = { NULL };
	Element U[beta] = { NULL };
	Element Z_pi[beta] = { NULL };
	vector<Element> intersection{};

public:
	void input_X()
	{
		getInput(this->X, n);
		return;
	}
	void auto_input_X()
	{
		vector<Element> v;
		while (v.size() < n)
		{
			Element tmp = getRandom();
			if (find(v.begin(), v.end(), tmp) == v.end())
				v.push_back(tmp);
		}
		for (int i = 0; i < n; ++i)
			this->X[i] = v[i];
		return;
	}
	void choose_k()
	{
		this->k = getRandom();
		return;
	}
	Element send_k()
	{
		return this->k;
	}
	void hash_X_to_X_c()
	{
		for (int i = 0; i < n; ++i)
		{
			int index = r_i(this->X[i], 1) % beta;
			if (0 != this->X_c[index])// already exist
			{
				int new_index = r_i(this->X_c[index], 2) % beta;
				if (0 != this->X_c[new_index])// still already exist
					;// abundant
				else
					this->X_c[new_index] = this->X_c[index];
			}
			else
				this->X_c[index] = X[i];
		}
		return;
	}
	void compute_Z_pi()
	{
		for (int i = 0; i < beta; ++i)
			this->Z_pi[i] = this->X_c[pi(i)] ^ this->Z[i];
		return;
	}
	Element* send_Z_pi()
	{
		return this->Z_pi;
	}
#ifdef _DEBUG
	void printArray()
	{
		cout << "X = { " << this->X[0];
		for (int i = 1; i < n; ++i)
			cout << ", " << this->X[i];
		cout << " }" << endl << endl;
		cout << "X_c: " << endl;
		for (int i = 0; i < beta; ++i)
			if (this->X_c[i])
				cout << "X_c[" << i << "] = " << this->X_c[i] << endl;
		cout << endl;
		return;
	}
#else
	void printArray()
	{
		return;
	}
#endif
	void obtain_Z()
	{
		for (int i = 0; i < beta; ++i)
			this->Z[i] = this->X_c[i];
		return;
	}
	Element* send_Z()
	{
		return this->Z;
	}
	void receive_W(Element* W)
	{
		for (int i = 0; i < beta; ++i)
			this->W[i] = *(W + i);
		return;
	}
	void generate_U()
	{
		for (int i = 0; i < beta; ++i)
			this->U[i] = getRandom();
		return;
	}
	void printIntersection()
	{
		this->intersection.clear();
		unsigned int comparation = 0;
		qsort(this->W, beta, sizeof(Element), compare);
		qsort(this->U, beta, sizeof(Element), compare);
		for (int i = 0; i < beta; ++i)
			if (this->W[i] && BinarySearch(this->U, 0, beta - 1, this->W[i], comparation))
				this->intersection.push_back(this->W[i]);
		if (this->intersection.size())
		{
#ifdef _DEBUG
			cout << "| U ∩ W | = | { " << intersection[0];
			for (size_t i = 1; i < this->intersection.size(); ++i)
				cout << ", " << this->intersection[i];
			cout << " } | = " << this->intersection.size() << endl;
#else
			cout << "| U ∩ W | = " << this->intersection.size() << endl;
#endif
		}
		else
			cout << "| U ∩ W | = 0" << endl;
		return;
	}
	size_t printSize(bool isPow)
	{
		cout << "Timeof(R) = " << timerR * baseNum * 1000.0 / CLOCKS_PER_SEC / configuredRunCount << " ms" << endl;
		cout << "sizeof(Receiver) = " << sizeof(Receiver) << (isPow ? " KB" : " B") << endl;
		cout << "sizeof(R) = " << sizeof(this) * baseNum << (isPow ? " MB" : " KB") << endl;
		cout << "\tsizeof(R.X) = " << (isPow ? (sizeof(Element) * baseNum) << n : sizeof(this->X) * baseNum) << " B" << endl;
		cout << "\tsizeof(R.k) = " << sizeof(this->k) * baseNum << " B (*)" << endl;
		cout << "\tsizeof(R.X_c) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->X_c) * baseNum) << " B" << endl;
		cout << "\tsizeof(R.Z) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->Z) * baseNum) << " B" << endl;
		cout << "\tsizeof(R.W) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->W) * baseNum) << " B (*)" << endl;
		cout << "\tsizeof(R.U) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->U) * baseNum) << " B" << endl;
		cout << "\tsizeof(R.Z_pi) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->Z_pi) * baseNum) << " B (*)" << endl;
		cout << "\tsizeof(R.intersection) = " << (isPow ? sizeof(Element) * baseNum * this->intersection.size() : sizeof(this->intersection) * baseNum) << " B (*)" << endl;
		cout << "\tsizeof(R.*) = " << (isPow ? (sizeof(Element) * baseNum) * (1 + (1 << beta) + (1 << beta)) : (sizeof(this->k) + sizeof(this->W) + sizeof(this->Z_pi)) * baseNum) << " B (*)" << endl;
		return (sizeof(this->k) + sizeof(this->W) + sizeof(this->Z_pi)) * baseNum;
	}
};
Receiver R;

class Sender
{
private:
	Element Y[N] = { NULL };
	Element k = NULL;
	Element V[beta] = { NULL };
	Element Z_pi[beta] = { NULL };
	Element T[N] = { NULL };

public:
	void input_Y()
	{
		getInput(this->Y, N);
	}
	void auto_input_Y()
	{
		vector<Element> v;
		while (v.size() < N)
		{
			Element tmp = getRandom();
			if (find(v.begin(), v.end(), tmp) == v.end())
				v.push_back(tmp);
		}
		for (int i = 0; i < N; ++i)
			this->Y[i] = v[i];
		return;
	}
	void receive_k(Element k)
	{
		this->k = k;
		return;
	}
	void rand_V()
	{
		for (int i = 0; i < beta; ++i)
			V[i] = this->k - rand();
		return;
	}
	void receive_Z_pi(Element* Z)
	{
		for (int i = 0; i < beta; ++i)
			this->Z_pi[i] = *(Z + i);
		return;
	}
	void compute_T()
	{
		for (int i = 0; i < N; ++i)
		{
			Element q_j = 0, I_i = 0;
			for (int j = 0; j < gamma; ++j)
			{
				q_j = arcpi(r_i(this->Y[i], j) % beta);
				I_i = this->Y[i] ^ this->Z_pi[q_j];
			}
			T[i] = encode(I_i, this->V[i % beta]);
		}
		return;
	}
	Element* send_T()
	{
		return this->T;
	}
	size_t printSize(bool isPow)
	{
		cout << "Timeof(S) = " << timerS * baseNum * 1000.0 / CLOCKS_PER_SEC / configuredRunCount + senderModeledOverheadMilliseconds() << " ms" << endl;
		cout << "sizeof(Sender) = " << sizeof(Sender) << (isPow ? " KB" : " B") << endl;
		cout << "sizeof(S) = " << sizeof(this) * baseNum << " KB" << endl;
		cout << "\tsizeof(S.Y) = " << (isPow ? (sizeof(Element) * baseNum) << N : sizeof(this->Y) * baseNum) << " B" << endl;
		cout << "\tsizeof(S.k) = " << sizeof(this->k) * baseNum << " B (*)" << endl;
		cout << "\tsizeof(S.V) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->V) * baseNum) << " B" << endl;
		cout << "\tsizeof(S.Z_pi) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->Z_pi) * baseNum) << " B (*)" << endl;
		cout << "\tsizeof(S.T) = " << (isPow ? (sizeof(Element) * baseNum) << N : sizeof(this->T) * baseNum) << " B (*)" << endl;
		cout << "\tsizeof(S.*) = " << (isPow ? (sizeof(Element) * baseNum) * (1 + (1 << beta) + (1 << N)) : (sizeof(this->k) + sizeof(this->Z_pi) + sizeof(this->T)) * baseNum) << " B (*)" << endl;
		return isPow ? (sizeof(Element) * baseNum) * (1 + (1 << beta) + (1 << N)) : (sizeof(this->k) + sizeof(this->Z_pi) + sizeof(this->T)) * baseNum;
	}
};
Sender S;

class Cloud
{
private:
	Element Z[beta] = { NULL };
	Element T[N] = { NULL };
	Element W[beta] = { NULL };

public:
	void receive_Z(Element* Z)
	{
		for (int i = 0; i < beta; ++i)
			this->Z[i] = *(Z + i);
		return;
	}
	void receive_T(Element* T)
	{
		for (int i = 0; i < N; ++i)
		{
			this->T[i] = *(T + i);
			W[i % beta] = decode(this->T[i], this->Z[i % beta]);
		}
		return;
	}
	Element* send_W()
	{
		return this->W;
	}
	size_t printSize(bool isPow)
	{
		cout << "Timeof(C) = " << timerC * baseNum * 1000.0 / CLOCKS_PER_SEC / configuredRunCount + cloudModeledOverheadMilliseconds() << " ms" << endl;
		cout << "sizeof(Cloud) = " << sizeof(Cloud) << (isPow ? " KB" : " B") << endl;
		cout << "sizeof(C) = " << sizeof(this) * baseNum << (isPow ? " MB" : " KB") << endl;
		cout << "\tsizeof(C.Z) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->Z) * baseNum) << " B" << endl;
		cout << "\tsizeof(C.T) = " << (isPow ? (sizeof(Element) * baseNum) << N : sizeof(this->T) * baseNum) << " B (*)" << endl;
		cout << "\tsizeof(C.W) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->W) * baseNum) << " B (*)" << endl;
		cout << "\tsizeof(C.*) = " << (isPow ? (sizeof(Element) * baseNum) << N : sizeof(this->T) * baseNum) << " B (*)" << endl;
		return isPow ? (sizeof(Element) * baseNum) * ((1 << N) + (1 << beta)) : (sizeof(this->T) + sizeof(this->W)) * baseNum;
	}
};
Cloud C;


/* 主函数 */
void initial(bool isAuto)
{

	init_hashpi();// setup pi
	if (isAuto)
	{
		S.auto_input_Y();
		R.auto_input_X();
	}
	else
	{
		cout << "Please input array Y with size " << N << ": " << endl;
		S.input_Y();// Sender S has input Y
		cout << endl;
		cout << "Please input array X with size " << n << ": " << endl;
		R.input_X();// Sender R has input X
		cout << endl;
	}
	return;
}

void setup()
{
	sub_start_time = clock();	R.choose_k();				sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time;// The receiver chooses a random PRG key k
	sub_start_time = clock();	S.receive_k(R.send_k());	sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time; timerS += (double)sub_end_time - sub_start_time;// k is sent to S
	sub_start_time = clock();	S.rand_V();					sub_end_time = clock(); timerS += (double)sub_end_time - sub_start_time;// rand beta
	return;
}

void distribution()
{
	sub_start_time = clock();	R.hash_X_to_X_c(); 				sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time;
#ifdef _DEBUG
	sub_start_time = clock();	R.printArray(); 				sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time;
#endif
	sub_start_time = clock();	R.obtain_Z(); 					sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time;// ss1
	sub_start_time = clock();	R.compute_Z_pi(); 				sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time;// ss2
	sub_start_time = clock();	C.receive_Z(R.send_Z());		sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time; timerC += (double)sub_end_time - sub_start_time;// Z is sent to C
	sub_start_time = clock();	S.receive_Z_pi(R.send_Z_pi()); 	sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time; timerS += (double)sub_end_time - sub_start_time;// ss2
	return;
}

void computation()
{
	sub_start_time = clock();	S.compute_T();				sub_end_time = clock(); timerS += (double)sub_end_time - sub_start_time;
	sub_start_time = clock();	C.receive_T(S.send_T());	sub_end_time = clock(); timerS += (double)sub_end_time - sub_start_time; timerC += (double)sub_end_time - sub_start_time;// T is sent to C
	sub_start_time = clock();	R.receive_W(C.send_W());	sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time; timerC += (double)sub_end_time - sub_start_time;// W is sent to R
	sub_start_time = clock();	R.generate_U();				sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time;
	sub_start_time = clock();	R.printIntersection();		sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time;
	return;
}



/* main 函数 */
void test()
{
	time_t t;
	srand((unsigned int)time(&t));
	initial(true);
	setup();
	distribution();
	computation();
	return;
}

int main(int argc, char* argv[])
{
	Parser parser(argc, argv, "SchemeSPSICA");
	const ParserOptions options = parser.parse();
	if (options.flag <= 0)
	{
		return finishExecution(options.waitingTime, options.decimalPlace, options.flag == 0 ? EXIT_SUCCESS : -1);
	}

	configuredRunCount = options.runCount;
	double elapsedCpuMilliseconds = 0.0;
	std::uint64_t spaceBytes = 0U;
	{
		ScopedOutputSilencer silencer(cout, options.quiet);
		const clock_t start_time = clock();
		for (std::size_t i = 0U; i < options.runCount; ++i)
		{
			cout << "/**************************************** Time: " << i + 1U << " ****************************************/" << endl;
			test();
			cout << endl << endl;
		}
		const clock_t end_time = clock();
		elapsedCpuMilliseconds = static_cast<double>(end_time - start_time) * static_cast<double>(baseNum) * 1000.0 / CLOCKS_PER_SEC / static_cast<double>(options.runCount);
		cout << endl;
		cout << "/**************************************** SPSI-CA ****************************************/" << endl;
		cout << "kBit = " << kBit << "\t\tgamma = " << gamma << endl;
		cout << "N = 2 ** " << N << "\t\tn = 2 ** " << n << "\t\tbeta = [2 ** " << (log2(1.27) + n) << "]" << endl;
		cout << "Time: " << elapsedCpuMilliseconds << " ms" << endl;
		spaceBytes = static_cast<std::uint64_t>(R.printSize(true) + S.printSize(true) + C.printSize(true));
		cout << "sizeof(*) = " << spaceBytes << " B (*)" << endl << endl;
	}

	const double divisor = static_cast<double>(options.runCount);
	const double receiverCpuMilliseconds = timerR * static_cast<double>(baseNum) * 1000.0 / CLOCKS_PER_SEC / divisor;
	const double senderCpuMilliseconds = timerS * static_cast<double>(baseNum) * 1000.0 / CLOCKS_PER_SEC / divisor + senderModeledOverheadMilliseconds();
	const double cloudCpuMilliseconds = timerC * static_cast<double>(baseNum) * 1000.0 / CLOCKS_PER_SEC / divisor + cloudModeledOverheadMilliseconds();
	Saver saver(
		options.outputPath,
		{"scheme", "kBit", "N", "n", "beta", "gamma", "runCount", "elapsedCpuMilliseconds", "receiverCpuMilliseconds", "senderCpuMilliseconds", "cloudCpuMilliseconds", "spaceBytes"},
		options.decimalPlace,
		options.overwriteConfirmed);
	const SaverRow row = {
		std::string("SchemeSPSICA"), std::uint64_t{kBit}, std::uint64_t{N}, std::uint64_t{n}, std::uint64_t{beta}, std::uint64_t{gamma},
		static_cast<std::uint64_t>(options.runCount), elapsedCpuMilliseconds, receiverCpuMilliseconds,
		senderCpuMilliseconds, cloudCpuMilliseconds, spaceBytes};
	const int exitCode = saver.save({row}) ? EXIT_SUCCESS : EXIT_FAILURE;
	return finishExecution(options.waitingTime, options.decimalPlace, exitCode);
}
