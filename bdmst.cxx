//  File: bdmst.cxx
//  Author: Christopher Lee Jackson & Jason Jones
//  Description: This is our implementation of our ant based algorithm to aproximate the BDMST problem.

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cassert>
#include <algorithm>
#include <vector>
#include "Graph.cxx"
#include "Queue.h"
#include "BinaryHeap.h"
#include <cmath>
#include <cstring>
#include <stack>
#include <cstdlib>
#include "processFile.h"
#include "randomc.h"
#include "mersenne.cxx"

using namespace std;

typedef struct {
    int data; // initial vertex ant started on
    int nonMove;
    Vertex *location;
    Queue* vQueue;
}Ant;

typedef struct {
    double low;
    double high;
    Edge *assocEdge;
}Range;

//  Globals
const double P_UPDATE_EVAP = 1.05;
const double P_UPDATE_ENHA = 1.05;
const unsigned int TABU_MODIFIER = 5;
const int MAX_CYCLES = 2500; // change back to 2500
const int ONE_EDGE_OPT_BOUND = 500;
const int ONE_EDGE_OPT_MAX = 2500;
const int K = 250;

int instance = 0;
double loopCount = 0;
double evap_factor = .65;
double enha_factor = 1.5;
double maxCost = 0;
double minCost = std::numeric_limits<double>::infinity();

//  Variables for proportional selection
int32 seed = time(0), rand_pher;                                                        
TRandomMersenne rg(seed);

int cycles = 1;
int totalCycles = 1;

//  Prototypes
vector<Edge*> AB_DBMST(Graph *g, int d);
vector<Edge*> locOpt(Graph *g, int d, vector<Edge*> *best);
vector<Edge*> treeConstruct(Graph *g, int d);
bool asc_cmp_plevel(Edge *a, Edge *b);
bool des_cmp_cost(Edge *a, Edge *b);
bool asc_src(Edge *a, Edge *b);
bool asc_hub(Hub* a, Hub* b);
void move(Graph *g, Ant *a);
void updatePheromonesPerEdge(Graph *g);
void updatePheromonesGlobal(Graph *g, vector<Edge*> *best, bool improved);
void printEdge(Edge* e);
void resetItems(Graph* g, processFile p);
void compute(Graph* g, int d, processFile p, int instance);
void heapifyHubs(vector<Hub*> *hubs, vector<Edge*> *c, int & numEdges, BinaryHeap* heap);
bool replenish(vector<Edge*> *c, vector<Edge*> *v, const unsigned int & CAN_SIZE);
void connectHubs(Graph* gFull, Graph* g, vector<Edge*> *tree, unsigned int & treeCount, int d);
Edge* remEdge(vector<Edge*> best);
int find(vector<int> UF, int start);
int universalSearch(Graph* g, int start);
void populateVector(Graph* g, vector<Edge*> *v);
void getCandidateSet(vector<Edge*> *v, vector<Edge*> *c, const unsigned int & CAN_SIZE);
void getHubs(Graph* g, BinaryHeap* heap, vector<Edge*> *v, vector<Edge*> *c, vector<Edge*> *tree, vector<Hub*> *hubs, vector<Hub*> *treeHubs, const unsigned int & MAX_TREE_SIZE, const unsigned int & CAN_SIZE);
void getHubConnections(vector<Hub*> *treeHubs, vector<Edge*> *possConn, vector<Hub*> *hubs);
void opt_one_edge(Graph* g, Graph* gOpt, vector<Edge*> *tree, unsigned int treeCount, int d);

