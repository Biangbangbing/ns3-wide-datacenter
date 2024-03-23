/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#undef PGO_TRAINING
#define PATH_TO_PGO_CONFIG "path_to_pgo_config"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/error-model.h"
#include "ns3/global-route-manager.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/packet.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/qbb-helper.h"
#include <ns3/rdma-client-helper.h>
#include <ns3/rdma-client.h>
#include <ns3/rdma-driver.h>
#include <ns3/rdma.h>
#include <ns3/sim-setting.h>
#include <ns3/switch-node.h>

#include <cmath>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

using namespace ns3;
using namespace std;

NS_LOG_COMPONENT_DEFINE("GENERIC_SIMULATION");

extern "C"
{
#include "cdf.h"
}
#define LINK_CAPACITY_BASE 1000000000 // 1Gbps

uint32_t cc_mode = 1;
bool enable_qcn = true;
uint32_t packet_payload_size = 1000, l2_chunk_size = 0, l2_ack_interval = 0;
double pause_time = 5, simulator_stop_time = 3.01;
std::string data_rate, link_delay, topology_file, flow_file, trace_file, trace_output_file;
std::string fct_output_file = "fct.txt";
std::string pfc_output_file = "pfc.txt";

double alpha_resume_interval = 55, rp_timer, ewma_gain = 1 / 16;
double rate_decrease_interval = 4;
uint32_t fast_recovery_times = 5;
std::string rate_ai, rate_hai, min_rate = "100Mb/s";
std::string dctcp_rate_ai = "1000Mb/s";

bool clamp_target_rate = false, l2_back_to_zero = false;
double error_rate_per_link = 0.0;
uint32_t has_win = 1;
uint32_t global_t = 1;
uint32_t mi_thresh = 5;
bool var_win = false, fast_react = true;
bool multi_rate = true;
bool sample_feedback = false;
double pint_log_base = 1.05;
double pint_prob = 1.0;
double u_target = 0.95;
uint32_t int_multi = 1;
bool rate_bound = true;

uint32_t ack_high_prio = 0;
uint64_t link_down_time = 0;
uint32_t link_down_A = 0, link_down_B = 0;

uint32_t enable_trace = 1;

uint32_t buffer_size = 16;

uint32_t qlen_dump_interval = 100000000, qlen_mon_interval = 100;
uint64_t qlen_mon_start = 2000000000, qlen_mon_end = 2100000000;
string qlen_mon_file;

unordered_map<uint64_t, uint32_t> rate2kmax, rate2kmin;
unordered_map<uint64_t, double> rate2pmax;

/************************************************
 * Runtime varibles
 ***********************************************/
std::ifstream topof, flowf, tracef;

NodeContainer n;
NetDeviceContainer switchToSwitchInterfaces;
std::map<uint32_t, std::map<uint32_t, std::vector<Ptr<QbbNetDevice>>>> switchToSwitch;

// vamsi
std::map<uint32_t, uint32_t> switchNumToId;
std::map<uint32_t, uint32_t> switchIdToNum;
std::map<uint32_t, NetDeviceContainer> switchUp;
std::map<uint32_t, NetDeviceContainer> switchDown;
// NetDeviceContainer switchUp[switch_num];
std::map<uint32_t, NetDeviceContainer> sourceNodes;

NodeContainer servers;
NodeContainer tors;

uint64_t nic_rate;

uint64_t maxRtt, maxBdp;

struct Interface
{
    uint32_t idx;
    bool up;
    uint64_t delay;
    uint64_t bw;

    Interface()
        : idx(0),
          up(false)
    {
    }
};

map<Ptr<Node>, map<Ptr<Node>, Interface>> nbr2if;
// Mapping destination to next hop for each node: <node, <dest, <nexthop0, ...> > >
map<Ptr<Node>, map<Ptr<Node>, vector<Ptr<Node>>>> nextHop;
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairDelay;
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairTxDelay;
map<uint32_t, map<uint32_t, uint64_t>> pairBw;
map<Ptr<Node>, map<Ptr<Node>, uint64_t>> pairBdp;
map<uint32_t, map<uint32_t, uint64_t>> pairRtt;

std::vector<Ipv4Address> serverAddress;

// maintain port number for each host pair
std::unordered_map<uint32_t, unordered_map<uint32_t, uint16_t>> portNumder;
std::unordered_map<uint32_t, unordered_map<uint32_t, uint16_t>> DestportNumder;

struct FlowInput
{
    uint64_t src, dst, pg, maxPacketCount, port, dport;
    double start_time;
    uint32_t idx;
};

FlowInput flow_input = {0};
uint32_t flow_num;

void
ReadFlowInput()
{
    if (flow_input.idx < flow_num)
    {
        flowf >> flow_input.src >> flow_input.dst >> flow_input.pg >> flow_input.dport >>
            flow_input.maxPacketCount >> flow_input.start_time;
        std::cout << "Flow " << flow_input.src << " " << flow_input.dst << " " << flow_input.pg
                  << " " << flow_input.dport << " " << flow_input.maxPacketCount << " "
                  << flow_input.start_time << " " << Simulator::Now().GetSeconds() << std::endl;
        NS_ASSERT(n.Get(flow_input.src)->GetNodeType() == 0 &&
                  n.Get(flow_input.dst)->GetNodeType() == 0);
    }
}

void
ScheduleFlowInputs()
{
    while (flow_input.idx < flow_num && Seconds(flow_input.start_time) <= Simulator::Now())
    {
        uint32_t port = portNumder[flow_input.src][flow_input.dst]++; // get a new port number
        RdmaClientHelper clientHelper(
            flow_input.pg,
            serverAddress[flow_input.src],
            serverAddress[flow_input.dst],
            port,
            flow_input.dport,
            flow_input.maxPacketCount,
            has_win
                ? (global_t == 1 ? maxBdp : pairBdp[n.Get(flow_input.src)][n.Get(flow_input.dst)])
                : 0,
            global_t == 1 ? maxRtt : pairRtt[flow_input.src][flow_input.dst],
            Simulator::GetMaximumSimulationTime());
        ApplicationContainer appCon = clientHelper.Install(n.Get(flow_input.src));
        //		appCon.Start(Seconds(flow_input.start_time));
        appCon.Start(
            Seconds(0)); // setting the correct time here conflicts with Sim time since there is
                         // already a schedule event that triggered this function at desired time.
        // get the next flow input
        flow_input.idx++;
        ReadFlowInput();
    }

    // schedule the next time to run this function
    if (flow_input.idx < flow_num)
    {
        Simulator::Schedule(Seconds(flow_input.start_time) - Simulator::Now(), ScheduleFlowInputs);
    }
    else
    { // no more flows, close the file
        flowf.close();
    }
}

Ipv4Address
node_id_to_ip(uint32_t id)
{
    return Ipv4Address(0x0b000001 + ((id / 256) * 0x00010000) + ((id % 256) * 0x00000100));
}

uint32_t
ip_to_node_id(Ipv4Address ip)
{
    return (ip.Get() >> 8) & 0xffff;
}

