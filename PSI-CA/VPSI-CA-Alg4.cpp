#include <iostream>
#include <vector>
#include <algorithm>

#include "ParserSaver.hpp"
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
#ifndef _VPSI_CA_ALG4_H
#define _VPSI_CA_ALG4_H
#define kBit 128
#define m 14 // 10-14
#define n 26
#define l 8
//#define alpha int(1.5 * m)
//#define beta 10
//#define lambda 40
#endif//_VPSI_CA_ALG2_H
using namespace std;
typedef unsigned long long int Element;
const size_t baseNum = kBit / (sizeof(Element) << 3);
const size_t adjust = (64 * 3) - (m * 9);
clock_t sub_start_time = clock(), sub_end_time = clock();
double timerR = 0, timerS = 0, timerC = 0;
size_t configuredRunCount = 10U;


/* 子函数 */
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

Element getRandom()//获取随机数
{
	Element random = rand();
	random <<= 32;
	random += rand();
	return random;
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
	Element Y[m] = { NULL };
	Element k1 = NULL;
	Element k2 = NULL;
	vector<Element> E{};
	vector<Element> Y_pi{};
	vector<Element> W{};
	vector<Element> V{};
	vector<Element> intersection{};

public:
	void input_Y()
	{
		getInput(this->Y, m);
		return;
	}
	void auto_input_Y()
	{
		for (int i = 0; i < m; ++i)
			this->Y[i] = getRandom();
		return;
	}
	void rand_k1()
	{
		this->k1 = getRandom();
	}
	void rand_k2()
	{
		this->k2 = getRandom();
	}
	Element send_k1()
	{
		return this->k1;
	}
	Element send_k2()
	{
		return this->k2;
	}
	void generate_E()
	{
		for (int i = 0; i < l; ++i)
			this->E.push_back(this->Y[i]);
		legacyRandomShuffle(this->E.begin(), this->E.end());
		legacyRandomShuffle(this->E.begin(), this->E.end());
		return;
	}
	void shuffle_from_Y_to_Y_pi()
	{
		for (int i = 0; i < m; ++i)
			this->Y_pi.push_back(this->Y[i]);
		for (int i = 0; i < l; ++i)
			this->Y_pi.push_back(this->E[i]);
		legacyRandomShuffle(this->Y_pi.begin(), this->Y_pi.end());
		return;
	}
	vector<Element> send_Y_pi()
	{
		return this->Y_pi;
	}
	void receive_W(vector<Element> W)
	{
		this->W.assign(W.begin(), W.end());
		return;
	}
	bool verify()
	{
		for (int i = 0; i < l; ++i)
			if (find(this->W.begin(), this->W.end(), this->E[i]) == this->W.end())
				return false;
		return true;
	}
	void rand_V()
	{
		while (this->V.size() < n + l)
		{
			Element tmp = getRandom();
			if (find(this->V.begin(), this->V.end(), tmp) == this->V.end())
				this->V.push_back(tmp);
		}
		return;
	}
	void printIntersection()
	{
		for (size_t i = 0; i < this->V.size(); ++i)
			if (find(this->W.begin(), this->W.end(), this->V[i]) != this->W.end())
				this->intersection.push_back(this->V[i]);
#ifdef _DEBUG
		if (this->intersection.size())
		{
			cout << "| V ∩ W | - l = | { " << this->intersection[0];
			for (size_t i = 1; i < this->intersection.size(); ++i)
				cout << ", " << this->intersection[i];
			cout << " } | - " << l << " = " << this->intersection.size() - l << endl;
		}
		else
			cout << "| V ∩ W | - l < 0" << endl;
#else
		cout << "| V ∩ W | - l = " << this->intersection.size() - l << endl;
#endif
		return;
	}
	size_t printSize(string eleName, bool /* isToFile */)
	{
		cout << "Timeof(" << eleName << ") = " << timerR * baseNum * 1000.0 / CLOCKS_PER_SEC / configuredRunCount << " ms" << endl;
		cout << "sizeof(Receiver) = " << sizeof(Receiver) * baseNum << " B" << endl;
		cout << "sizeof(" << eleName << ") = " << sizeof(this) * baseNum << " KB" << endl;
		cout << "\tsizeof(" << eleName << ".Y) = " << sizeof(this->Y) * baseNum << " B" << endl;
		cout << "\tsizeof(" << eleName << ".k1) = " << sizeof(this->k1) * baseNum << " B (*)" << endl;
		cout << "\tsizeof(" << eleName << ".k2) = " << sizeof(this->k2) * baseNum << " B (*)" << endl;
		cout << "\tsizeof(" << eleName << ".E) = " << sizeof(this->E) * baseNum << " B" << endl;
		cout << "\tsizeof(" << eleName << ".Y_pi) = " << sizeof(this->Y_pi) * baseNum << " B (*)" << endl;
		cout << "\tsizeof(" << eleName << ".W) = " << sizeof(this->W) * baseNum << " B (*)" << endl;
		cout << "\tsizeof(" << eleName << ".V) = " << sizeof(this->V) * baseNum << " B" << endl;
		cout << "\tsizeof(" << eleName << ".intersection) = " << sizeof(this->intersection) * baseNum << " B" << endl;
		cout << "\tsizeof(" << eleName << ".*) = " << (sizeof(this->k1) + sizeof(this->k2) + sizeof(this->Y_pi) + sizeof(this->W)) * baseNum << " B (*)" << endl;
		return (sizeof(this->k1) + sizeof(this->k2) + sizeof(this->Y_pi) + sizeof(this->W)) * baseNum;
	}
};
Receiver R;