int main( int argc, char *argv[]) {
    //  Process input from command line
    if (argc != 4) {
        cerr << "Usage: ab_dbmst <input.file> <fileType> <diameter_bound>\n";
        cerr << "Where: fileType: r = random, e = estien or euc, o = other\n";
        cerr << "       diameter_bound: an integer i, s.t. 4 <= i < |v| \n";
        return 1;
    }
    char* fileName = new char[50];
    char* fileType = new char[2];
    int numInst = 0;
    strcpy(fileName,argv[1]);
    strcpy(fileType,argv[2]);
    int d;
    d = atoi(argv[3]);
    processFile p;
    //  Open file for reading
    ifstream inFile;
    inFile.open(fileName);
    assert(inFile.is_open());
    //  Process input file and get resulting graph
    Graph* g;
    cout << "INFO: Parameters: " << endl;
    cout << "INFO: P_UPDATE_EVAP: " << P_UPDATE_EVAP << ", P_UPDATE_ENHA: " << P_UPDATE_ENHA << ", Tabu_modifier: " << TABU_MODIFIER << endl;
    cout << "INFO: max_cycles: " << MAX_CYCLES << ", evap_factor: " << evap_factor << ", enha_factor: " << enha_factor << endl;
    cout << "INFO: Input file: " << fileName << ", Diameter Constraint: " << d << endl << endl;
    cout << "INFO: MT Seed: " << seed << endl;
    if(fileType[0] == 'e') {
        //cout << "USING e file type" << endl;
        inFile >> numInst;
        cout << "INFO: num_inst: " << numInst << endl;
        cout << "INFO: ";
        for(int i = 0; i < numInst; i++) {
            //cout << "Instance num: " << i+1 << endl;
            g = new Graph();
            p.processEFile(g, inFile);
            instance++;
            compute(g, d, p, i);
            resetItems(g, p);
        }
    }
    else if (fileType[0] == 'r') {
        //cout << "USING r file type" << endl;
        inFile >> numInst;
        for(int i = 0; i < numInst; i++) {
            //cout << "Instance num: " << i+1 << endl;
            g = new Graph();
            p.processRFile(g, inFile);
            compute(g, d, p, i);
            resetItems(g, p);
        }
    }
    else {
        //cout << "USING o file type" << endl;
        g = new Graph();
        p.processFileOld(g, inFile);
        compute(g, d, p, 0);
    }
    return 0;
}

void compute(Graph* g, int d, processFile p, int i) {
    maxCost = p.getMax();
    minCost = p.getMin();
    if((unsigned int) d > g->getNumNodes()) {
        cout << "No need to run this diameter test. Running MST will give you solution, since diameter is greater than number of nodes." << endl;
        exit(1);
    }
    vector<Edge*> best = AB_DBMST(g, d);
    //cout << "Size of best: " << best.size() << endl;
    //sort(best.begin(), best.end(), asc_src);
    //cout << "Best Tree num edges: " << best.size() << endl;
    cout << "Instance number: " << i << endl;
    for_each(best.begin(), best.end(), printEdge);
}

/*
*
*
*   For Each, Sort - Helper Functions
*
*/
bool asc_cmp_plevel(Edge *a, Edge *b) {
    return (a->pLevel < b->pLevel);
}

bool des_cmp_cost(Edge *a, Edge *b) {
    return (a->weight > b->weight);
}

bool asc_src(Edge* a, Edge* b) {
    return (a->a->data < b->a->data);
}

bool asc_hub(Hub* a, Hub* b) {
    return (a->vertId < b->vertId);
}

void printEdge(Edge* e) {
    cout << "RESULT: ";
    cout << e->a->data << " " << e->b->data << " " << e->weight << " " << e->pLevel << endl;
}

void resetItems(Graph* g, processFile p) {
    free(g);
    maxCost = 0;
    minCost = std::numeric_limits<double>::infinity();
    p.reset();
}