void
qp_finish(FILE* fout, Ptr<RdmaQueuePair> q)
{
    uint32_t sid = ip_to_node_id(q->sip);
    uint32_t did = ip_to_node_id(q->dip);
    uint64_t base_rtt = pairRtt[sid][did];
    uint64_t b = pairBw[sid][did];
    uint32_t total_bytes =
        q->m_size + ((q->m_size - 1) / packet_payload_size + 1) *
                        (CustomHeader::GetStaticWholeHeaderSize() -
                         IntHeader::GetStaticSize()); // translate to the minimum bytes required
                                                      // (with header but no INT)
    uint64_t standalone_fct = base_rtt + total_bytes * 8 * 1e9 / b;
    std::cout << "FCT " << (Simulator::Now() - q->startTime).GetNanoSeconds() << " size "
              << q->m_size << " baseFCT " << standalone_fct << std::endl;

    // remove rxQp from the receiver
    Ptr<Node> dstNode = n.Get(did);
    Ptr<RdmaDriver> rdma = dstNode->GetObject<RdmaDriver>();
    rdma->m_rdma->DeleteRxQp(q->sip.Get(), q->m_pg, q->sport);
}

void
get_pfc(FILE* fout, Ptr<QbbNetDevice> dev, uint32_t type)
{
    fprintf(fout,
            "%lu %u %u %u %u\n",
            Simulator::Now().GetTimeStep(),
            dev->GetNode()->GetId(),
            dev->GetNode()->GetNodeType(),
            dev->GetIfIndex(),
            type);
}

struct QlenDistribution
{
    vector<uint32_t> cnt; // cnt[i] is the number of times that the queue len is i KB

    void add(uint32_t qlen)
    {
        uint32_t kb = qlen / 1000;
        if (cnt.size() < kb + 1)
        {
            cnt.resize(kb + 1);
        }
        cnt[kb]++;
    }
};

map<uint32_t, map<uint32_t, QlenDistribution>> queue_result;

void
monitor_buffer(FILE* qlen_output, NodeContainer* n)
{
    for (uint32_t i = 0; i < n->GetN(); i++)
    {
        if (n->Get(i)->GetNodeType() == 1)
        { // is switch
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n->Get(i));
            if (queue_result.find(i) == queue_result.end())
            {
                queue_result[i];
            }
            for (uint32_t j = 1; j < sw->GetNDevices(); j++)
            {
                uint32_t size = 0;
                for (uint32_t k = 0; k < SwitchMmu::qCnt; k++)
                {
                    size += sw->m_mmu->egress_bytes[j][k];
                }
                queue_result[i][j].add(size);
            }
        }
    }
    if (Simulator::Now().GetTimeStep() % qlen_dump_interval == 0)
    {
        fprintf(qlen_output, "time: %lu\n", Simulator::Now().GetTimeStep());
        for (auto& it0 : queue_result)
        {
            for (auto& it1 : it0.second)
            {
                fprintf(qlen_output, "%u %u", it0.first, it1.first);
                auto& dist = it1.second.cnt;
                for (uint32_t i = 0; i < dist.size(); i++)
                {
                    fprintf(qlen_output, " %u", dist[i]);
                }
                fprintf(qlen_output, "\n");
            }
        }
        fflush(qlen_output);
    }
    if (Simulator::Now().GetTimeStep() < qlen_mon_end)
    {
        Simulator::Schedule(NanoSeconds(qlen_mon_interval), &monitor_buffer, qlen_output, n);
    }
}

void
CalculateRoute(Ptr<Node> host)
{
    // queue for the BFS.
    vector<Ptr<Node>> q;
    // Distance from the host to each node.
    map<Ptr<Node>, int> dis;
    map<Ptr<Node>, uint64_t> delay;
    map<Ptr<Node>, uint64_t> txDelay;
    map<Ptr<Node>, uint64_t> bw;
    // init BFS.
    q.push_back(host);
    dis[host] = 0;
    delay[host] = 0;
    txDelay[host] = 0;
    bw[host] = 0xffffffffffffffffLU;
    // BFS.
    for (int i = 0; i < (int)q.size(); i++)
    {
        Ptr<Node> now = q[i];
        int d = dis[now];
        for (auto it = nbr2if[now].begin(); it != nbr2if[now].end(); it++)
        {
            // skip down link
            if (!it->second.up)
            {
                continue;
            }
            Ptr<Node> next = it->first;
            // If 'next' have not been visited.
            if (dis.find(next) == dis.end())
            {
                dis[next] = d + 1;
                delay[next] = delay[now] + it->second.delay;
                txDelay[next] =
                    txDelay[now] + packet_payload_size * 1000000000LU * 8 / it->second.bw;
                bw[next] = std::min(bw[now], it->second.bw);
                // we only enqueue switch, because we do not want packets to go through host as
                // middle point
                if (next->GetNodeType() == 1)
                {
                    q.push_back(next);
                }
            }
            // if 'now' is on the shortest path from 'next' to 'host'.
            if (d + 1 == dis[next])
            {
                nextHop[next][host].push_back(now);
            }
        }
    }
    for (const auto& it : delay)
    {
        pairDelay[it.first][host] = it.second;
    }
    for (const auto& it : txDelay)
    {
        pairTxDelay[it.first][host] = it.second;
    }
    for (const auto& it : bw)
    {
        pairBw[it.first->GetId()][host->GetId()] = it.second;
    }
}

void
CalculateRoutes(NodeContainer& n)
{
    for (int i = 0; i < (int)n.GetN(); i++)
    {
        Ptr<Node> node = n.Get(i);
        if (node->GetNodeType() == 0)
        {
            CalculateRoute(node);
        }
    }
}

void
SetRoutingEntries()
{
    // For each node.
    for (auto i = nextHop.begin(); i != nextHop.end(); i++)
    {
        Ptr<Node> node = i->first;
        auto& table = i->second;
        for (auto j = table.begin(); j != table.end(); j++)
        {
            // The destination node.
            Ptr<Node> dst = j->first;
            // The IP address of the dst.
            Ipv4Address dstAddr = dst->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
            // The next hops towards the dst.
            vector<Ptr<Node>> nexts = j->second;
            for (int k = 0; k < (int)nexts.size(); k++)
            {
                Ptr<Node> next = nexts[k];
                uint32_t interface = nbr2if[node][next].idx;
                if (node->GetNodeType() == 1)
                {
                    DynamicCast<SwitchNode>(node)->AddTableEntry(dstAddr, interface);
                }
                else
                {
                    node->GetObject<RdmaDriver>()->m_rdma->AddTableEntry(dstAddr, interface);
                }
            }
        }
    }
}

// take down the link between a and b, and redo the routing
void
TakeDownLink(NodeContainer n, Ptr<Node> a, Ptr<Node> b)
{
    if (!nbr2if[a][b].up)
    {
        return;
    }
    // take down link between a and b
    nbr2if[a][b].up = nbr2if[b][a].up = false;
    nextHop.clear();
    CalculateRoutes(n);
    // clear routing tables
    for (uint32_t i = 0; i < n.GetN(); i++)
    {
        if (n.Get(i)->GetNodeType() == 1)
        {
            DynamicCast<SwitchNode>(n.Get(i))->ClearTable();
        }
        else
        {
            n.Get(i)->GetObject<RdmaDriver>()->m_rdma->ClearTable();
        }
    }
    DynamicCast<QbbNetDevice>(a->GetDevice(nbr2if[a][b].idx))->TakeDown();
    DynamicCast<QbbNetDevice>(b->GetDevice(nbr2if[b][a].idx))->TakeDown();
    // reset routing table
    SetRoutingEntries();

    // redistribute qp on each host
    for (uint32_t i = 0; i < n.GetN(); i++)
    {
        if (n.Get(i)->GetNodeType() == 0)
        {
            n.Get(i)->GetObject<RdmaDriver>()->m_rdma->RedistributeQp();
        }
    }
}

