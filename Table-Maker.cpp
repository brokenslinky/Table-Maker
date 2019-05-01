#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "math.h"
#include <ctime>
#include <thread>
#include <mutex>
#include <algorithm>
#include <climits>

using namespace std;

mutex m;
vector<double> xLog;
mutex n;
vector<double> yLog;
mutex o;
vector<double> zLog; 
bool doneParsing = false;
double xMultiplier{ 1.0 }, yMultiplier{ 1.0 }, zMultiplier{ 1.0 };

void writeX(double x)
{
	lock_guard<mutex> lock(m);
	xLog.push_back(x);
}
void writeY(double y)
{
	lock_guard<mutex> lock(n);
	yLog.push_back(y);
}
void writeZ(double z)
{
	lock_guard<mutex> lock(o);
	zLog.push_back(z);
}
double readX(int index)
{
	lock_guard<mutex> lock(m);
	return xLog[index];
}
double readY(int index)
{
	lock_guard<mutex> lock(n);
	return yLog[index];
}
double readZ(int index)
{
	lock_guard<mutex> lock(o);
	return zLog[index];
}

#ifdef __WINDOWS__
#include <windows.h>
#else 
#include <unistd.h>
#endif
void OSAgnosticSleep(int milliseconds)
{
#ifdef WINDOWS
	Sleep(milliseconds);
#else 
usleep(milliseconds * 1000);   // usleep uses microseconds
#endif
}

class Axis
{
public:
	string name;
	int columnNumber;
	double multiplier{ 1.0 }, power{ 1.0 }, offset{ 0.0 };
	vector<double> nodes;
};

void parseHeader(string line, char delimiter, Axis& xAxis, Axis& yAxis, Axis& zAxis)
{
	int columnNumber = 0;
	int delimiterIndex;
	string word;
	while ((delimiterIndex = line.find(delimiter)) != string::npos)
	{
		word = line.substr(0, delimiterIndex);
		line = line.substr(delimiterIndex + 1);
		if (word == xAxis.name)
			xAxis.columnNumber = columnNumber;
		if (word == yAxis.name)
			yAxis.columnNumber = columnNumber;
		if (word == zAxis.name)
			zAxis.columnNumber = columnNumber;
		columnNumber++;
	}
	word = line;
	if (word == xAxis.name)
		xAxis.columnNumber = columnNumber;
	if (word == yAxis.name)
		yAxis.columnNumber = columnNumber;
	if (word == zAxis.name)
		zAxis.columnNumber = columnNumber;
}

void getModifier(string line, Axis& axis)
{
	remove(line.begin(), line.end(), ' ');
	string operations{ "^*/+-" };
	char operation = ' ';
	double operand;
	if (operations.find(line[0]) != string::npos)
	{
		operation = line[0];
		operand = stod(line.substr(1));
	}
	else
	{
		operation = line[line.size() - 1];
		operand = stod(line.substr(0, line.size() - 1));
	}
	if (operation == '^')
		axis.power = operand;
	if (operation == '*')
		axis.multiplier = operand;
	else if (operation == '/')
		axis.multiplier = 1.0 / operand;
	else if (operation == '+')
		axis.offset = operand;
	else if (operation == '-')
		axis.offset = -operand;
}
void getModifier(Axis& axis)
{
	cout << "Enter any modifiers for " << axis.name;
	string line = "";
	getline(cin, line);
	while (line != "")
	{
		getModifier(line, axis);
		cout << "Enter additional modifiers for " << axis.name;
		getline(cin, line);
	}
}

void parseLog(string logLocation, Axis xAxis, Axis yAxis, Axis zAxis)
{
	// get column numbers from header
	ifstream file(logLocation); // log of experimental data, parameters vs time
	string line;
	getline(file, line);
	char delimiter = '\t';
	if (line.find(',') != string::npos)
		delimiter = ',';
	parseHeader(line, delimiter, xAxis, yAxis, zAxis);
	while (getline(file, line))
	{
		string word = "";
		int columnNumber = 0;
		int delimiterIndex;
		int prevDelimiter = 0;
		while ((delimiterIndex = line.find(delimiter, prevDelimiter)) != string::npos)
		{
			if (columnNumber == xAxis.columnNumber)
				writeX(stod(line.substr(prevDelimiter, delimiterIndex - prevDelimiter)));
			if (columnNumber == yAxis.columnNumber)
				writeY(stod(line.substr(prevDelimiter, delimiterIndex - prevDelimiter)));
			if (columnNumber == zAxis.columnNumber)
				writeZ(stod(line.substr(prevDelimiter, delimiterIndex - prevDelimiter)));
			prevDelimiter = delimiterIndex + 1;
			columnNumber++;
		}
		if (columnNumber == xAxis.columnNumber)
			writeX(stod(line.substr(prevDelimiter)));
		if (columnNumber == yAxis.columnNumber)
			writeY(stod(line.substr(prevDelimiter)));
		if (columnNumber == zAxis.columnNumber)
			writeZ(stod(line.substr(prevDelimiter)));
	}
	file.close();
	doneParsing = true;
}