vector<Edge*> AB_DBMST(Graph *g, int d) {
    //  Local Declerations
    double bestCost = std::numeric_limits<double>::infinity();
    double treeCost = 0;
    bool newBest = false;
    const int s = 75;
    vector<Edge*> best, current;
    Vertex *vertWalkPtr;
    Edge *edgeWalkPtr, *pEdge;
    vector<Ant*> ants;
    vector<Edge*>::iterator e, ed, iedge1;

    Ant *a;
    //  Assign one ant to each vertex
    vertWalkPtr = g->getFirst();
    for (unsigned int i = 0; i < g->getNumNodes(); i++) {
        a = new Ant;
        a->data = i +1;
        a->nonMove = 0;
        a->location = vertWalkPtr;
        a->vQueue = new Queue(TABU_MODIFIER);
        ants.push_back(a);
        //  Initialize pheremone level of each edge, and set pUdatesNeeded to zero
        for ( e = vertWalkPtr->edges.begin() ; e < vertWalkPtr->edges.end(); e++ ) {
            edgeWalkPtr = *e;
            if (edgeWalkPtr->a == vertWalkPtr) {
                edgeWalkPtr->pUpdatesNeeded = 0;
                edgeWalkPtr->pLevel = (maxCost - edgeWalkPtr->weight) + ((maxCost - minCost) / 3);
            }
        }
        //  Done with this vertex's edges; move on to next vertex
        vertWalkPtr->updateVerticeWeight();
        vertWalkPtr = vertWalkPtr->pNextVert;
    }
    while (totalCycles <= 10000 && cycles <= MAX_CYCLES) { 
        //if(totalCycles % 25 == 0) 
            //cerr << "CYCLE " << totalCycles << endl;
        //  Exploration Stage
        for (int step = 1; step <= s; step++) {
            if (step == s/3 || step == (2*s)/3) {
                updatePheromonesPerEdge(g);
            }
            for (unsigned int j = 0; j < g->getNumNodes(); j++) {
                a = ants[j];
                move(g, a);
            }
        }
        //  Do we even need to still do this since we are using a circular queue?
        for(unsigned int w = 0; w < g->getNumNodes(); w++) {
            ants[w]->vQueue->reset(); //  RESET VISITED FOR EACH ANT
        }
        updatePheromonesPerEdge(g);
        //  Tree Construction Stage
        current = treeConstruct(g, d);
        //  Get new tree cost
        for ( ed = current.begin(); ed < current.end(); ed++ ) {
            edgeWalkPtr = *ed;
            treeCost+=edgeWalkPtr->weight;
        }
        if (treeCost < bestCost && (current.size() == g->getNumNodes() - 1)) {
            //cerr << "FOUND NEW BEST at cycle: " << totalCycles <<endl;
            best = current;
            bestCost = treeCost;
            newBest=true;
            if (totalCycles != 1)
                cycles = 0;
        } 
        if (cycles % 100 == 0) {
            if(newBest) {
                updatePheromonesGlobal(g, &best, false);
            } 
            else {
                updatePheromonesGlobal(g, &best, true);
            }
            newBest = false;
        }
        if (totalCycles % 500 == 0) {
            evap_factor *= P_UPDATE_EVAP; 
            enha_factor *= P_UPDATE_ENHA; 
        }
        totalCycles++;
        cycles++;
        treeCost = 0;
    }

    // Test if it meets the diameter bound.
    Graph* gTest = new Graph();
    //  add all vertices
    Vertex* pVert = g->getFirst();
    while(pVert) {
        gTest->insertVertex(pVert->data);
        pVert = pVert->pNextVert;
    }
    //  Now add edges to graph.
    for(iedge1 = best.begin(); iedge1 < best.end(); iedge1++) {
        pEdge = *iedge1;
        gTest->insertEdge(pEdge->a->data, pEdge->b->data, pEdge->weight, pEdge->pLevel);
    }
    gTest->root = g->root;
    gTest->oddRoot = g->oddRoot;
    cout << "RESULT: Diameter: " << gTest->testDiameter() << endl;
    cout << "RESULT" << instance << ": Cost: " << bestCost << endl;

    opt_one_edge(g, gTest, &best, best.size(), d);

    bestCost = 0;
    for ( ed = best.begin(); ed < best.end(); ed++ ) {
        edgeWalkPtr = *ed;
        bestCost+=edgeWalkPtr->weight;
    }
    
    cout << "RESULT" << instance << ": Cost: " << bestCost << endl;
    //  Reset items
    ants.clear();
    cycles = 1;
    totalCycles = 1;
    current.clear();
    treeCost = 0;
    bestCost = 0;
    //  Return best tree
    return best;
}

void updatePheromonesGlobal(Graph *g, vector<Edge*> *best, bool improved) {
    //  Local Variables
    srand((unsigned) time(NULL));
    double pMax = 1000*((maxCost - minCost) + (maxCost - minCost) / 3);
    double pMin = (maxCost - minCost)/3;
    Edge *e;
    double XMax = 0.3;
    double XMin = 0.1;
    double rand_evap_factor;
    double IP;
    vector<Edge*>::iterator ex;

    //  For each edge in the best tree update pheromone levels
    for ( ex = best->begin() ; ex < best->end(); ex++ ) {
        e = *ex;
        IP = (maxCost - e->weight) + ((maxCost - minCost) / 3);
        if (improved) {
        //  IMPROVEMENT so Apply Enhancement
            e->pLevel = enha_factor*e->pLevel;
        } else {
        //  NO IMPROVEMENTS so Apply Evaporation
            rand_evap_factor = XMin + rand() * (XMax - XMin) / RAND_MAX;
            e->pLevel = rand_evap_factor*e->pLevel;
        }
        //  Check if fell below minCost or went above maxCost
        if (e->pLevel > pMax) {
            e->pLevel = pMax - IP;
        } else if (e->pLevel < pMin) {
            e->pLevel = pMin + IP;
        }
    }
}