uint64_t
get_nic_rate(NodeContainer& n)
{
    for (uint32_t i = 0; i < n.GetN(); i++)
    {
        if (n.Get(i)->GetNodeType() == 0)
        {
            return DynamicCast<QbbNetDevice>(n.Get(i)->GetDevice(1))->GetDataRate().GetBitRate();
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Applications */

double
poission_gen_interval(double avg_rate)
{
    if (avg_rate > 0)
    {
        return -logf(1.0 - (double)rand() / RAND_MAX) / avg_rate;
    }
    else
    {
        return 0;
    }
}

template <typename T>
T
rand_range(T min, T max)
{
    return min + ((double)max - min) * rand() / RAND_MAX;
}

uint32_t numPriorities;
uint32_t prioRand = 0;

#define QUERY_DATA 300000

int tar = 0;

int
get_target_leaf(int leafCount)
{
    tar += 1;
    if (tar == leafCount)
    {
        tar = 0;
        return tar;
    }
    return tar;
}

uint64_t PORT_START = 4000;

uint32_t FAN = 5;

void
install_applications_queryNew(int fromLeafId,
                              double requestRate,
                              uint32_t requestSize,
                              struct cdf_table* cdfTable,
                              long& flowCount,
                              long& totalFlowSize,
                              int SERVER_COUNT,
                              int LEAF_COUNT,
                              double START_TIME,
                              double END_TIME,
                              double FLOW_LAUNCH_END_TIME)
{
    uint32_t fan = FAN; // SERVER_COUNT/rand_range(1,SERVER_COUNT);
    for (int i = 0; i < SERVER_COUNT; i++)
    {
        int fromServerIndexX = fromLeafId * SERVER_COUNT + i;

        double startTime = START_TIME + poission_gen_interval(requestRate);
        while (startTime < FLOW_LAUNCH_END_TIME && startTime > START_TIME)
        {
            int leaftarget = fromLeafId;
            while (leaftarget == fromLeafId)
            {
                leaftarget = get_target_leaf(LEAF_COUNT); // rand_range(0,LEAF_COUNT);
            }

            uint16_t port = PORT_START++; // uint16_t (rand_range (PORT_START, PORT_END));

            int destServerIndex = fromServerIndexX;

            uint32_t query = 0;
            uint32_t flowSize =
                double(requestSize) / double(fan); // QUERY_DATA/fan;//gen_random_cdf (cdfTable);

            for (int r = 0; r < fan; r++)
            {
                uint32_t fromServerIndex = SERVER_COUNT * leaftarget + rand_range(0, SERVER_COUNT);

                if (DestportNumder[fromServerIndex][destServerIndex] == UINT16_MAX - 1)
                {
                    DestportNumder[fromServerIndex][destServerIndex] = rand_range(10000, 11000);
                }

                if (DestportNumder[fromServerIndex][destServerIndex] == UINT16_MAX - 1)
                {
                    portNumder[fromServerIndex][destServerIndex] = rand_range(10000, 11000);
                }

                uint16_t dport =
                    DestportNumder[fromServerIndex][destServerIndex]++; // uint16_t (rand_range
                                                                        // (PORT_START, PORT_END));
                uint16_t sport = portNumder[fromServerIndex][destServerIndex]++;

                totalFlowSize += flowSize;
                query += flowSize;
                flowCount++;

                RdmaClientHelper clientHelper(
                    3,
                    serverAddress[fromServerIndex],
                    serverAddress[destServerIndex],
                    sport,
                    dport,
                    flowSize,
                    has_win
                        ? (global_t == 1 ? maxBdp
                                         : pairBdp[n.Get(fromServerIndex)][n.Get(destServerIndex)])
                        : 0,
                    global_t == 1 ? maxRtt : pairRtt[fromServerIndex][destServerIndex],
                    Simulator::GetMaximumSimulationTime());
                ApplicationContainer appCon = clientHelper.Install(n.Get(fromServerIndex));
                std::cout << " from " << fromServerIndex << " to " << destServerIndex
                          << " fromLeadId " << fromLeafId << " serverCount " << SERVER_COUNT
                          << " leafCount " << LEAF_COUNT << std::endl;
                //		appCon.Start(Seconds(flow_input.start_time));
                appCon.Start(Seconds(startTime));
            }
            startTime += poission_gen_interval(requestRate);
        }
    }
}

void
install_applications(int fromLeafId,
                     double requestRate,
                     struct cdf_table* cdfTable,
                     long& flowCount,
                     long& totalFlowSize,
                     int SERVER_COUNT,
                     int LEAF_COUNT,
                     double START_TIME,
                     double END_TIME,
                     double FLOW_LAUNCH_END_TIME)
{
    for (int i = 0; i < SERVER_COUNT; i++)
    {
        int fromServerIndex = fromLeafId * SERVER_COUNT + i;

        double startTime = START_TIME + poission_gen_interval(requestRate);
        while (startTime < FLOW_LAUNCH_END_TIME && startTime > START_TIME)
        {
            int destServerIndex = fromServerIndex;
            while (destServerIndex >= fromLeafId * SERVER_COUNT &&
                   destServerIndex < fromLeafId * SERVER_COUNT + SERVER_COUNT &&
                   destServerIndex == fromServerIndex)
            {
                destServerIndex = rand_range(fromLeafId * SERVER_COUNT, SERVER_COUNT * LEAF_COUNT);
            }

            if (DestportNumder[fromServerIndex][destServerIndex] == UINT16_MAX - 1)
            {
                DestportNumder[fromServerIndex][destServerIndex] = rand_range(10000, 11000);
            }

            if (DestportNumder[fromServerIndex][destServerIndex] == UINT16_MAX - 1)
            {
                portNumder[fromServerIndex][destServerIndex] = rand_range(10000, 11000);
            }

            uint16_t dport =
                DestportNumder[fromServerIndex]
                              [destServerIndex]++; // uint16_t (rand_range (PORT_START, PORT_END));
            uint16_t sport = portNumder[fromServerIndex][destServerIndex]++;

            uint64_t flowSize = gen_random_cdf(cdfTable);
            while (flowSize == 0)
            {
                flowSize = gen_random_cdf(cdfTable);
            }

            totalFlowSize += flowSize;
            flowCount += 1;

            RdmaClientHelper clientHelper(
                3,
                serverAddress[fromServerIndex],
                serverAddress[destServerIndex],
                sport,
                dport,
                flowSize,
                has_win ? (global_t == 1 ? maxBdp
                                         : pairBdp[n.Get(fromServerIndex)][n.Get(destServerIndex)])
                        : 0,
                global_t == 1 ? maxRtt : pairRtt[fromServerIndex][destServerIndex],
                Simulator::GetMaximumSimulationTime());
            ApplicationContainer appCon = clientHelper.Install(n.Get(fromServerIndex));
            std::cout << " from " << fromServerIndex << " to " << destServerIndex << " fromLeadId "
                      << fromLeafId << " serverCount " << SERVER_COUNT << " leafCount "
                      << LEAF_COUNT << std::endl;
            //		appCon.Start(Seconds(flow_input.start_time));
            appCon.Start(Seconds(startTime));

            startTime += poission_gen_interval(requestRate);
        }
    }
    std::cout << "Finished installation of applications from leaf-" << fromLeafId << std::endl;
}

std::vector<uint32_t> maxBinbufferOccupancy(100, 0); // ugly initialization

uint32_t binBuffer = 0;
uint32_t maxBin = 100;

void
printBuffer(NodeContainer switches, double delay)
{
    binBuffer++;
    for (uint32_t i = 0; i < switches.GetN(); i++)
    {
        if (switches.Get(i)->GetNodeType())
        { // switch
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(switches.Get(i));
            uint32_t totalSize = 0;
            for (uint32_t j = 0; j < sw->GetNDevices(); j++)
            {
                uint32_t size = 0;
                for (uint32_t k = 0; k < SwitchMmu::qCnt; k++)
                {
                    size += sw->m_mmu->egress_bytes[j][k];
                }
                totalSize += size;
            }
            if (binBuffer < maxBin)
            {
                if (maxBinbufferOccupancy[i] < totalSize)
                {
                    maxBinbufferOccupancy[i] = totalSize;
                }
            }
            else
            {
                std::cout << "switch " << i << " qlen " << maxBinbufferOccupancy[i] << " time "
                          << Simulator::Now().GetSeconds() << std::endl;
                maxBinbufferOccupancy[i] = 0;
            }
        }
    }
    if (binBuffer == maxBin)
    {
        binBuffer = 0;
    }

    Simulator::Schedule(Seconds(delay), printBuffer, switches, delay);
}

/******************************************************************************************************************************************************************************************************/

int
main(int argc, char* argv[])
{
    clock_t begint;
    clock_t endt;
    begint = clock();
    std::ifstream conf;
    bool wien = true;       // wien enables PowerTCP.
    bool delayWien = false; // delayWien enables Theta-PowerTCP (delaypowertcp)

    uint32_t SERVER_COUNT = 32;
    uint32_t LEAF_COUNT = 2; // LEAF and SPINE correspond to a single pod. Leafs are ToR switches
                             // and Spine are AGG switches. Count is within a single pod.
    uint32_t SPINE_COUNT = 2;
    uint64_t LEAF_SERVER_CAPACITY = 25;
    uint64_t SPINE_LEAF_CAPACITY = 100;

    double START_TIME = 0.1;
    double END_TIME = 6;
    double FLOW_LAUNCH_END_TIME = 5;

    double load = 0.2;

    uint32_t requestSize = 1000000;
    double queryRequestRate = 1;
    uint32_t incast = 5;

    uint32_t algorithm = 3;
    uint32_t windowCheck = 1;

    std::string confFile = "/home/vamsi/src/phd/codebase/ns3-datacenter/simulator/ns-3.39/examples/"
                           "PowerTCP/config-workload.txt";
    std::string cdfFileName = "/home/vamsi/src/phd/codebase/ns3-datacenter/simulator/ns-3.39/"
                              "examples/PowerTCP/websearch.txt";

    unsigned randomSeed = 7;

    std::cout << confFile;
    CommandLine cmd;
    cmd.AddValue("conf", "config file path", confFile);
    cmd.AddValue("wien", "enable wien --> wien enables PowerTCP.", wien);
    cmd.AddValue("delayWien",
                 "enable wien delay --> delayWien enables Theta-PowerTCP (delaypowertcp) ",
                 delayWien);
    cmd.AddValue("randomSeed", "Random seed, 0 for random generated", randomSeed);

    cmd.AddValue("SERVER_COUNT",
                 "servers per tor. Please specify the correct value according to the topology file "
                 "information",
                 SERVER_COUNT);
    cmd.AddValue("LEAF_COUNT", "number of ToRs that receive traffic", LEAF_COUNT);
    cmd.AddValue("SPINE_COUNT", "number of ToRs that receive traffic", SPINE_COUNT);
    cmd.AddValue("LEAF_SERVER_CAPACITY", "tor to server capacity", LEAF_SERVER_CAPACITY);
    cmd.AddValue("SPINE_LEAF_CAPACITY", "tor to aggregation switch capacity", SPINE_LEAF_CAPACITY);

    cmd.AddValue("START_TIME", "sim start time", START_TIME);
    cmd.AddValue("END_TIME", "sim end time", END_TIME);
    cmd.AddValue("FLOW_LAUNCH_END_TIME", "flow launch process end time", FLOW_LAUNCH_END_TIME);
    cmd.AddValue("cdfFileName", "File name for flow distribution", cdfFileName);
    cmd.AddValue("load",
                 "Load on the links from ToR to Agg switches (This is the only place with "
                 "Over-subscription and is the correct place to load, 0.0 - 1.0",
                 load);

    cmd.AddValue("request", "Query Size in Bytes", requestSize);
    cmd.AddValue("queryRequestRate", "Query request rate (poisson arrivals)", queryRequestRate);

    cmd.AddValue("algorithm",
                 "specify CC mode. This is added for my convinience since I prefer cmd rather than "
                 "parsing files.",
                 algorithm);
    cmd.AddValue("windowCheck", "windowCheck", windowCheck);

    cmd.AddValue("incast", "incast", incast);

    cmd.Parse(argc, argv);

    SPINE_LEAF_CAPACITY = SPINE_LEAF_CAPACITY * LINK_CAPACITY_BASE;
    LEAF_SERVER_CAPACITY = LEAF_SERVER_CAPACITY * LINK_CAPACITY_BASE;

    conf.open(confFile.c_str());
    while (!conf.eof())
    {
        std::string key;
        conf >> key;

        if (key == "ENABLE_QCN")
        {
            uint32_t v;
            conf >> v;
            enable_qcn = v;
            if (enable_qcn)
            {
                std::cout << "ENABLE_QCN\t\t\t"
                          << "Yes"
                          << "\n";
            }
            else
            {
                std::cout << "ENABLE_QCN\t\t\t"
                          << "No"
                          << "\n";
            }
        }
        else if (key == "CLAMP_TARGET_RATE")
        {
            uint32_t v;
            conf >> v;
            clamp_target_rate = v;
            if (clamp_target_rate)
            {
                std::cout << "CLAMP_TARGET_RATE\t\t"
                          << "Yes"
                          << "\n";
            }
            else
            {
                std::cout << "CLAMP_TARGET_RATE\t\t"
                          << "No"
                          << "\n";
            }
        }
        else if (key == "PAUSE_TIME")
        {
            double v;
            conf >> v;
            pause_time = v;
            std::cout << "PAUSE_TIME\t\t\t" << pause_time << "\n";
        }
        else if (key == "DATA_RATE")
        {
            std::string v;
            conf >> v;
            data_rate = v;
            std::cout << "DATA_RATE\t\t\t" << data_rate << "\n";
        }
        else if (key == "LINK_DELAY")
        {
            std::string v;
            conf >> v;
            link_delay = v;
            std::cout << "LINK_DELAY\t\t\t" << link_delay << "\n";
        }
        else if (key == "PACKET_PAYLOAD_SIZE")
        {
            uint32_t v;
            conf >> v;
            packet_payload_size = v;
            std::cout << "PACKET_PAYLOAD_SIZE\t\t" << packet_payload_size << "\n";
        }
        else if (key == "L2_CHUNK_SIZE")
        {
            uint32_t v;
            conf >> v;
            l2_chunk_size = v;
            std::cout << "L2_CHUNK_SIZE\t\t\t" << l2_chunk_size << "\n";
        }
        else if (key == "L2_ACK_INTERVAL")
        {
            uint32_t v;
            conf >> v;
            l2_ack_interval = v;
            std::cout << "L2_ACK_INTERVAL\t\t\t" << l2_ack_interval << "\n";
        }
        else if (key == "L2_BACK_TO_ZERO")
        {
            uint32_t v;
            conf >> v;
            l2_back_to_zero = v;
            if (l2_back_to_zero)
            {
                std::cout << "L2_BACK_TO_ZERO\t\t\t"
                          << "Yes"
                          << "\n";
            }
            else
            {
                std::cout << "L2_BACK_TO_ZERO\t\t\t"
                          << "No"
                          << "\n";
            }
        }
        else if (key == "TOPOLOGY_FILE")
        {
            std::string v;
            conf >> v;
            topology_file = v;
            std::cout << "TOPOLOGY_FILE\t\t\t" << topology_file << "\n";
        }
        else if (key == "FLOW_FILE")
        {
            std::string v;
            conf >> v;
            flow_file = v;
            std::cout << "FLOW_FILE\t\t\t" << flow_file << "\n";
        }
        else if (key == "TRACE_FILE")
        {
            std::string v;
            conf >> v;
            trace_file = v;
            std::cout << "TRACE_FILE\t\t\t" << trace_file << "\n";
        }
        else if (key == "TRACE_OUTPUT_FILE")
        {
            std::string v;
            conf >> v;
            trace_output_file = v;
            if (argc > 2)
            {
                trace_output_file = trace_output_file + std::string(argv[2]);
            }
            std::cout << "TRACE_OUTPUT_FILE\t\t" << trace_output_file << "\n";
        }
        else if (key == "SIMULATOR_STOP_TIME")
        {
            double v;
            conf >> v;
            simulator_stop_time = v;
            std::cout << "SIMULATOR_STOP_TIME\t\t" << simulator_stop_time << "\n";
        }
        else if (key == "ALPHA_RESUME_INTERVAL")
        {
            double v;
            conf >> v;
            alpha_resume_interval = v;
            std::cout << "ALPHA_RESUME_INTERVAL\t\t" << alpha_resume_interval << "\n";
        }
        else if (key == "RP_TIMER")
        {
            double v;
            conf >> v;
            rp_timer = v;
            std::cout << "RP_TIMER\t\t\t" << rp_timer << "\n";
        }
        else if (key == "EWMA_GAIN")
        {
            double v;
            conf >> v;
            ewma_gain = v;
            std::cout << "EWMA_GAIN\t\t\t" << ewma_gain << "\n";
        }
        else if (key == "FAST_RECOVERY_TIMES")
        {
            uint32_t v;
            conf >> v;
            fast_recovery_times = v;
            std::cout << "FAST_RECOVERY_TIMES\t\t" << fast_recovery_times << "\n";
        }
        else if (key == "RATE_AI")
        {
            std::string v;
            conf >> v;
            rate_ai = v;
            std::cout << "RATE_AI\t\t\t\t" << rate_ai << "\n";
        }
        else if (key == "RATE_HAI")
        {
            std::string v;
            conf >> v;
            rate_hai = v;
            std::cout << "RATE_HAI\t\t\t" << rate_hai << "\n";
        }
        else if (key == "ERROR_RATE_PER_LINK")
        {
            double v;
            conf >> v;
            error_rate_per_link = v;
            std::cout << "ERROR_RATE_PER_LINK\t\t" << error_rate_per_link << "\n";
        }
        else if (key == "CC_MODE")
        {
            conf >> cc_mode;
            std::cout << "CC_MODE\t\t" << cc_mode << '\n';
        }
        else if (key == "RATE_DECREASE_INTERVAL")
        {
            double v;
            conf >> v;
            rate_decrease_interval = v;
            std::cout << "RATE_DECREASE_INTERVAL\t\t" << rate_decrease_interval << "\n";
        }
        else if (key == "MIN_RATE")
        {
            conf >> min_rate;
            std::cout << "MIN_RATE\t\t" << min_rate << "\n";
        }
        else if (key == "FCT_OUTPUT_FILE")
        {
            conf >> fct_output_file;
            std::cout << "FCT_OUTPUT_FILE\t\t" << fct_output_file << '\n';
        }
        else if (key == "HAS_WIN")
        {
            conf >> has_win;
            std::cout << "HAS_WIN\t\t" << has_win << "\n";
        }
        else if (key == "GLOBAL_T")
        {
            conf >> global_t;
            std::cout << "GLOBAL_T\t\t" << global_t << '\n';
        }
        else if (key == "MI_THRESH")
        {
            conf >> mi_thresh;
            std::cout << "MI_THRESH\t\t" << mi_thresh << '\n';
        }
        else if (key == "VAR_WIN")
        {
            uint32_t v;
            conf >> v;
            var_win = v;
            std::cout << "VAR_WIN\t\t" << v << '\n';
        }
        else if (key == "FAST_REACT")
        {
            uint32_t v;
            conf >> v;
            fast_react = v;
            std::cout << "FAST_REACT\t\t" << v << '\n';
        }
        else if (key == "U_TARGET")
        {
            conf >> u_target;
            std::cout << "U_TARGET\t\t" << u_target << '\n';
        }
        else if (key == "INT_MULTI")
        {
            conf >> int_multi;
            std::cout << "INT_MULTI\t\t\t\t" << int_multi << '\n';
        }
        else if (key == "RATE_BOUND")
        {
            uint32_t v;
            conf >> v;
            rate_bound = v;
            std::cout << "RATE_BOUND\t\t" << rate_bound << '\n';
        }
        else if (key == "ACK_HIGH_PRIO")
        {
            conf >> ack_high_prio;
            std::cout << "ACK_HIGH_PRIO\t\t" << ack_high_prio << '\n';
        }
        else if (key == "DCTCP_RATE_AI")
        {
            conf >> dctcp_rate_ai;
            std::cout << "DCTCP_RATE_AI\t\t\t\t" << dctcp_rate_ai << "\n";
        }
        else if (key == "PFC_OUTPUT_FILE")
        {
            conf >> pfc_output_file;
            std::cout << "PFC_OUTPUT_FILE\t\t\t\t" << pfc_output_file << '\n';
        }
        else if (key == "LINK_DOWN")
        {
            conf >> link_down_time >> link_down_A >> link_down_B;
            std::cout << "LINK_DOWN\t\t\t\t" << link_down_time << ' ' << link_down_A << ' '
                      << link_down_B << '\n';
        }
        else if (key == "ENABLE_TRACE")
        {
            conf >> enable_trace;
            std::cout << "ENABLE_TRACE\t\t\t\t" << enable_trace << '\n';
        }
        else if (key == "KMAX_MAP")
        {
            int n_k;
            conf >> n_k;
            std::cout << "KMAX_MAP\t\t\t\t";
            for (int i = 0; i < n_k; i++)
            {
                uint64_t rate;
                uint32_t k;
                conf >> rate >> k;
                rate2kmax[rate] = k;
                std::cout << ' ' << rate << ' ' << k;
            }
            std::cout << '\n';
        }
        else if (key == "KMIN_MAP")
        {
            int n_k;
            conf >> n_k;
            std::cout << "KMIN_MAP\t\t\t\t";
            for (int i = 0; i < n_k; i++)
            {
                uint64_t rate;
                uint32_t k;
                conf >> rate >> k;
                rate2kmin[rate] = k;
                std::cout << ' ' << rate << ' ' << k;
            }
            std::cout << '\n';
        }
        else if (key == "PMAX_MAP")
        {
            int n_k;
            conf >> n_k;
            std::cout << "PMAX_MAP\t\t\t\t";
            for (int i = 0; i < n_k; i++)
            {
                uint64_t rate;
                double p;
                conf >> rate >> p;
                rate2pmax[rate] = p;
                std::cout << ' ' << rate << ' ' << p;
            }
            std::cout << '\n';
        }
        else if (key == "BUFFER_SIZE")
        {
            conf >> buffer_size;
            std::cout << "BUFFER_SIZE\t\t\t\t" << buffer_size << '\n';
        }
        else if (key == "QLEN_MON_FILE")
        {
            conf >> qlen_mon_file;
            std::cout << "QLEN_MON_FILE\t\t\t\t" << qlen_mon_file << '\n';
        }
        else if (key == "QLEN_MON_START")
        {
            conf >> qlen_mon_start;
            std::cout << "QLEN_MON_START\t\t\t\t" << qlen_mon_start << '\n';
        }
        else if (key == "QLEN_MON_END")
        {
            conf >> qlen_mon_end;
            std::cout << "QLEN_MON_END\t\t\t\t" << qlen_mon_end << '\n';
        }
        else if (key == "MULTI_RATE")
        {
            int v;
            conf >> v;
            multi_rate = v;
            std::cout << "MULTI_RATE\t\t\t\t" << multi_rate << '\n';
        }
        else if (key == "SAMPLE_FEEDBACK")
        {
            int v;
            conf >> v;
            sample_feedback = v;
            std::cout << "SAMPLE_FEEDBACK\t\t\t\t" << sample_feedback << '\n';
        }
        else if (key == "PINT_LOG_BASE")
        {
            conf >> pint_log_base;
            std::cout << "PINT_LOG_BASE\t\t\t\t" << pint_log_base << '\n';
        }
        else if (key == "PINT_PROB")
        {
            conf >> pint_prob;
            std::cout << "PINT_PROB\t\t\t\t" << pint_prob << '\n';
        }
        fflush(stdout);
    }
    conf.close();

    // overriding config file. I prefer to use cmd arguments
    cc_mode = algorithm;   // overrides configuration file
    has_win = windowCheck; // overrides configuration file
    FAN = incast;

    Config::SetDefault("ns3::QbbNetDevice::PauseTime", UintegerValue(pause_time));
    Config::SetDefault("ns3::QbbNetDevice::QcnEnabled", BooleanValue(enable_qcn));

    // set int_multi
    IntHop::multi = int_multi;
    // IntHeader::mode
    if (cc_mode == CC_MODE::TIMELY || cc_mode == CC_MODE::PATCHED_TIMELY)
    { // timely or patched timely, use ts
        IntHeader::mode = IntHeader::TS;
    }
    else if (cc_mode == CC_MODE::POWERTCP)
    { // hpcc, powertcp, use int
        IntHeader::mode = IntHeader::NORMAL;
    }
    else if (cc_mode == CC_MODE::HPCC_PINT)
    { // hpcc-pint
        IntHeader::mode = IntHeader::PINT;
    }
    else
    { // others, no extra header
        IntHeader::mode = IntHeader::NONE;
    }

    // Set Pint
    if (cc_mode == CC_MODE::HPCC_PINT)
    {
        Pint::set_log_base(pint_log_base);
        IntHeader::pint_bytes = Pint::get_n_bytes();
        printf("PINT bits: %d bytes: %d\n", Pint::get_n_bits(), Pint::get_n_bytes());
    }

    topof.open(topology_file.c_str());
    flowf.open(flow_file.c_str());
    uint32_t node_num;
    uint32_t switch_num;
    uint32_t tors;
    uint32_t link_num;
    uint32_t trace_num;
    topof >> node_num >> switch_num >> tors >>
        link_num; // changed here. The previous order was node, switch, link // tors is not used.
                  // switch_num=tors for now.
    std::cout << node_num << " " << switch_num << " " << tors << " " << link_num << std::endl;
    flowf >> flow_num;

    NodeContainer serverNodes;
    NodeContainer torNodes;
    NodeContainer spineNodes;
    NodeContainer switchNodes;
    NodeContainer allNodes;

    std::vector<uint32_t> node_type(node_num, 0);

    std::cout << "switch_num " << switch_num << std::endl;
    for (uint32_t i = 0; i < switch_num; i++)
    {
        uint32_t sid;
        topof >> sid;
        std::cout << "sid " << sid << std::endl;
        switchNumToId[i] = sid;
        switchIdToNum[sid] = i;
        if (i < LEAF_COUNT)
        {
            node_type[sid] = 1;
        }
        else
        {
            node_type[sid] = 2;
        }
    }

    for (uint32_t i = 0; i < node_num; i++)
    {
        if (node_type[i] == 0)
        {
            Ptr<Node> node = CreateObject<Node>();
            n.Add(node);
            allNodes.Add(node);
            serverNodes.Add(node);
        }
        else
        {
            Ptr<SwitchNode> sw = CreateObject<SwitchNode>();
            n.Add(sw);
            switchNodes.Add(sw);
            allNodes.Add(sw);
            sw->SetAttribute("EcnEnabled", BooleanValue(enable_qcn));
            sw->SetNodeType(1);
            if (node_type[i] == 1)
            {
                torNodes.Add(sw);
            }
        }
    }

    NS_LOG_INFO("Create nodes.");

    InternetStackHelper internet;
    internet.Install(n);

    //
    // Assign IP to each server
    //
    for (uint32_t i = 0; i < node_num; i++)
    {
        if (n.Get(i)->GetNodeType() == 0)
        { // is server
            serverAddress.resize(i + 1);
            serverAddress[i] = node_id_to_ip(i);
        }
    }

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the channels required by the topology.
    //

    Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    rem->SetRandomVariable(uv);
    uv->SetStream(50);
    rem->SetAttribute("ErrorRate", DoubleValue(error_rate_per_link));
    rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));

    FILE* pfc_file = fopen(pfc_output_file.c_str(), "w");

    QbbHelper qbb;
    Ipv4AddressHelper ipv4;
    for (uint32_t i = 0; i < link_num; i++)
    {
        uint32_t src;
        uint32_t dst;
        std::string data_rate;
        std::string link_delay;
        double error_rate;
        topof >> src >> dst >> data_rate >> link_delay >> error_rate;

        std::cout << src << " " << dst << " " << n.GetN() << std::endl;
        Ptr<Node> snode = n.Get(src);
        Ptr<Node> dnode = n.Get(dst);

        qbb.SetDeviceAttribute("DataRate", StringValue(data_rate));
        qbb.SetChannelAttribute("Delay", StringValue(link_delay));

        if (error_rate > 0)
        {
            Ptr<RateErrorModel> rem = CreateObject<RateErrorModel>();
            Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
            rem->SetRandomVariable(uv);
            uv->SetStream(50);
            rem->SetAttribute("ErrorRate", DoubleValue(error_rate));
            rem->SetAttribute("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
            qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
        }
        else
        {
            qbb.SetDeviceAttribute("ReceiveErrorModel", PointerValue(rem));
        }

        fflush(stdout);

        // Assigne server IP
        // Note: this should be before the automatic assignment below (ipv4.Assign(d)),
        // because we want our IP to be the primary IP (first in the IP address list),
        // so that the global routing is based on our IP
        NetDeviceContainer d = qbb.Install(snode, dnode);
        if (snode->GetNodeType() == 0)
        {
            Ptr<Ipv4> ipv4 = snode->GetObject<Ipv4>();
            ipv4->AddInterface(d.Get(0));
            ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[src], Ipv4Mask(0xff000000)));
        }
        if (dnode->GetNodeType() == 0)
        {
            Ptr<Ipv4> ipv4 = dnode->GetObject<Ipv4>();
            ipv4->AddInterface(d.Get(1));
            ipv4->AddAddress(1, Ipv4InterfaceAddress(serverAddress[dst], Ipv4Mask(0xff000000)));
        }

        if (!snode->GetNodeType())
        {
            sourceNodes[src].Add(DynamicCast<QbbNetDevice>(d.Get(0)));
        }

        if (!snode->GetNodeType() && dnode->GetNodeType())
        {
            switchDown[switchIdToNum[dst]].Add(DynamicCast<QbbNetDevice>(d.Get(1)));
        }

        if (snode->GetNodeType() && dnode->GetNodeType())
        {
            switchToSwitchInterfaces.Add(d);
            switchUp[switchIdToNum[src]].Add(DynamicCast<QbbNetDevice>(d.Get(0)));
            switchUp[switchIdToNum[dst]].Add(DynamicCast<QbbNetDevice>(d.Get(1)));
            switchToSwitch[src][dst].push_back(DynamicCast<QbbNetDevice>(d.Get(0)));
            switchToSwitch[src][dst].push_back(DynamicCast<QbbNetDevice>(d.Get(1)));
        }

        // used to create a graph of the topology
        nbr2if[snode][dnode].idx = DynamicCast<QbbNetDevice>(d.Get(0))->GetIfIndex();
        nbr2if[snode][dnode].up = true;
        nbr2if[snode][dnode].delay =
            DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(0))->GetChannel())
                ->GetDelay()
                .GetTimeStep();
        nbr2if[snode][dnode].bw = DynamicCast<QbbNetDevice>(d.Get(0))->GetDataRate().GetBitRate();
        nbr2if[dnode][snode].idx = DynamicCast<QbbNetDevice>(d.Get(1))->GetIfIndex();
        nbr2if[dnode][snode].up = true;
        nbr2if[dnode][snode].delay =
            DynamicCast<QbbChannel>(DynamicCast<QbbNetDevice>(d.Get(1))->GetChannel())
                ->GetDelay()
                .GetTimeStep();
        nbr2if[dnode][snode].bw = DynamicCast<QbbNetDevice>(d.Get(1))->GetDataRate().GetBitRate();

        // This is just to set up the connectivity between nodes. The IP addresses are useless
        // char ipstring[16];
        std::stringstream ipstring;
        ipstring << "10." << i / 254 + 1 << "." << i % 254 + 1 << ".0";
        // sprintf(ipstring, "10.%d.%d.0", i / 254 + 1, i % 254 + 1);
        ipv4.SetBase(ipstring.str().c_str(), "255.255.255.0");
        // ipv4.SetBase(ipstring, "255.255.255.0");
        ipv4.Assign(d);

        // setup PFC trace
        DynamicCast<QbbNetDevice>(d.Get(0))->TraceConnectWithoutContext(
            "QbbPfc",
            MakeBoundCallback(&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(0))));
        DynamicCast<QbbNetDevice>(d.Get(1))->TraceConnectWithoutContext(
            "QbbPfc",
            MakeBoundCallback(&get_pfc, pfc_file, DynamicCast<QbbNetDevice>(d.Get(1))));
    }

    nic_rate = get_nic_rate(n);

    // config switch
    // The switch mmu runs Dynamic Thresholds (DT) by default.
    for (uint32_t i = 0; i < node_num; i++)
    {
        if (n.Get(i)->GetNodeType())
        { // is switch
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
            uint32_t shift = 3; // by default 1/8
            double alpha = 1.0 / 8;
            sw->m_mmu->SetAlphaIngress(alpha);
            uint64_t totalHeadroom = 0;
            for (uint32_t j = 1; j < sw->GetNDevices(); j++)
            {
                for (uint32_t qu = 0; qu < 8; qu++)
                {
                    Ptr<QbbNetDevice> dev = DynamicCast<QbbNetDevice>(sw->GetDevice(j));
                    // set ecn
                    uint64_t rate = dev->GetDataRate().GetBitRate();
                    NS_ASSERT_MSG(rate2kmin.find(rate) != rate2kmin.end(),
                                  "must set kmin for each link speed");
                    NS_ASSERT_MSG(rate2kmax.find(rate) != rate2kmax.end(),
                                  "must set kmax for each link speed");
                    NS_ASSERT_MSG(rate2pmax.find(rate) != rate2pmax.end(),
                                  "must set pmax for each link speed");
                    sw->m_mmu->ConfigEcn(j, rate2kmin[rate], rate2kmax[rate], rate2pmax[rate]);
                    // set pfc
                    uint64_t delay =
                        DynamicCast<QbbChannel>(dev->GetChannel())->GetDelay().GetTimeStep();
                    uint32_t headroom = rate * delay / 8 / 1000000000 * 3;

                    sw->m_mmu->SetHeadroom(headroom, j, qu);
                    totalHeadroom += headroom;
                }
            }
            sw->m_mmu->SetBufferPool(buffer_size * 1024 * 1024);
            sw->m_mmu->SetIngressPool(buffer_size * 1024 * 1024 - totalHeadroom);
            sw->m_mmu->SetEgressLosslessPool(buffer_size * 1024 * 1024);
            sw->m_mmu->node_id = sw->GetId();
        }
    }

