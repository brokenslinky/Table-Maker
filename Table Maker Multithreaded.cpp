// Table Maker.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "math.h"
#include <ctime>
#include "windows.h"
#include <thread>
#include <mutex>
using namespace std;

mutex m;
vector<double> xLog;
mutex n;
vector<double> yLog;
mutex o;
vector<double> zLog; 
bool doneParsing = false, doneCalculating = false;

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

void askNameOfZ(string& z)
{
	cout << "Enter the name of the z variable: ";
	getline(cin, z);
}

void parseHeader(string line, string xName, int& xColumn, string yName, int& yColumn, string zName, int& zColumn)
{
	int columnNumber = 0;
	int delimiterIndex;
	string word;
	while ((delimiterIndex = line.find(',')) != string::npos)
	{
		word = line.substr(0, delimiterIndex);
		line = line.substr(delimiterIndex + 1);
		if (word == xName)
			xColumn = columnNumber;
		if (word == yName)
			yColumn = columnNumber;
		if (word == zName)
			zColumn = columnNumber;
		columnNumber++;
	}
	word = line;
	if (word == xName)
		xColumn = columnNumber;
	if (word == yName)
		yColumn = columnNumber;
	if (word == zName)
		zColumn = columnNumber;
}

void parseLog()
{
	ifstream file;
	string logLocation = "log xyz.csv"; // log of experimental data, parameters vs time
	string xAxis = "x-axis.txt"; // x-axis for output
	string yAxis = "y-axis.txt"; // y-axis for output
	string zFile = "z-name.txt";
	// get axes
	string line = "";
	file.open(xAxis);
	getline(file, line);
	string xName = line;;
	file.close();
	file.open(yAxis);
	getline(file, line);
	string yName = line;
	file.close();
	file.open(zFile);
	getline(file, line);
	file.close();
	string zName = line;
	// get column numbers from header
	cout << "Parsing log file..." << endl;
	file.open(logLocation);
	int xColumn;
	int yColumn;
	int zColumn;
	getline(file, line);
	parseHeader(line, xName, xColumn, yName, yColumn, zName, zColumn);

	// reserve memory for variables
	/*size_t numberOfData = 0;
	while (getline(file, line))
		numberOfData++;
	file.close();
	xLog.reserve(numberOfData); yLog.reserve(numberOfData); zLog.reserve(numberOfData);
	// store the variables in memory
	file.open(logLocation);
	getline(file, line);*/
	while (getline(file, line))
	{
		string word = "";
		int columnNumber = 0;
		int delimiterIndex;
		int prevDelimiter = 0;
		while ((delimiterIndex = line.find(',', prevDelimiter)) != string::npos)
		{
			if (columnNumber == xColumn)
				writeX(stod(line.substr(prevDelimiter, delimiterIndex - prevDelimiter)));
			if (columnNumber == yColumn)
				writeY(stod(line.substr(prevDelimiter, delimiterIndex - prevDelimiter)));
			if (columnNumber == zColumn)
				writeZ(stod(line.substr(prevDelimiter, delimiterIndex - prevDelimiter)));
			prevDelimiter = delimiterIndex + 1;
			columnNumber++;
		}
		if (columnNumber == xColumn)
			writeX(stod(line.substr(prevDelimiter)));
		if (columnNumber == yColumn)
			writeY(stod(line.substr(prevDelimiter)));
		if (columnNumber == zColumn)
			writeZ(stod(line.substr(prevDelimiter)));
	}
	file.close();
	doneParsing = true;
}

void calculateTable()
{

	// file locations
	ifstream file;
	string tableLocation = "table.txt"; // output location

	string xAxis = "x-axis.txt"; // x-axis for output
	string yAxis = "y-axis.txt"; // y-axis for output
	string zFile = "z-name.txt";
	// get axes
	string line = "";
	file.open(xAxis);
	getline(file, line);
	vector<double> nodesX;
	while (getline(file, line))
		nodesX.push_back(stod(line));
	file.close();
	file.open(yAxis);
	getline(file, line);
	vector<double> nodesY;
	while (getline(file, line))
		nodesY.push_back(stod(line));
	file.close();
	file.open(zFile);
	getline(file, line);
	file.close();
	double avgXBucketSizeSquared = abs(nodesX.back() - nodesX.front()) / nodesX.size();
	avgXBucketSizeSquared *= avgXBucketSizeSquared;
	double avgYBucketSizeSquared = abs(nodesY.back() - nodesY.front()) / nodesY.size();
	avgYBucketSizeSquared *= avgYBucketSizeSquared;
	// populate Z(x, y) table based on log
	vector<vector<long double>> tableZ;
	vector<vector<long double>> sumInvSqXyDist;
	for (double& x : nodesX)
	{
		vector<long double> row;
		for (double& y : nodesY)
			row.push_back(0.0);
		tableZ.push_back(row);
		sumInvSqXyDist.push_back(row);
	}
	while (zLog.size() < 2)
		continue;
	cout << "Calculating table coefficients..." << endl;
	for (int datum = 0; datum < zLog.size(); datum++)
	{
		// determine closest x-index
		int indexNow;
		int xIndex = 0;
		double difference = (double)INT_MAX;
		for (indexNow = 0; indexNow < nodesX.size(); indexNow++)
			if (abs(readX(datum) - nodesX[indexNow]) < difference)
			{
				xIndex = indexNow;
				difference = abs(readX(datum) - nodesX[indexNow]);
			}
		double differenceSquared = (difference * difference) / avgXBucketSizeSquared;
		//determine closest y-index
		difference = (double)INT_MAX;
		int yIndex = 0;
		for (indexNow = 0; indexNow < nodesY.size(); indexNow++)
			if (abs(readY(datum) - nodesY[indexNow]) < difference)
			{
				yIndex = indexNow;
				difference = abs(readY(datum) - nodesY[indexNow]);
			}
		differenceSquared += difference * difference / avgYBucketSizeSquared;
		tableZ[xIndex][yIndex] += readZ(datum) / differenceSquared;
		sumInvSqXyDist[xIndex][yIndex] += 1.0 / differenceSquared;
		while (datum + 2 >= zLog.size() && !doneParsing)
			continue;
	}
	for (int i = 0; i < nodesX.size(); i++)
		for (int j = 0; j < nodesY.size(); j++)
		{
			if (sumInvSqXyDist[i][j] == 0.0)
				tableZ[i][j] = 0.0;
			else
				tableZ[i][j] /= sumInvSqXyDist[i][j];
		}

	// save table to a file
	ofstream outputFile;
	outputFile.open(tableLocation);
	for (int i = 0; i < nodesX.size(); i++)
		outputFile << "\t" << to_string(nodesX[i]);
	for (int j = 0; j < nodesY.size(); j++)
	{
		outputFile << endl << to_string(nodesY[j]);
		for (int i = 0; i < nodesX.size(); i++)
			outputFile << "\t" << to_string(tableZ[i][j]);
	}
	outputFile.close();
	doneCalculating = true;
}

int main()
{
	clock_t startTime = clock();
	thread parsingThread(parseLog);
	calculateTable();
	parsingThread.detach();
	cout << "table created";
	cout << endl << "time elapsed = " << (double)(clock() - startTime) / CLOCKS_PER_SEC << " seconds";
	string line;
	cin >> line;
	return 0;
}