void updatePheromonesPerEdge(Graph *g) {
    //  Local Variables
    Vertex *vertWalkPtr = g->getFirst();
    double pMax = 1000*((maxCost - minCost) + (maxCost - minCost) / 3);
    double pMin = (maxCost - minCost)/3;
    double IP;
    vector<Edge*>::iterator ex;
    Edge *edgeWalkPtr;

    while (vertWalkPtr) {
        for ( ex = vertWalkPtr->edges.begin() ; ex < vertWalkPtr->edges.end(); ex++ ) {
            edgeWalkPtr = *ex;
            if (edgeWalkPtr->a == vertWalkPtr) {
                IP = (maxCost - edgeWalkPtr->weight) + ((maxCost - minCost) / 3);
                edgeWalkPtr->pLevel = (1 - evap_factor)*(edgeWalkPtr->pLevel)+(edgeWalkPtr->pUpdatesNeeded * IP);
                if (edgeWalkPtr->pLevel > pMax) {
                    edgeWalkPtr->pLevel = pMax - IP;
                } else if (edgeWalkPtr->pLevel < pMin) {
                    edgeWalkPtr->pLevel = pMin + IP;
                }
                //  Done updating this edge reset multiplier
                edgeWalkPtr->pUpdatesNeeded = 0;
            }
        }
        vertWalkPtr->updateVerticeWeight();
        vertWalkPtr = vertWalkPtr->pNextVert;
    }
}

vector<Edge*> treeConstruct(Graph *g, int d) {
    //  Local Variables
    vector<Edge*> v, c, tree, possConn;
    int numEdges = 0;
    vector<Hub*> hubs, treeHubs;
    Vertex *vert;
    Hub  *h, *pHub;
    Edge *pEdge;
    unsigned int treeCount = 0;
    vector<Edge*>::iterator iedge1, iedge2, iedge3, ie;
    vector<Hub*>::iterator ihubs1, ihubs2;
    BinaryHeap* heap = new BinaryHeap(g->getNumNodes());
    const unsigned int MAX_TREE_SIZE = g->getNumNodes() - 1;
    unsigned const int CAN_SIZE = g->getNumNodes();
    //  Logic

    //  Put all edges into a vector
    populateVector(g, &v);
    //  Sort edges in ascending order based upon pheromone level
    sort(v.begin(), v.end(), asc_cmp_plevel);
    //  Select 5n edges from the end of v( the highest pheromones edges) and put them into c.
    getCandidateSet(&v,&c,CAN_SIZE);
    //  Sort edges in descending order based upon cost
    sort(c.begin(), c.end(), des_cmp_cost);
    //  Fill vector of Hubs
    vert = g->getFirst();
    for(unsigned int index = 0; index < g->getNumNodes(); index++) {
        hubs.push_back(new Hub());
        hubs[index]->vertId = index + 1;
        hubs[index]->vert = vert;
        hubs[index]->inTree = false;
        hubs[index]->lenPath = 0;
        vert = vert->pNextVert;
    }
    heapifyHubs(&hubs, &c, numEdges, heap);
    //  Now get hubs
    getHubs(g, heap, &v, &c, &tree, &hubs, &treeHubs, MAX_TREE_SIZE, CAN_SIZE);
    //  Now that we have our hubs get vertices not yet in tree
    for(ihubs1 = hubs.begin(); ihubs1 < hubs.end(); ihubs1++) {
        h = *ihubs1;
        if(h->inTree == false) {
            h->inTree = true;
            treeHubs.push_back(h);
        }
    }   
    sort(treeHubs.begin(), treeHubs.end(), asc_hub );
    //  Now lets get the possible connections for them all.
    getHubConnections(&treeHubs, &possConn, &hubs);
    //  Create a new graph, using hubs as vertices, and the edges from possible hub connections.
    Graph* gHub = new Graph();
    //  add all vertices
    for(ihubs1 = treeHubs.begin(); ihubs1 < treeHubs.end(); ihubs1++) {
        pHub = *ihubs1;
        gHub->insertVertex(pHub->vertId, pHub);
    }
    //  Now add edges to graph.
    for(iedge1 = possConn.begin(); iedge1 < possConn.end(); iedge1++) {
        pEdge = *iedge1;
        gHub->insertEdge(pEdge->a->data, pEdge->b->data, pEdge->weight, pEdge->pLevel);
    }
    //  now construct a tree from this new graph
    connectHubs(g, gHub, &tree, treeCount, d - 2);
    //  Now that we are done cleanup
    delete gHub;
    hubs.clear();
    possConn.clear();
    treeHubs.clear();
    //  Return the tree
    return tree;
}

