// Table Maker.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "math.h"
#include <ctime>
#include "windows.h"
#include <thread>
#include <mutex>
#include <algorithm>
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
	line.erase(remove(line.begin(), line.end(), ' '));
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

Axis parseAxis(string fileLocation)
{
	Axis axis;
	string line = "";
	ifstream file(fileLocation);
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
	return axis;
}

void parseLog(string logLocation, Axis xAxis, Axis yAxis, Axis zAxis)
{
	// get column numbers from header
	std::cout << "Parsing log file..." << endl;
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

void calculateTable()
{
	// file locations
	string tableLocation = "generated table.txt"; // output location

	// get axes
	Axis xAxis = parseAxis("x-axis.txt");
	Axis yAxis = parseAxis("y-axis.txt");
	Axis zAxis = parseAxis("z-name.txt");

	// start log parsing thread
	thread parsingThread(parseLog, "log xyz.csv", xAxis, yAxis, zAxis);

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
		Sleep(16);
	std::cout << "Calculating table coefficients..." << endl;
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
			Sleep(16);
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
	std::cout << "printing log to " << tableLocation << endl;
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
	std::cout << "Table printed to " << tableLocation << endl;

	// print map of standard deviations
	std::cout << "Calculating standard deviations..." << endl;
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

	string stdevMap = "Standard Deviations.txt";
	std::cout << "printing uncertainties to " << stdevMap << endl;
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
}

int main()
{
	clock_t startTime = clock();
	calculateTable();
	std::cout << "table created";
	std::cout << endl << "time elapsed = " << (double)(clock() - startTime) / CLOCKS_PER_SEC << " seconds";
	string line;
	cin >> line;
	return 0;
}