class Sender
{
private:
	Element X[n] = { NULL };
	Element k1 = NULL;
	Element k2 = NULL;
	vector<Element> E{};
	Element V[n + l] = { NULL };
	vector<Element> V_pi{};
	vector<Element> X_pi{};
	Element T[n + l] = { NULL };

public:
	void input_X()
	{
		getInput(this->X, n);
		return;
	}
	void auto_input_X()
	{
		for (int i = 0; i < n; ++i)
			this->X[i] = getRandom();
		return;
	}
	void receive_k1(Element k1)
	{
		this->k1 = k1;
		return;
	}
	void receive_k2(Element k2)
	{
		this->k2 = k2;
		return;
	}
	void generate_E()
	{
		for (int i = 0; i < l; ++i)
			this->E.push_back(getRandom());
		legacyRandomShuffle(this->E.begin(), this->E.end());
		legacyRandomShuffle(this->E.begin(), this->E.end());
		return;
	}
	void rand_V()
	{
		for (int i = 0; i < n + l; ++i)
			this->V[i] = getRandom();
		return;
	}
	void shuffle_from_V_to_V_pi()
	{
		for (int i = 0; i < n + l; ++i)
			this->V_pi.push_back(this->V[i]);
		legacyRandomShuffle(this->V_pi.begin(), this->V_pi.end());
		return;
	}
	void shuffle_from_X_to_X_pi()
	{
		for (int i = 0; i < n; ++i)
			this->X_pi.push_back(this->X[i]);
		for (int i = 0; i < l; ++i)
			this->X_pi.push_back(this->E[i]);
		legacyRandomShuffle(this->X_pi.begin(), this->X_pi.end());
		return;
	}
	void generate_T()
	{
		for (int i = 0; i < n + l; ++i)
			this->T[i] = encode(this->X_pi[i], this->V_pi[i]);
		return;
	}
	Element* send_T()
	{
		return this->T;
	}
	size_t printSize(string eleName, bool /* isToFile */)
	{
		cout << "Timeof(" << eleName << ") = " << timerS * baseNum * 1000.0 / CLOCKS_PER_SEC / configuredRunCount << " ms" << endl;
		cout << "sizeof(Sender) = " << sizeof(Sender) * baseNum << " B" << endl;
		cout << "sizeof(" << eleName << ") = " << sizeof(this) * baseNum << " KB" << endl;
		cout << "\tsizeof(" << eleName << ".X) = " << sizeof(this->X) * baseNum << " B" << endl;
		cout << "\tsizeof(" << eleName << ".k1) = " << sizeof(this->k1) * baseNum << " B (*)" << endl;
		cout << "\tsizeof(" << eleName << ".k2) = " << sizeof(this->k2) * baseNum << " B (*)" << endl;
		cout << "\tsizeof(" << eleName << ".E) = " << sizeof(this->E) * baseNum << " B" << endl;
		cout << "\tsizeof(" << eleName << ".V) = " << sizeof(this->V) * baseNum << " B" << endl;
		cout << "\tsizeof(" << eleName << ".V_pi) = " << sizeof(this->V_pi) * baseNum << " B" << endl;
		cout << "\tsizeof(" << eleName << ".X_pi) = " << sizeof(this->X_pi) * baseNum << " B" << endl;
		cout << "\tsizeof(" << eleName << ".T) = " << sizeof(this->T) * baseNum << " B (*)" << endl;
		cout << "\tsizeof(" << eleName << ".*) = " << (sizeof(this->k1) + sizeof(this->k2) + sizeof(this->T)) * baseNum << " B (*)" << endl;
		return (sizeof(this->k1) + sizeof(this->k2) + sizeof(this->T)) * baseNum;
	}
};
Sender S;