void getHubConnections(vector<Hub*> *treeHubs, vector<Edge*> *possConn, vector<Hub*> *hubs) {
    //  Local Variables
    Vertex *v1, *v2;
    Edge* pEdge;
    vector<Edge*>::iterator iedge3;

    //  Logic
    for(unsigned int i = 0; i < treeHubs->size(); i++) {
        v1 = (*treeHubs)[i]->vert;
        for(unsigned int j = i+1; j < treeHubs->size(); j++) {
            v2 = (*treeHubs)[j]->vert;
            for(iedge3 = v1->edges.begin(); iedge3 < v1->edges.end(); iedge3++) {
                pEdge = *iedge3;
                //cout << "compairing: v1.pEdge: " << pEdge->b->data << ":" << hubs[pEdge->b->data - 1]->inTree << " and v2: " << v2->data << ":"<< hubs[v2->data - 1]->inTree << endl;
                if(pEdge->b->data == v2->data && (*hubs)[pEdge->b->data - 1]->inTree == true && (*hubs)[v2->data - 1]->inTree == true)
                    possConn->push_back(pEdge);
            }
        }
    }
}

void getCandidateSet(vector<Edge*> *v, vector<Edge*> *c, unsigned const int & CAN_SIZE) {
    for (unsigned int i = 0; i < CAN_SIZE; i++) {
        if (v->empty()) {
            break;
        }
        c->push_back(v->back());
        v->pop_back();
    }
    return;
}

void getHubs(Graph* g, BinaryHeap* heap, vector<Edge*> *v, vector<Edge*> *c, vector<Edge*> *tree, vector<Hub*> *hubs, vector<Hub*> *treeHubs, const unsigned int & MAX_TREE_SIZE, const unsigned int & CAN_SIZE) {
    //  Local Variables
    unsigned int treeCount = 0; 
    stack<Edge*> temp;
    int numEdges = 0;
    Hub  *h;
    Edge *pEdge;
    int numHubs = 0;
    vector<Edge*>::iterator iedge1, iedge2, iedge3, ie;
    vector<Hub*>::iterator ihubs1, ihubs2;
    Hub *highHub = NULL;
    bool added = false, isEmpty = false;

    //  Logic
    while(treeCount != MAX_TREE_SIZE && !isEmpty) {
        if(heap->topSize() != 0) {
            //  cout << "while, treecount: " << treeCount << ", numHubs: " << numHubs << ", numEdges: " << numEdges << endl;
            //  Get highest degree v to make our initial hub (should be top of heap)
            highHub = heap->deleteMax();
            // cout << "Found new HUB: id: "<< highHub->vertId <<", #edges: " << highHub->edges.size() << endl; 
            //  Add all edges in highhub to tree.
            for(iedge1 = highHub->edges.begin(); iedge1 < highHub->edges.end(); iedge1++) {
                pEdge = *iedge1;
                //highHub->vert->inTree = true;
                // cout << "tyring to add edge: " << pEdge->b->data << "-" << pEdge->a->data << endl;
                if(!pEdge->inTree && treeCount < MAX_TREE_SIZE && !(pEdge->b->inTree == true || pEdge->a->inTree == true)) {
                    temp.push(pEdge);
                }
            }
            if(!temp.empty()) {
                numHubs++;
                highHub->lenPath = 1;
                treeHubs->push_back(highHub);
                highHub->inTree = true;
                added = true;
            }
            while(!temp.empty() && treeCount < MAX_TREE_SIZE) {
                pEdge = temp.top();
                temp.pop();
                pEdge->b->inTree = true;
                pEdge->a->inTree = true;
                pEdge->inTree = true;
                tree->push_back(pEdge);
                treeCount++;
                // cout << "added edge\n";
            }
            added = false;
            //  Get rid of edges that are already in tree or that would cause a loop
            for(iedge1 = highHub->edges.begin(); iedge1 < highHub->edges.end(); iedge1++) {
                pEdge = *iedge1;
                //  Update Source Vertex
                h = (*hubs)[pEdge->a->data - 1];
                if(h->vertId != highHub->vertId) {
                    numEdges -= h->edges.size();
                    h->edges.clear();
                    h->inTree = true;
                }
                //Update Destination
                h = (*hubs)[pEdge->b->data - 1];
                if(h->vertId != highHub->vertId) {
                    numEdges -= h->edges.size();
                    h->edges.clear();
                    h->inTree = true;
                }
            }
            //  delete highHub edges
            highHub->edges.clear();
            //  Update Heap
            heap->updateHeap();
        } else {
            isEmpty = replenish(c, v, CAN_SIZE);
            heapifyHubs(hubs, c, numEdges, heap);
        }
    }
}