#if ENABLE_QP
    FILE* fct_output = fopen(fct_output_file.c_str(), "w");
    //
    // install RDMA driver
    //
    for (uint32_t i = 0; i < node_num; i++)
    {
        if (n.Get(i)->GetNodeType() == 0)
        { // is server
            // create RdmaHw
            Ptr<RdmaHw> rdmaHw = CreateObject<RdmaHw>();
            rdmaHw->SetAttribute("ClampTargetRate", BooleanValue(clamp_target_rate));
            rdmaHw->SetAttribute("AlphaResumInterval", DoubleValue(alpha_resume_interval));
            rdmaHw->SetAttribute("RPTimer", DoubleValue(rp_timer));
            rdmaHw->SetAttribute("FastRecoveryTimes", UintegerValue(fast_recovery_times));
            rdmaHw->SetAttribute("EwmaGain", DoubleValue(ewma_gain));
            rdmaHw->SetAttribute("RateAI", DataRateValue(DataRate(rate_ai)));
            rdmaHw->SetAttribute("RateHAI", DataRateValue(DataRate(rate_hai)));
            rdmaHw->SetAttribute("L2BackToZero", BooleanValue(l2_back_to_zero));
            rdmaHw->SetAttribute("L2ChunkSize", UintegerValue(l2_chunk_size));
            rdmaHw->SetAttribute("L2AckInterval", UintegerValue(l2_ack_interval));
            rdmaHw->SetAttribute("CcMode", UintegerValue(cc_mode));
            rdmaHw->SetAttribute("RateDecreaseInterval", DoubleValue(rate_decrease_interval));
            rdmaHw->SetAttribute("MinRate", DataRateValue(DataRate(min_rate)));
            rdmaHw->SetAttribute("Mtu", UintegerValue(packet_payload_size));
            rdmaHw->SetAttribute("MiThresh", UintegerValue(mi_thresh));
            rdmaHw->SetAttribute("VarWin", BooleanValue(var_win));
            rdmaHw->SetAttribute("FastReact", BooleanValue(fast_react));
            rdmaHw->SetAttribute("MultiRate", BooleanValue(multi_rate));
            rdmaHw->SetAttribute("SampleFeedback", BooleanValue(sample_feedback));
            rdmaHw->SetAttribute("TargetUtil", DoubleValue(u_target));
            rdmaHw->SetAttribute("RateBound", BooleanValue(rate_bound));
            rdmaHw->SetAttribute("DctcpRateAI", DataRateValue(DataRate(dctcp_rate_ai)));
            rdmaHw->SetAttribute("PowerTCPEnabled", BooleanValue(wien));
            rdmaHw->SetAttribute("PowerTCPdelay", BooleanValue(delayWien));
            rdmaHw->SetPintSmplThresh(pint_prob);
            // create and install RdmaDriver
            Ptr<RdmaDriver> rdma = CreateObject<RdmaDriver>();
            Ptr<Node> node = n.Get(i);
            rdma->SetNode(node);
            rdma->SetRdmaHw(rdmaHw);

            node->AggregateObject(rdma);
            rdma->Init();
            rdma->TraceConnectWithoutContext("QpComplete",
                                             MakeBoundCallback(qp_finish, fct_output));
        }
    }