string askPath(string item, string defaultPath = "")
{
	cout << endl << "Enter the path to " << item << endl;
	string path = "";
	getline(cin, path);
	if (path == "")
	{
		cout << "Using default path \"" << defaultPath << "\"" << endl;
		return defaultPath;
	}
	else 
		return path;
}

string askName(string item)
{
	cout << endl << "Enter the name of " << item << endl;
	string name = "";
	getline(cin, name);
	return name;
}

Axis getAxisFromFile(string fileName)
{
	Axis axis;
	string line = "";
	ifstream file(fileName);
	getline(file, line);
	{
		string operations{ "^*/+-" };
		while (operations.find(line[0]) != string::npos || operations.find(line[line.size() - 1]) != string::npos)
		{
			getModifier(line, axis);
			getline(file, line);
		}
	}
	axis.name = line;
	while (getline(file, line))
		axis.nodes.push_back(stod(line));
	file.close();
	cout << "Using the default name and settings found in \"" << fileName << "\": " << axis.name;
	if (axis.power != 1.0)
		cout << " ^ " << axis.power;
	if (axis.multiplier != 1.0)
		cout << " * " << axis.multiplier;
	if (axis.offset != 0.0)
		cout << " + " << axis.offset;
	cout << endl;
	return axis;
}

void showAxisOptions(string logFile)
{
	ifstream file(logFile);
	string line;
	getline(file, line);
	file.close();
	cout << line << endl;
}