void populateVector(Graph* g, vector<Edge*> *v) {
    //  Local Variables
    Vertex* vertWalkPtr;
    vector<Edge*>::iterator ie;
    Edge* edgeWalkPtr;
    //  Logic
    vertWalkPtr = g->getFirst();
    while (vertWalkPtr) {
        vertWalkPtr->treeDegree = 0;
        vertWalkPtr->inTree = false;
        vertWalkPtr->isConn = false;
        for ( ie = vertWalkPtr->edges.begin() ; ie < vertWalkPtr->edges.end(); ie++ ) {
            edgeWalkPtr = *ie;
            //  Dont want duplicate edges in listing
            if (edgeWalkPtr->a == vertWalkPtr) {
                edgeWalkPtr->inTree = false;
                v->push_back(edgeWalkPtr);
            }
        }
        vertWalkPtr = vertWalkPtr->pNextVert;
    }
}

void opt_one_edge(Graph* g, Graph* gOpt, vector<Edge*> *tree, unsigned int treeCount, int d) {
    Edge* edgeWalkPtr = NULL,* edgeTemp;
    int noImp = 0, tries = 0;
    vector<Edge*>::iterator e;
    double sum = 0.0;
    bool improved = false;
    Range* ranges[treeCount];
    int value;
    int i, s, q;
    int bsint = 0;
    Range* current;
    Range* temp;
    vector<Edge*> v;
    populateVector(g, &v);
    int diameter = 0;
    int numEdge = v.size();
    //initialize ranges
    for ( e = tree->begin(), q=0; e < tree->end(); e++, q++) {
        edgeWalkPtr = *e;
        Range* r = new Range();
        r->assocEdge = edgeWalkPtr;
        r->low = sum;
        sum += edgeWalkPtr->weight * 10000;
        r->high = sum;
        ranges[q] = r;
    }
    //  mark edges already in tree
    for ( e = tree->begin(); e < tree->end(); e++) {
        edgeWalkPtr = *e;
        edgeWalkPtr->inTree = true;
    }

    while (noImp < ONE_EDGE_OPT_BOUND && tries < ONE_EDGE_OPT_MAX) {
    //  Pick an edge to remove at random favoring edges with low pheremones
    //  First we determine the ranges for each edge
        value = rg.IRandom(0,((int) (sum+1))); // produce a random number between 0 and highest range + 1
        i = treeCount / 2;
        if (i%2 != 0)
            i++;
        bsint = i;
        s=0;
        while(true && s++ < 1000) {
        //cout << "oh shit " << i << "treeCount " << treeCount << endl  ;
            current = ranges[i];
            bsint -= bsint/2;
            if(value < current->low){
                i -= bsint;
            }
            else if(value >= current->high){
                i += bsint;
            }
            else{
            //  We will use this edge
            //cout << current->assocEdge->weight << endl;
                edgeWalkPtr = current->assocEdge;
            break;
            }
        }
        //  We now have an edge that we wish to remove.
        //  Remove the edge
        gOpt->removeEdge(edgeWalkPtr->a->data, edgeWalkPtr->b->data);
        //  Try adding new edge if it improves the tree and doesn't violate the diameter constraint keep it.
        for (int j=0; j < K; j++) {
        //  select a random edge, if its weight is less than the edge we just removed use it to try and improve tree.
            value = rg.IRandom(0, numEdge - 1);
            if (v[value]->weight < edgeWalkPtr->weight && v[value]->inTree == false ) {
                gOpt->insertEdge(v[value]->a->data, v[value]->b->data);
                diameter = gOpt->testDiameter();
                //cout << "the diameter after the addition is: " << diameter << endl;
                if (diameter > 0 && diameter <= d) {
                    cout << "IMPROVEMENT! Lets replace the edge." << endl;
                    edgeWalkPtr->inTree = false;
                    tree->erase(tree->begin() + i);
                    tree->push_back(v[value]);
                    v[value]->inTree = true;
                    // update the associated edge for the range that is being changed.
                    ranges[i]->assocEdge = v[value];
                    //  Update the ranges now that we have added a new edge into the tree
                    if(i != 0)
                        sum = ranges[i-1]->high;
                    else
                        sum = 0;
                    for ( e = tree->begin() + i, q = i ; e < tree->end(); e++, q++ ) {
                        edgeTemp = *e;
                        temp = ranges[q];
                        temp->low = sum;
                        sum += edgeTemp->weight * 10000;
                        temp->high = sum;
                    }
                    improved = true;
                    //cout << "im about to break\n";
                    break;
                } 
                else { 
                    //cout << "diameter failed remove the added edge.\n";
                    gOpt->removeEdge(v[value]->a->data, v[value]->b->data); 
                }
                if (improved) {
                    break;
                }
            }
            //cout << "END FOR\n";
        }
        //cout << "i broke.\n";
        //  Handle Counters
        if (improved) {
            noImp = 0;
            improved = false;
        }
        else {
            noImp++;
            gOpt->insertEdge(edgeWalkPtr->a->data, edgeWalkPtr->b->data);
        }
            tries++;
    }
    cout << "RESULT: Diameter: " << gOpt->testDiameter() << endl;
}

