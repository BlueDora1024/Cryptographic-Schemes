#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>

#include "ParserSaver.hpp"
#ifndef _PSICA_H
#define _PSICA_H
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
#endif

#define kBit 128
#define N 26
#define n 11
#define beta 11 // 2 ** beta = 2 ** n * 1.27
#define gamma 3
#endif//_PSICA_H
using namespace std;
typedef unsigned long long int Element;
typedef const void* CPVOID;
vector<int> hashpi{}, archashpi{};
size_t baseNum = kBit / (sizeof(Element) << 3);
clock_t sub_start_time = clock(), sub_end_time = clock();
double timerR = 0, timerS = 0, timerC = 0;
size_t configuredRunCount = 10U;


/* 子函数 */
static void init_hashpi()
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

static int pi(int index)
{
	return hashpi[index % beta];
}

static int arcpi(int value)
{
	return archashpi[value % beta];
}

static Element getRandom()//获取随机数
{
	Element random = rand();
	random <<= 32;
	random += rand();
	return random;
}

static void getInput(Element array[], int size)//获得输入
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

static Element r_i(Element ele, int i)//哈希函数
{
	return ele << i;
}

static int compare(CPVOID a, CPVOID b)//比较函数
{
	return (int)(*(Element*)a - *(Element*)b);
}

static int BinarySearch(Element array_lists[], int nBegin, int nEnd, Element target, unsigned int& compareCount)
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

static Element encode(Element a, Element b)
{
	return a + b;
}

