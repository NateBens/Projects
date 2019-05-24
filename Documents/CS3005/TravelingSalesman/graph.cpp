#include "graph.h"
Graph::Graph()
	:
	mSize(0), mGraph(),mMinWeight(), mMaxWeight(),mEdges(){

}
std::vector< std::vector<double> > Graph::getGraph(){
	return this->mGraph;
}

void Graph::setMaxWeight() {
	double max = (mGraph)[0][1];
	for (size_t i = 0; i < mGraph.size(); i++)
	{
		for (size_t j = 0; j < mGraph.size(); j++)
		{
			double x = (mGraph)[i][j];
			if (x > max)
			{
				max = x;
			}
		}
		
	}
	mMaxWeight = max * mSize;
}
void Graph::setMinWeight() {
	double min = (mGraph)[0][1];
	for (size_t i = 0; i < mGraph.size(); i++)
	{
		for (size_t j = 0; j < mGraph.size()-1; j++)
		{
			double x = (mGraph)[i][j];
			if (x < min && x>0.0)
			{
				min = x;
			}
		}
	}
	mMinWeight = min * mSize;
}
double Graph::getMaxWeight() const {
	return mMaxWeight;
}
double Graph::getMinWeight() const {
	return mMinWeight;
}
void Graph::setEdges() {
	for (size_t i = 0; i < mSize; i++)
	{
		for (size_t j = 0; j < mSize; j++)
		{
			mEdges.push_back(mGraph[i][j]);
		}
	}
}
std::vector<double> Graph::getEdges() const {
	return mEdges;
}


void Graph::setSize(const int& size) {
	mSize = size;
	mGraph.resize(size);
}
int Graph::getSize() const {
	return this->mSize;
}
void Graph::pushBack(std::vector<double> edges,int a) {
	mGraph[a-1]=edges;
}

double Graph::getPathWeight(std::vector<int>& path) {
	double path_weight = 0.0;
	size_t x;
	for (size_t i = 0; i < mGraph.size() - 1; i++)
	{
		path_weight += (mGraph)[path[i] - 1][path[i + 1] - 1];
		x = i;
	}
	path_weight += (mGraph)[path[x + 1] - 1][0];

	return path_weight;
}

double Graph::getPathQuality(std::vector<int>& path) {
	double path_weight = 0.0;
	size_t x;
	for (size_t i = 0; i < mGraph.size() - 1; i++)
	{
		path_weight += (mGraph)[path[i] - 1][path[i + 1] - 1];
		x = i;
	}
	path_weight += (mGraph)[path[x + 1] - 1][0];
	double path_quality = (1 - (path_weight - mMinWeight) / (mMaxWeight - mMinWeight));
	return path_quality;
}


Graph::~Graph()
{
	mGraph.clear();
}


std::istream& operator>>(std::istream& is, Graph& graph) {
	int size;
	is >> size;
	graph.setSize(size);
	std::vector<double> edges;
	edges.resize(size);
	int i, j, a, b; 
	double value;
	for (i = 0; i != (size); i++) {
		std::fill(edges.begin(), edges.end(), 0.0);
		for (j = 0; j != (size-1); j++)
		{
			is >> a;
			is >> b;
			is >> value;
			edges[b-1] = value;
			
		}
		graph.pushBack(edges, a);
	}
	//graph.setEdges();
	return is;
}