void connectHubs(Graph* gFull, Graph* g, vector<Edge*> *tree, unsigned int & treeCount, int d) {
    vector<Edge*>::iterator iEdge;
    Vertex *pVert, *pVertChk;
    Edge *pEdge, *pEdgeMin;
    bool done, back = false;
    double minEdge;
    int first;
    bool flag = true;
    //g->print();
    if(g->emptyGraph())
        return;
    //  Initialize graph
    pVert = g->getFirst();
    while(pVert) {
        pVert->inTree = false;
        for(iEdge = pVert->edges.begin(); iEdge < pVert->edges.end(); iEdge++) {
            pEdge = *iEdge;
            pEdge->inTree = false;
        }
        pVert = pVert->pNextVert;
    }
    //  Now derive spanning tree
    //pVert = g->getFirst();    
    pVert = g->getRand();
    first = pVert->data;
    pVert->inTree = true;
    pVert->depth = 0;
    gFull->root = first;
    done = false;
    while(!done) {
        //cout << "loop" << endl;
        done = true;
        pVertChk = pVert;
        minEdge = -1;
        pEdgeMin = NULL;
        back = false;
        while(!back) {
            //  walk through graph checking vertices in tree
            if( pVertChk->inTree == true) {
                for(iEdge = pVertChk->edges.begin(); iEdge < pVertChk->edges.end(); iEdge++) {
                    pEdge = *iEdge;
                    if(pEdge->b->inTree == false && pEdge->usable) {
                        done = false;
                        if(pEdge->pLevel > minEdge) {
                            minEdge = pEdge->pLevel;
                            pEdgeMin = pEdge;
                        }
                    }
                }
            }
            pVertChk = pVertChk->pNextVert;
            if(pVertChk == NULL){
                pVertChk = g->getFirst();
            }
            if(pVertChk->data == first){
                //cout << pVertChk->data;
                back = true;
            }
        }
        if(pEdgeMin) {
        //  Found edge to insert into the tree
            //cout << pEdgeMin->a->depth;
            if(pEdgeMin->a->depth < d / 2){
                //cout << "new edge" << endl;
                //cout << "Edge: " << pEdgeMin->a->data << ", " << pEdgeMin->b->data << endl;
                pEdgeMin->inTree = true;
                pEdgeMin->b->inTree = true;
                if(pEdgeMin->a->depth == 0 && (d % 2 != 0) && flag) {
                    pEdgeMin->b->depth = 0;
                    gFull->oddRoot = pEdgeMin->b->data;
                    flag = false;
                }
                else 
                    pEdgeMin->b->depth = pEdgeMin->a->depth + 1;
                tree->push_back(pEdgeMin);
                treeCount++;
            }
            else
                pEdgeMin->usable = false;
        }
    }
}

int find(vector<int> UF, int start){
    while(UF[start] != start)
        start = UF[start];

    return start;
}