static Element decode(Element a, Element b)
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
	Element send_k() const
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
	Element* help_compute_Z_pi()
	{
		for (int i = 0; i < beta; ++i)
			this->Z_pi[i] = this->X_c[pi(i)] ^ this->Z[i];
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
	void receive_W(Element* _W)
	{
		for (int i = 0; i < beta; ++i)
			this->W[i] = *(_W + i);
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
		cout << "\tsizeof(R.Z) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->Z) * baseNum) << " B (*)" << endl;
		cout << "\tsizeof(R.W) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->W) * baseNum) << " B (*)" << endl;
		cout << "\tsizeof(R.U) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->U) * baseNum) << " B" << endl;
		cout << "\tsizeof(R.Z_pi) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->Z_pi) * baseNum) << " B (*)" << endl;
		cout << "\tsizeof(R.intersection) = " << (isPow ? sizeof(Element) * baseNum * this->intersection.size() : sizeof(this->intersection) * baseNum) << " B (*)" << endl;
		cout << "\tsizeof(R.*) = " << (isPow ? (sizeof(Element) * baseNum) * (1 + (1 << beta) + (1 << beta) + (1 << beta)) : (sizeof(this->k) + sizeof(this->Z) + sizeof(this->W) + sizeof(this->Z_pi)) * baseNum) << " B (*)" << endl;
		return (sizeof(this->k) + sizeof(this->Z) + sizeof(this->W) + sizeof(this->Z_pi)) * baseNum;
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
	void receive_k(Element _k)
	{
		this->k = _k;
		return;
	}
	void rand_V()
	{
		for (int i = 0; i < beta; ++i)
			V[i] = this->k - rand();
		return;
	}
	void compute_Z_pi(Element* Z)
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
		cout << "Timeof(S) = " << timerS * baseNum * 1000.0 / CLOCKS_PER_SEC / configuredRunCount << " ms" << endl;
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
	void receive_Z(Element* _Z)
	{
		for (int i = 0; i < beta; ++i)
			this->Z[i] = *(_Z + i);
		return;
	}
	void receive_T(Element* _T)
	{
		for (int i = 0; i < N; ++i)
		{
			this->T[i] = *(_T + i);
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
		cout << "Timeof(C) = " << timerC * baseNum * 1000.0 / CLOCKS_PER_SEC / configuredRunCount << " ms" << endl;
		cout << "sizeof(Cloud) = " << sizeof(Cloud) << (isPow ? " KB" : " B") << endl;
		cout << "sizeof(C) = " << sizeof(this) * baseNum << (isPow ? " MB" : " KB") << endl;
		cout << "\tsizeof(C.Z) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->Z) * baseNum) << " B (*)" << endl;
		cout << "\tsizeof(C.T) = " << (isPow ? (sizeof(Element) * baseNum) << N : sizeof(this->T) * baseNum) << " B (*)" << endl;
		cout << "\tsizeof(C.W) = " << (isPow ? (sizeof(Element) * baseNum) << beta : sizeof(this->W) * baseNum) << " B" << endl;
		cout << "\tsizeof(C.*) = " << (isPow ? (sizeof(Element) * baseNum) * ((1 << beta) + (1 << N)) : (sizeof(this->Z) + sizeof(this->T)) * baseNum) << " B (*)" << endl;
		return isPow ? (sizeof(Element) * baseNum) * ((1 << beta) + (1 << N)) : (sizeof(this->Z) + sizeof(this->T)) * baseNum;
	}
};
Cloud C;


/* 主函数 */
static void initial(bool isAuto)
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

static void setup()
{
	sub_start_time = clock();	R.choose_k();				sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time;// The receiver chooses a random PRG key k
	sub_start_time = clock();	S.receive_k(R.send_k());	sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time; timerS += (double)sub_end_time - sub_start_time;// k is sent to S
	sub_start_time = clock();	S.rand_V();					sub_end_time = clock(); timerS += (double)sub_end_time - sub_start_time;// rand beta
	return;
}

static void distribution()
{
	sub_start_time = clock();	R.hash_X_to_X_c(); 						sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time;
#ifdef _DEBUG
	sub_start_time = clock();	R.printArray(); 						sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time;
#endif
	sub_start_time = clock();	R.obtain_Z(); 							sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time;// ss1
	sub_start_time = clock();	S.compute_Z_pi(R.help_compute_Z_pi()); 	sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time; timerS += (double)sub_end_time - sub_start_time;// ss2
	sub_start_time = clock();	C.receive_Z(R.send_Z());				sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time; timerC += (double)sub_end_time - sub_start_time;// Z is sent to C
	return;
}

static void computation()
{
	sub_start_time = clock();	S.compute_T();				sub_end_time = clock(); timerS += (double)sub_end_time - sub_start_time;
	sub_start_time = clock();	C.receive_T(S.send_T());	sub_end_time = clock(); timerS += (double)sub_end_time - sub_start_time; timerC += (double)sub_end_time - sub_start_time;// T is sent to C
	sub_start_time = clock();	R.receive_W(C.send_W());	sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time; timerC += (double)sub_end_time - sub_start_time;// W is sent to R
	sub_start_time = clock();	R.generate_U();				sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time;
	sub_start_time = clock();	R.printIntersection();		sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time;
	return;
}



/* main 函数 */
static void test()
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
	Parser parser(argc, argv, "PSI-CA");
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
		cout << "/**************************************** PSI-CA ****************************************/" << endl;
		cout << "kBit = " << kBit << "\t\tgamma = " << gamma << endl;
		cout << "N = 2 ** " << N << "\t\tn = 2 ** " << n << "\t\tbeta = [2 ** " << (log2(1.27) + n) << "]" << endl;
		cout << "Time: " << elapsedCpuMilliseconds << " ms" << endl;
		spaceBytes = static_cast<std::uint64_t>(R.printSize(true) + S.printSize(true) + C.printSize(true));
		cout << "sizeof(*) = " << spaceBytes << " B (*)" << endl << endl;
	}

	const double divisor = static_cast<double>(options.runCount);
	const double receiverCpuMilliseconds = timerR * static_cast<double>(baseNum) * 1000.0 / CLOCKS_PER_SEC / divisor;
	const double senderCpuMilliseconds = timerS * static_cast<double>(baseNum) * 1000.0 / CLOCKS_PER_SEC / divisor;
	const double cloudCpuMilliseconds = timerC * static_cast<double>(baseNum) * 1000.0 / CLOCKS_PER_SEC / divisor;
	Saver saver(
		options.outputPath,
		{"scheme", "kBit", "N", "n", "beta", "gamma", "runCount", "elapsedCpuMilliseconds", "receiverCpuMilliseconds", "senderCpuMilliseconds", "cloudCpuMilliseconds", "spaceBytes"},
		options.decimalPlace,
		options.overwriteConfirmed);
	const SaverRow row = {
		std::string("PSI-CA"), std::uint64_t{kBit}, std::uint64_t{N}, std::uint64_t{n}, std::uint64_t{beta}, std::uint64_t{gamma},
		static_cast<std::uint64_t>(options.runCount), elapsedCpuMilliseconds, receiverCpuMilliseconds,
		senderCpuMilliseconds, cloudCpuMilliseconds, spaceBytes};
	const int exitCode = saver.save({row}) ? EXIT_SUCCESS : EXIT_FAILURE;
	return finishExecution(options.waitingTime, options.decimalPlace, exitCode);
}
