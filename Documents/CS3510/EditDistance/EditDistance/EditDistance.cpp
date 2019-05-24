#include <string>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <utility>
#include <chrono>
#include <ctime>

using namespace std;

struct Result {
	string word1;
	string word2;
	int cost;
};



int minimum(int x, int y, int z)
{
	return min(min(x, y), z);
}

int editDistance(string s1, string s2) {
	
	int m = s1.length();
	int n = s2.length();
	int ** results = new int*[m + 1];
	for (int i = 0; i < m+1; i++) {
		results[i] = new int[n+1];
	}

	
	for (int i = 0; i <= m; i++)
	{
		for (int j = 0; j <= n; j++) {
			if (i == 0)
			{
				results[i][j] = j;
			}
			else if (j == 0)
			{
				results[i][j] = i;
			}
			else if (s1[i-1] == s2[j-1])
			{
				results[i][j] = results[i - 1][j - 1];
			}
			else
			{
				results[i][j] = 1 + minimum(results[i][j - 1], results[i-1][j], results[i-1][j - 1]);
			}
		}
	}
	return results[m][n];
}

int main(int argc, char **argv) {
	if (argc != 2)
	{
		cout << "This program requires exacty one argument, a filename" << endl;
		return -1;
	}
	vector<Result> results;
	vector<pair<string, string>> pairs;
	string line;
	string infilename = argv[1];
	ifstream infile(infilename);
	if (infile.is_open())
	{
		while (getline(infile,line))
		{
			istringstream ss(line);
			string word1;
			string word2;
			int i = 0;
			do
			{
				string word;
				ss >> word;
				if (i==0)
				{
					word1 = word;
				} 
				else if (i == 1)
				{
					word2 = word;
				}
				i += 1;
			} while (ss);
			int editcost = editDistance(word1, word2);
			//cout << editcost << endl;
			pair<string,string> wordpair = make_pair(word1, word2);
			pairs.push_back(wordpair);
		}
		infile.close();
	}
	else {
		cout << "Unable to open file" << endl;
		return -1;
	}
	
	vector<double> timings;
	int reps = 1;
	int cost;

	for (int i = 0; i < pairs.size(); i++)
	{
		clock_t t1 = clock();
		// algorithm to time goes here
		for (size_t j = 0; j < reps; j++) {
			cost = editDistance(pairs[i].first, pairs[i].second);
		}
		clock_t t2 = clock();
		clock_t dt = t2 - t1;
		double clocks_per_rep = ((double)dt) / reps;
		double seconds = clocks_per_rep / CLOCKS_PER_SEC;
		timings.push_back(seconds);
		Result result = { pairs[i].first,pairs[i].second,cost};
		//cout << pairs[i].first << " " << pairs[i].second << " " << cost << endl;
		results.push_back(result);
	}

	double sumtime=0;
	double avgtime;
	int sum=0;
	
	for (int i = 0; i < results.size(); i++)
	{	
		//cout << results[i].word1 << " " << results[i].word2 << " " << results[i].cost << endl;
		sum += results[i].cost;
		sumtime += timings[i];
		//cout << timings[i] << endl;
	}
	avgtime = sumtime / (double)timings.size();
	infilename.erase(0, 15);
	infilename.erase(infilename.end() - 4, infilename.end());
	cout << infilename << " , " << sum << " , " << avgtime << endl;


	return 0;
}