bool replenish(vector<Edge*> *c, vector<Edge*> *v, const unsigned int & CAN_SIZE) {
    if(v->empty()) {
        return false;
    }
    for (unsigned int j = 0; j < CAN_SIZE; j++) {
        if (v->empty()) {
            break;
        }
        c->push_back(v->back());
        v->pop_back();
    }
    sort(c->begin(), c->end(), des_cmp_cost);
    return true;
}

void heapifyHubs(vector<Hub*> *hubs, vector<Edge*> *c, int & numEdges, BinaryHeap* heap) {
    //cout << "in foo\n";
    vector<Edge*>::iterator iedge1;
    vector<Hub*>::iterator ihubs2;
    Edge* pEdge;
    Hub* pHub;
    int vertIndex;
    //  Get Degree of each vertice in candidate set
    for(iedge1 = c->begin(); iedge1 < c->end(); iedge1++) {
        pEdge = *iedge1;
        //  Handle Source
        vertIndex = pEdge->a->data; // the vertice number uniquely identifies each vertice
        (*hubs)[vertIndex - 1]->edges.push_back(pEdge);
        //  Handle Destination
        vertIndex = pEdge->b->data; // the vertice number uniquely identifies each vertice
        (*hubs)[vertIndex - 1]->edges.push_back(pEdge);
        numEdges += 2;
    }
    c->clear();
    //  First get rid of vertices with zero edges from candidate set
    for(ihubs2 = hubs->begin(); ihubs2 < hubs->end(); ihubs2++) {
        pHub = *ihubs2;
        if(pHub->edges.size() != 0) {
            heap->insert(pHub);
        }
    }
}

void move(Graph *g, Ant *a) {
    Vertex* vertWalkPtr;
    Vertex* vDest;
    vertWalkPtr = a->location;
    Edge* edgeWalkPtr = NULL;
    int numMoves = 0;
    int size = 0, initialI = 0;
    bool alreadyVisited;
    vector<Edge*>::iterator e;
    double sum = 0.0;
    vector<Range> edges;
    int value;
    int i = 0, bsint = 0;
    Range* current;
    //  Determine Ranges for each edge
    for ( e = vertWalkPtr->edges.begin() ; e < vertWalkPtr->edges.end(); e++ ) {
        edgeWalkPtr = *e;
        Range r;
        r.assocEdge = edgeWalkPtr;
        r.low = sum;
        sum += edgeWalkPtr->pLevel + edgeWalkPtr->getOtherSide(vertWalkPtr)->sum; 
        r.high = sum;
        edges.push_back(r);
    }
    size = edges.size();
    initialI = size / 2;
    while (numMoves < 5) {
        //  Select an edge at random and proportional to its pheremone level
        value = rg.IRandom(0,((int) (sum))); // produce a random number between 0 and highest range + 1
        //value = fmod(rand(),(sum+1));
        //cout << value << endl;
        /*for (unsigned int i = 0; i < edges.size(); i++) {
            current = &edges[i];
            if (value >= current->low && value < current->high) {
                //  We will use this edge
                edgeWalkPtr = current->assocEdge;
                break;
            }
        }*/

        i = initialI;
        if(i%2 != 0)
            i++;
        bsint = i;
        while(true){
            current = &edges[i];
            bsint -= bsint/2;
            //cout << value << ", " << current->low << ", " << current->high << endl;
            //cout << "i = " << i << " bsint = " << bsint << endl;
            if(value < current->low){
                i -= bsint;
            }
            else if(value >= current->high){
                i += bsint;
            }
            else{
            //  We will use this edge
                //cout << current->assocEdge->weight << endl;
                edgeWalkPtr = current->assocEdge;
                break;
            } } 
        //  Check to see if the ant is stuck
        if (a->nonMove > 4) {
            a->vQueue->reset();
        }
        //  We have a randomly selected edge, if that edges hasnt already been visited by this ant traverse the edge
        vDest = edgeWalkPtr->getOtherSide(vertWalkPtr);
        alreadyVisited = false;
        for(unsigned int j = 0; j <= TABU_MODIFIER; j++) {
            if( a->vQueue->array[j] == vDest->data ) {
                // This ant has already visited this vertex
                alreadyVisited = true;
                break;
            }
        }
        if (!alreadyVisited) {
            edgeWalkPtr->pUpdatesNeeded++;
            a->location = vDest;
            //  the ant has moved so update where required
            a->nonMove = 0;
            a->vQueue->push(vDest->data);
            break;
        } else {
            // Already been visited, so we didn't make a move.
            a->nonMove++;
            numMoves++;
        }
    }
}