#endif

    // set ACK priority on hosts
    if (ack_high_prio)
    {
        RdmaEgressQueue::ack_q_idx = 0;
    }
    else
    {
        RdmaEgressQueue::ack_q_idx = 3;
    }

    // setup routing
    CalculateRoutes(n);
    SetRoutingEntries();
    //
    // get BDP and delay
    //
    maxRtt = maxBdp = 0;
    uint64_t minRtt = 1e9;
    for (uint32_t i = 0; i < node_num; i++)
    {
        if (n.Get(i)->GetNodeType() != 0)
        {
            continue;
        }
        for (uint32_t j = 0; j < node_num; j++)
        {
            if (n.Get(j)->GetNodeType() != 0)
            {
                continue;
            }
            if (i == j)
            {
                continue;
            }
            uint64_t delay = pairDelay[n.Get(i)][n.Get(j)];
            uint64_t txDelay = pairTxDelay[n.Get(i)][n.Get(j)];
            uint64_t rtt = delay * 2 + txDelay;
            uint64_t bw = pairBw[i][j];
            uint64_t bdp = rtt * bw / 1000000000 / 8;
            pairBdp[n.Get(i)][n.Get(j)] = bdp;
            pairRtt[i][j] = rtt;
            if (bdp > maxBdp)
            {
                maxBdp = bdp;
            }
            if (rtt > maxRtt)
            {
                maxRtt = rtt;
            }
            if (rtt < minRtt)
            {
                minRtt = rtt;
            }
        }
    }
    printf("maxRtt=%lu maxBdp=%lu minRtt=%lu\n", maxRtt, maxBdp, minRtt);

    //
    // setup switch CC
    //
    for (uint32_t i = 0; i < node_num; i++)
    {
        if (n.Get(i)->GetNodeType())
        { // switch
            Ptr<SwitchNode> sw = DynamicCast<SwitchNode>(n.Get(i));
            sw->SetAttribute("CcMode", UintegerValue(cc_mode));
            sw->SetAttribute("MaxRtt", UintegerValue(maxRtt));
            sw->SetAttribute("PowerEnabled", BooleanValue(wien));
        }
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Create Applications.");

    Time interPacketInterval = Seconds(0.0000005 / 2);

    // maintain port number for each host
    for (uint32_t i = 0; i < node_num; i++)
    {
        if (n.Get(i)->GetNodeType() == 0)
        {
            for (uint32_t j = 0; j < node_num; j++)
            {
                if (n.Get(j)->GetNodeType() == 0)
                {
                    portNumder[i][j] =
                        rand_range(10000, 11000); // each host pair use port number from 10000
                }
            }
        }
    }
    DestportNumder = portNumder;

    double oversubRatio =
        double(SERVER_COUNT * LEAF_SERVER_CAPACITY) / (SPINE_LEAF_CAPACITY * SPINE_COUNT);
    std::cout << "Over-subscription ratio: " << oversubRatio << " numerator "
              << double(SERVER_COUNT * LEAF_SERVER_CAPACITY) << " denom "
              << (SPINE_LEAF_CAPACITY * SPINE_COUNT) << std::endl;
    NS_LOG_INFO("Initialize CDF table");
    struct cdf_table* cdfTable = new cdf_table();
    init_cdf(cdfTable);
    load_cdf(cdfTable, cdfFileName.c_str());
    NS_LOG_INFO("Calculating request rate");
    //        double requestRate =0;
    double requestRate = load * LEAF_SERVER_CAPACITY * SERVER_COUNT / oversubRatio /
                         (8 * avg_cdf(cdfTable)) / SERVER_COUNT;
    //       double requestRate = load * LEAF_SERVER_CAPACITY * SERVER_COUNT  / (8 * avg_cdf
    //       (cdfTable)) / SERVER_COUNT;
    NS_LOG_INFO("Average request rate: " << requestRate << " per second");
    std::cout << "Average request rate: " << requestRate << " per second" << std::endl;
    NS_LOG_INFO("Initialize random seed: " << randomSeed);
    if (randomSeed == 0)
    {
        srand((unsigned)time(NULL));
    }
    else
    {
        srand(randomSeed);
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /* Applications Background*/
    NS_LOG_INFO("Create background applications");

    long flowCount = 1;
    long totalFlowSize = 0;

    for (int fromLeafId = 0; fromLeafId < LEAF_COUNT; fromLeafId++)
    {
        install_applications(fromLeafId,
                             requestRate,
                             cdfTable,
                             flowCount,
                             totalFlowSize,
                             SERVER_COUNT,
                             LEAF_COUNT,
                             START_TIME,
                             END_TIME,
                             FLOW_LAUNCH_END_TIME);
    }

    NS_LOG_INFO("Total flow: " << flowCount);
    std::cout << "Total flow: " << flowCount << std::endl;
    NS_LOG_INFO("Actual average flow size: " << static_cast<double>(totalFlowSize) / flowCount);
    std::cout << "Actual average flow size: " << static_cast<double>(totalFlowSize) / flowCount
              << std::endl;
    NS_LOG_INFO("Create applications");

    /* Applications Foreground*/
    NS_LOG_INFO("Create foreground applications");
    long flowCountQ = flowCount;
    long totalFlowSizeQ = 0;
    double QUERY_START_TIME = 0.0;
    requestRate = queryRequestRate; // 1;//10;
    if (requestSize == 0)
    {
        requestRate = 0;
    }
    if (requestRate > 0 && requestSize > 0)
    {
        for (int fromLeafId = 0; fromLeafId < LEAF_COUNT; fromLeafId++)
        {
            install_applications_queryNew(fromLeafId,
                                          requestRate,
                                          requestSize,
                                          cdfTable,
                                          flowCountQ,
                                          totalFlowSizeQ,
                                          SERVER_COUNT,
                                          LEAF_COUNT,
                                          QUERY_START_TIME,
                                          END_TIME,
                                          FLOW_LAUNCH_END_TIME);
        }
    }

    NS_LOG_INFO("Total Query: " << flowCountQ - flowCount);
    std::cout << "Total Query: " << flowCountQ - flowCount << std::endl;

    NS_LOG_INFO("Actual average QuerySize: " << static_cast<double>(totalFlowSizeQ) /
                                                    (flowCountQ - flowCount));
    std::cout << "Actual average QuerySize: "
              << static_cast<double>(totalFlowSizeQ) / (flowCountQ - flowCount) << std::endl;

    topof.close();
    tracef.close();
    double delay = 1.5 * minRtt * 1e-9; // 10 micro seconds
    Simulator::Schedule(Seconds(delay), printBuffer, torNodes, delay);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    std::cout << "Running Simulation.\n";
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(END_TIME));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    endt = clock();
    std::cout << (double)(endt - begint) / CLOCKS_PER_SEC << "\n";
}