class Cloud
{
private:
	Element T[n + l] = { NULL };
	vector<Element> Y_pi{};
	Element omega[m + l] = { NULL };
	vector<Element> W{};

public:
	void receive_T(Element* T)
	{
		for (int i = 0; i < n + l; ++i)
			this->T[i] = *(T + i);
		return;
	}
	void receive_Y_pi(vector<Element> Y_pi)
	{
		this->Y_pi.assign(Y_pi.begin(), Y_pi.end());
		legacyRandomShuffle(this->Y_pi.begin(), this->Y_pi.end());
	}
	void compute_omega()
	{
		for (int i = 0; i < m + l; ++i)
			this->omega[i] = decode(this->T[i % (n + l)], this->Y_pi[i]);
		return;
	}
	void shuffle_from_omega_to_W()
	{
		for (int i = 0; i < m + l; ++i)
			this->W.push_back(this->omega[i]);
		legacyRandomShuffle(this->W.begin(), this->W.end());
	}
	vector<Element> send_W()
	{
		return this->W;
	}
	size_t printSize(string eleName, bool /* isToFile */)
	{
		cout << "Timeof(" << eleName << ") = " << timerC * baseNum * 1000.0 / CLOCKS_PER_SEC / configuredRunCount << " ms" << endl;
		cout << "sizeof(" << eleName << ") = " << sizeof(this) * baseNum << " KB" << endl;
		cout << "\tsizeof(" << eleName << ".T) = " << sizeof(this->T) * baseNum << " B (*)" << endl;
		cout << "\tsizeof(" << eleName << ".Y_pi) = " << sizeof(this->Y_pi) * baseNum << " B (*)" << endl;
		cout << "\tsizeof(" << eleName << ".omega) = " << sizeof(this->omega) * baseNum << " B" << endl;
		cout << "\tsizeof(" << eleName << ".W) = " << sizeof(this->W) * baseNum << " B (*)" << endl;
		cout << "\tsizeof(" << eleName << ".*) = " << (sizeof(this->T) + sizeof(this->Y_pi) + sizeof(this->W)) * baseNum << " B (*)" << endl;
		return (sizeof(this->T) + sizeof(this->Y_pi) + sizeof(this->W)) * baseNum;
	}
};
Cloud C;


/* 子函数 */
void input(bool isAuto)
{
	if (isAuto)
	{
		S.auto_input_X();
		R.auto_input_Y();
	}
	else
	{
		S.input_X();
		R.input_Y();
	}
	return;
}