void calculateTable()
{
	string logPath = askPath("the log file", "log xyz.csv");

	// get axes
	cout << "Possible Axis names are:" << endl;
	showAxisOptions(logPath);
	Axis xAxis, yAxis, zAxis;
	xAxis.name = askName("the x-axis");
	if (xAxis.name == "")
		xAxis = getAxisFromFile("x-axis.txt");
	else
		getModifier(xAxis);
	yAxis.name = askName("the y-axis");
	if (yAxis.name == "")
		yAxis = getAxisFromFile("y-axis.txt");
	else
		getModifier(yAxis);
	zAxis.name = askName("the z-axis");
	if (zAxis.name == "")
		zAxis = getAxisFromFile("z-name.txt");
	else
		getModifier(zAxis);

	// start log parsing thread 
	cout << "Parsing log file in a new thread..." << endl;
	thread parsingThread(parseLog, logPath, xAxis, yAxis, zAxis);

	clock_t startTime = clock(); // start timer when log begins parsing

	// file locations
	string tableLocation = askPath("the desired output location", "generated table.txt");

	double avgXBucketSizeSquared = abs(xAxis.nodes.back() - xAxis.nodes.front()) / xAxis.nodes.size();
	avgXBucketSizeSquared *= avgXBucketSizeSquared;
	double avgYBucketSizeSquared = abs(yAxis.nodes.back() - yAxis.nodes.front()) / yAxis.nodes.size();
	avgYBucketSizeSquared *= avgYBucketSizeSquared;
	// populate Z(x, y) table based on log
	vector<vector<long double>> tableZ;
	vector<vector<long double>> sumInvSqXyDist;
	for (double& x : xAxis.nodes)
	{
		vector<long double> row;
		for (double& y : yAxis.nodes)
			row.push_back(0.0);
		tableZ.push_back(row);
		sumInvSqXyDist.push_back(row);
	}
	while (zLog.size() < 2)
		OSAgnosticSleep(16);
	std::cout << "Calculating table coefficients (as log is being parsed)..." << endl;
	for (int datum = 0; datum < zLog.size(); datum++)
	{
		// determine closest x-index
		int indexNow;
		int xIndex = 0;
		double difference = (double)INT_MAX;
		for (indexNow = 0; indexNow < xAxis.nodes.size(); indexNow++)
			if (abs(readX(datum) - xAxis.nodes[indexNow]) < difference)
			{
				xIndex = indexNow;
				difference = abs(readX(datum) - xAxis.nodes[indexNow]);
			}
		double differenceSquared = (difference * difference) / avgXBucketSizeSquared;
		//determine closest y-index
		difference = (double)INT_MAX;
		int yIndex = 0;
		for (indexNow = 0; indexNow < yAxis.nodes.size(); indexNow++)
			if (abs(readY(datum) - yAxis.nodes[indexNow]) < difference)
			{
				yIndex = indexNow;
				difference = abs(readY(datum) - yAxis.nodes[indexNow]);
			}
		differenceSquared += difference * difference / avgYBucketSizeSquared;
		double z = readZ(datum);
		if (differenceSquared != 0.0)
		{
			tableZ[xIndex][yIndex] += readZ(datum) / differenceSquared;
			sumInvSqXyDist[xIndex][yIndex] += 1.0 / differenceSquared;
		}
		while (datum + 2 >= zLog.size() && !doneParsing)
			OSAgnosticSleep(16);
	}
	parsingThread.detach();

	for (int i = 0; i < xAxis.nodes.size(); i++)
		for (int j = 0; j < yAxis.nodes.size(); j++)
		{
			if (sumInvSqXyDist[i][j] == 0.0)
				tableZ[i][j] = 0.0;
			else
				tableZ[i][j] /= sumInvSqXyDist[i][j];
		}

	// save table to a file
	std::cout << "Printing log to \"" << tableLocation << "\"" << endl;
	ofstream outputFile;
	outputFile.open(tableLocation);
	for (int i = 0; i < xAxis.nodes.size(); i++)
		outputFile << "\t" << to_string(pow(xAxis.nodes[i], xAxis.power) * xAxis.multiplier + xAxis.offset).substr(0, 5);
	for (int j = 0; j < yAxis.nodes.size(); j++)
	{
		outputFile << endl << to_string(pow(yAxis.nodes[j], yAxis.power) * yAxis.multiplier + yAxis.offset).substr(0, 5);
		for (int i = 0; i < xAxis.nodes.size(); i++)
		{
			if (tableZ[i][j] == 0.0)
			{
				if (sumInvSqXyDist[i][j] == 0.0)
					outputFile << "\t*";
				else
					outputFile << "\t0";
			}
			else
				outputFile << "\t" << to_string(pow(tableZ[i][j], zAxis.power) * zAxis.multiplier + zAxis.offset).substr(0, 5);
		}
	}
	outputFile.close();
	std::cout << "Table printed to \"" << tableLocation << "\"" << endl;
	std::cout << "Time elapsed = " << (double)(clock() - startTime) / CLOCKS_PER_SEC << " seconds" << endl;

	// print map of standard deviations
	std::cout << "Calculating standard deviations (okay to close now if not needed)..." << endl;
	vector<vector<double>> stdev;
	vector<vector<int>> samples;
	for (int i = 0; i < xAxis.nodes.size(); i++)
	{
		vector<double> rowDouble;
		vector<int> rowInt;
		for (int j = 0; j < yAxis.nodes.size(); j++)
		{
			rowDouble.push_back(0.0);
			rowInt.push_back(0);
		}
		stdev.push_back(rowDouble);
		samples.push_back(rowInt);
	}
	for (int datum = 0; datum < zLog.size(); datum++)
	{
		// determine closest x-index
		int indexNow;
		int xIndex = 0;
		double difference = (double)INT_MAX;
		for (indexNow = 0; indexNow < xAxis.nodes.size(); indexNow++)
			if (abs(readX(datum) - xAxis.nodes[indexNow]) < difference)
			{
				xIndex = indexNow;
				difference = abs(readX(datum) - xAxis.nodes[indexNow]);
			}
		//determine closest y-index
		difference = (double)INT_MAX;
		int yIndex = 0;
		for (indexNow = 0; indexNow < yAxis.nodes.size(); indexNow++)
			if (abs(readY(datum) - yAxis.nodes[indexNow]) < difference)
			{
				yIndex = indexNow;
				difference = abs(readY(datum) - yAxis.nodes[indexNow]);
			}
		stdev[xIndex][yIndex] = (readZ(datum) - tableZ[xIndex][yIndex]) * (readZ(datum) - tableZ[xIndex][yIndex]);
		samples[xIndex][yIndex]++;
	}

	string stdevMap = "standard deviations.txt";
	int characterIndex = tableLocation.find_last_of("/\\");
	if (characterIndex != string::npos)
		stdevMap = tableLocation.substr(0, characterIndex) + "/standard deviations.txt";
	outputFile.open(stdevMap);
	for (int i = 0; i < xAxis.nodes.size(); i++)
		outputFile << "\t" << to_string(pow(xAxis.nodes[i], xAxis.power) * xAxis.multiplier + xAxis.offset).substr(0, 5);
	for (int j = 0; j < yAxis.nodes.size(); j++)
	{
		outputFile << endl << to_string(pow(yAxis.nodes[j], yAxis.power) * yAxis.multiplier + yAxis.offset).substr(0, 5);
		for (int i = 0; i < xAxis.nodes.size(); i++)
		{
			if (samples[i][j] == 0)
				outputFile << "\t*";
			else
			{
				outputFile << "\t" << to_string(pow(sqrt(stdev[i][j] / (samples[i][j] - 1)), zAxis.power) * zAxis.multiplier + zAxis.offset).substr(0, 5);
			}
		}
	}
	std::cout << "Standard deviations printed to \"" << stdevMap << "\"" << endl;
	std::cout << "Time elapsed = " << (double)(clock() - startTime) / CLOCKS_PER_SEC << " seconds" << endl;
}

int main()
{
	calculateTable();
	std::cout << "Tables created";
	string waitForUser;
	cin >> waitForUser;
	return 0;
}
