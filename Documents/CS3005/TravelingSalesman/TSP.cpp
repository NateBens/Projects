#include "graph.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <algorithm>


std::vector<int> TSPNumericalOrder(Graph& g) {
	std::vector<int> path;
	std::vector<std::vector<double>> graph = g.getGraph();
	path.push_back(1);
	int current_vertex = 1;
	for (size_t i = 0; i < graph.size() - 1; i++)
	{
		current_vertex += 1;
		path.push_back(current_vertex);
	}
	return path;
}

std::vector<int> getRandomCycle(Graph& g) {
	std::vector<int> path;
	path = TSPNumericalOrder(g);
	std::random_shuffle(path.begin()+1, path.end());
	return path;
}

std::vector<int> getRandomNeighbor(std::vector<int> path) {
	int x, y,temp;
	x = rand() % (path.size()-1)+1;
	y = rand() % (path.size()-1)+1;
	if (x==y)
	{
		while (x == y)
		{
			y = rand() % path.size();
		}
	}
	temp = path[x];
	path[x] = path[y];
	path[y] = temp;
	return path;

}


std::vector<int> TSPHillClimb(Graph& g) {

	std::vector<int> current_path, best_path,temp_path;
	double best_quality = -1.0;
	double temp_quality;
	current_path = getRandomCycle(g);
	double current_quality = g.getPathQuality(current_path);
	while (current_quality > best_quality)
	{
		best_quality = current_quality;
		best_path = current_path;
		for (size_t i = 0; i < 100; i++)
		{
			temp_path = getRandomNeighbor(current_path);
			temp_quality = g.getPathQuality(temp_path);
			if (temp_quality>current_quality)
			{
				current_quality = temp_quality;
				current_path = temp_path;
				break;
			}
		}
	}
	return best_path;
}

std::vector<int> randomRestartHillClimb(Graph& g){
	double best_quality = 0.0;
	double current_quality;
	std::vector<int> best_path, current_path;
	for (size_t i = 0; i < 100; i++)
	{
		current_path = TSPHillClimb(g);
		current_quality = g.getPathQuality(current_path);
		if (current_quality>best_quality)
		{
			best_path = current_path;
			best_quality = current_quality;
		}
		if (best_quality>=.8)
		{
			return best_path;
		}
	}
	return best_path;
}

//Path2 Function
std::vector<int> TSPOddsEvens(Graph& g) {
	std::vector<int> path;
	std::vector<std::vector<double>> graph = g.getGraph();
	path.push_back(1);
	int current_vertex = 1;
	for (size_t i = 0; i < ((graph.size() - 1)/2); i++)
	{
		current_vertex += 2;
		path.push_back(current_vertex);
	}
	path.push_back(2);
	current_vertex = 2;
	for (size_t i = 0; i < ((graph.size() - 1) / 2); i++)
	{
		current_vertex += 2;
		path.push_back(current_vertex);
	}
	return path;
}
std::vector<int> TSPThrees(Graph& g) {
	std::vector<int> path;
	std::vector<std::vector<double>> graph = g.getGraph();
	path.push_back(1);
	int current_vertex = 1;
	int last_vertex = graph.size();
	while (current_vertex<(last_vertex-2))
	{
		current_vertex += 3;
		path.push_back(current_vertex);
	}
		
	
	path.push_back(2);
	current_vertex = 2;
	while (current_vertex < (last_vertex - 2))
	{
		current_vertex += 3;
		path.push_back(current_vertex);
	}
	path.push_back(3);
	current_vertex = 3;
	while (current_vertex < (last_vertex - 2))
	{
		current_vertex += 3;
		path.push_back(current_vertex);
	}
	return path;
}


int main() {
	std::srand(unsigned(std::time(0)));
	////File Input
	/*std::cout << "Input Filename?: ";
	std::string infile;
	std::cin >> infile;
	std::ifstream fin(infile, std::ios::binary);
	Graph g;
	fin >> g;
	fin.close();*/
	//std Input
	Graph g;
	std::cin >> g;

	
	g.setMaxWeight();
	g.setMinWeight();

	//std::vector<int> path = randomRestartHillClimb(g);

	//1)Vertices in order
	std::vector<int> path1 = TSPNumericalOrder(g);
	double path_1_quality = g.getPathQuality(path1);
	//2)Vertices with all Odds in order than all evens in order
	std::vector<int> path2 = TSPOddsEvens(g);
	double path_2_quality = g.getPathQuality(path2);
	/*for (size_t i = 0; i < path2.size(); i++)
	{
		std::cout << path2[i] << ' ';
	}
	std::cout << path_2_weight << ' ' << path_2_quality << std::endl;

*/
	//3)By Threes
	std::vector<int> path3 = TSPThrees(g);
	double path_3_quality = g.getPathQuality(path3);
	/*for (size_t i = 0; i < path3.size(); i++)
	{
	std::cout << path3[i] << ' ';
	}
	std::cout << path_3_weight << ' ' << path_3_quality << std::endl;
*/
	double best_quality = path_1_quality;
	std::vector<int> best_path = path1;
	if (path_2_quality>best_quality)
	{
		best_path = path2;
		best_quality = path_2_quality;
	}
	if (path_3_quality>best_quality)
	{
		best_path = path3;
		best_quality = path_3_quality;
	}
	double best_weight = g.getPathWeight(best_path);
	for (size_t i = 0; i < best_path.size(); i++)
	{
		std::cout << best_path[i] << ' ';
	}
	std::cout << best_weight << ' ' << best_quality << std::endl;



	return 0;

}