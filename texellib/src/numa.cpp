/*
    Texel - A UCI chess engine.
    Copyright (C) 2014  Peter Österlund, peterosterlund2@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * numa.cpp
 *
 *  Created on: Jul 24, 2014
 *      Author: petero
 */

#include "numa.hpp"
#include "util/util.hpp"
#include "util/logger.hpp"
#include "bitBoard.hpp"

#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include <iostream>

#ifdef NUMA
#ifdef __WIN32__
#include <windows.h>
#else
#include <numa.h>
#endif
#endif

Numa&
Numa::instance() {
    static Numa numa;
    return numa;
}

Numa::Numa() {
#ifdef NUMA
#ifdef __WIN32__
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* buffer = NULL;
    DWORD returnLength = 0;
    while (true) {
        if (GetLogicalProcessorInformation(buffer, &returnLength))
            break;
        if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
            free(buffer);
            buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(returnLength);
            if (!buffer)
                return;
        } else {
            free(buffer);
            return;
        }
    }

    int threads = 0;
    int nodes = 0;
    int cores = 0;
    DWORD byteOffset = 0;
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION* ptr = buffer;
    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) {
        switch (ptr->Relationship) {
        case RelationNumaNode:
            nodes++;
            break;
        case RelationProcessorCore:
            cores++;
            threads += BitBoard::bitCount(ptr->ProcessorMask);
            break;
        default:
            break;
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }
    free(buffer);

    for (int n = 0; n < nodes; n++)
        for (int i = 0; i < cores / nodes; i++)
            threadToNode.push_back(n);
    for (int t = 0; t < threads - cores; t++)
        threadToNode.push_back(t % nodes);
#else
    if (numa_available() == -1)
        return;

    const int maxNode = numa_max_node();
    if (maxNode == 0)
        return;

    std::set<int> nodesToUse;
    bitmask* runNodes = numa_get_run_node_mask();
    int nBits = numa_bitmask_nbytes(runNodes) * 8;
    for (int i = 0; i < nBits; i++)
        if (numa_bitmask_isbitset(runNodes, i))
            nodesToUse.insert(i);

    std::map<int, NodeInfo> nodeInfo;
    std::string baseDir("/sys/devices/system/cpu");
    for (int i = 0; ; i++) {
        std::string cpuDir(baseDir + "/cpu" + num2Str(i));
        if (i > 0) {
            std::ifstream is(cpuDir + "/online");
            if (!is)
                break;
            std::string line;
            std::getline(is, line);
            if (!is || is.eof() || (line != "1"))
                continue;
        }

        int node = -1;
        for (int n = 0; n <= maxNode; n++) {
            std::ifstream is(cpuDir + "/node" + num2Str(n));
            if (is) {
                node = n;
                break;
            }
        }
        if (node < 0)
            continue;

        nodeInfo[node].node = node;
        nodeInfo[node].numThreads++;

        std::ifstream is(cpuDir + "/topology/thread_siblings_list");
        if (is) {
            std::string line;
            std::getline(is, line);
            if (is && !is.eof()) {
                auto pos = line.find_first_of(",-");
                if (pos != std::string::npos)
                    line = line.substr(0, pos);
                int num;
                if (str2Num(line, num)) {
                    if (i == num)
                        nodeInfo[node].numCores++;
                }
            }
        }
    }

    std::vector<NodeInfo> nodes;
    for (int node : nodesToUse) {
        auto it = nodeInfo.find(node);
        if (it != nodeInfo.end())
            nodes.push_back(it->second);
    }
    std::sort(nodes.begin(), nodes.end(), [](const NodeInfo& a, const NodeInfo& b) {
        if (a.numCores != b.numCores)
            return a.numCores > b.numCores;
        return a.numThreads > b.numThreads;
    });

    for (const NodeInfo& ni : nodes)
        for (int i = 0; i < ni.numCores; i++)
            threadToNode.push_back(ni.node);

    bool done = false;
    while (!done) {
        done = true;
        for (NodeInfo& ni : nodes) {
            if (ni.numThreads > ni.numCores) {
                threadToNode.push_back(ni.node);
                ni.numThreads--;
                done = false;
            }
        }
    }
#endif
#endif
}

void
Numa::disable() {
    threadToNode.clear();
}

int
Numa::nodeForThread(int threadNo) const {
#ifdef NUMA
    if (threadNo < (int)threadToNode.size())
        return threadToNode[threadNo];
#endif
    return -1;
}

void
Numa::bindThread(int threadNo) const {
#ifdef NUMA
    int node = nodeForThread(threadNo);
    if (node < 0)
        return;
//    Logger::log([&](std::ostream& os){os << "threadNo:" << threadNo << " node:" << node;});
#ifdef __WIN32__
    ULONGLONG mask;
    if (GetNumaNodeProcessorMask(node, &mask))
        SetThreadAffinityMask(GetCurrentThread(), mask);
#else
    numa_run_on_node(node);
#endif
#endif
}

bool
Numa::isMainNode(int threadNo) const {
    if (threadToNode.empty())
        return true; // Not NUMA hardware
    return nodeForThread(threadNo) == threadToNode[0];
}