#pragma once
#ifndef _GRAPH_H_ 
#include <iostream>
#include <cstdlib>
#include <vector>
class Graph
{
public:
	Graph();
	virtual ~Graph();
	void setSize(const int& size);
	int getSize() const;
	std::vector< std::vector<double> > getGraph();
	void setMaxWeight();
	void setMinWeight();
	void setEdges();
	std::vector<double> getEdges() const;
	double getMaxWeight() const;
	double getMinWeight() const;
	void pushBack(std::vector<double> edges,int i);
	double getPathWeight(std::vector<int>& path);
	double getPathQuality(std::vector<int>& path);
private:
	std::vector< std::vector<double> > mGraph;
	std::vector<double> mEdges;
	int mSize;
	double mMaxWeight;
	double mMinWeight;
};
std::istream& operator>>(std::istream& is, Graph& graph);

#endif // !_GRAPH_H_