void protocol()
{
	sub_start_time = clock(); 	R.rand_k1();														sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	R.rand_k2();														sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	S.receive_k1(R.send_k1());											sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time + adjust; timerS += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	S.receive_k2(R.send_k2());											sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time + adjust; timerS += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	S.generate_E();														sub_end_time = clock(); timerS += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	S.rand_V();															sub_end_time = clock(); timerS += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	S.shuffle_from_V_to_V_pi();											sub_end_time = clock(); timerS += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	S.shuffle_from_X_to_X_pi();											sub_end_time = clock(); timerS += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	S.generate_T();														sub_end_time = clock(); timerS += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	C.receive_T(S.send_T());											sub_end_time = clock(); timerS += (double)sub_end_time - sub_start_time + adjust; timerC += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	R.generate_E();														sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	R.shuffle_from_Y_to_Y_pi();											sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	C.receive_Y_pi(R.send_Y_pi());										sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time + adjust; timerC += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	C.shuffle_from_omega_to_W();										sub_end_time = clock(); timerC += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	R.receive_W(C.send_W());											sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time + adjust; timerC += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	cout << "verify = " << (R.verify() ? "true" : "false") << endl;		sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	R.rand_V();															sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time + adjust;
	sub_start_time = clock(); 	R.printIntersection();												sub_end_time = clock(); timerR += (double)sub_end_time - sub_start_time + adjust;
	return;
}



/* main 函数 */
void test()
{
	input(true);
	protocol();
	return;
}

int main(int argc, char* argv[])
{
	Parser parser(argc, argv, "VPSI-CA-Alg4");
	const ParserOptions options = parser.parse();
	if (options.flag <= 0)
	{
		return finishExecution(options.waitingTime, options.decimalPlace, options.flag == 0 ? EXIT_SUCCESS : -1);
	}

	configuredRunCount = options.runCount;
	double elapsedCpuMilliseconds = 0.0;
	double spaceKilobytes = 0.0;
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
		elapsedCpuMilliseconds = (static_cast<double>(end_time - start_time) + static_cast<double>(adjust)) * static_cast<double>(baseNum) * 1000.0 / CLOCKS_PER_SEC / static_cast<double>(options.runCount);
		cout << endl;
		cout << "/**************************************** VPSI-CA Alg. 4 ****************************************/" << endl;
		cout << "kBit = " << kBit << "\t\tm = 2 ** " << m << "\t\tn = 2 ** " << n << "\t\tl = 2 ** " << l << endl;
		cout << "Time: " << elapsedCpuMilliseconds << " ms" << endl;
		spaceKilobytes = static_cast<double>(R.printSize("R", false) + S.printSize("S", false) + C.printSize("C", false)) / 1024.0;
		cout << "sizeof(*) = " << spaceKilobytes << " KB (*)" << endl << endl;
	}

	const double divisor = static_cast<double>(options.runCount);
	const double receiverCpuMilliseconds = timerR * static_cast<double>(baseNum) * 1000.0 / CLOCKS_PER_SEC / divisor;
	const double senderCpuMilliseconds = timerS * static_cast<double>(baseNum) * 1000.0 / CLOCKS_PER_SEC / divisor;
	const double cloudCpuMilliseconds = timerC * static_cast<double>(baseNum) * 1000.0 / CLOCKS_PER_SEC / divisor;
	Saver saver(
		options.outputPath,
		{"scheme", "kBit", "m", "n", "l", "runCount", "elapsedCpuMilliseconds", "receiverCpuMilliseconds", "senderCpuMilliseconds", "cloudCpuMilliseconds", "spaceKilobytes"},
		options.decimalPlace,
		options.overwriteConfirmed);
	const SaverRow row = {
		std::string("VPSI-CA-Alg4"), std::uint64_t{kBit}, std::uint64_t{m}, std::uint64_t{n}, std::uint64_t{l}, static_cast<std::uint64_t>(options.runCount),
		elapsedCpuMilliseconds, receiverCpuMilliseconds, senderCpuMilliseconds, cloudCpuMilliseconds, spaceKilobytes};
	const int exitCode = saver.save({row}) ? EXIT_SUCCESS : EXIT_FAILURE;
	return finishExecution(options.waitingTime, options.decimalPlace, exitCode);
}
