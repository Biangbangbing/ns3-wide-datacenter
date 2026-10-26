#include "rdma-hw.h"

#include "pint.h"
#include "ppp-header.h"
#include "qbb-header.h"

#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/double.h"
#include "ns3/feedback-tag.h"
#include "ns3/pointer.h"
#include "ns3/ppp-header.h"
#include "ns3/uinteger.h"
#include "ns3/unsched-tag.h"
#include <ns3/ipv4-header.h>
#include <ns3/seq-ts-header.h>
#include <ns3/simulator.h>
#include <ns3/udp-header.h>

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <random>
#include <set>
#include <vector>
namespace ns3
{

TypeId
RdmaHw::GetTypeId(void)
{
    static TypeId tid =
        TypeId("ns3::RdmaHw")
            .SetParent<Object>()
            .AddAttribute("MinRate",
                          "Minimum rate of a throttled flow",
                          DataRateValue(DataRate("100Mb/s")),
                          MakeDataRateAccessor(&RdmaHw::m_minRate),
                          MakeDataRateChecker())
            .AddAttribute("Mtu",
                          "Mtu.",
                          UintegerValue(1000),
                          MakeUintegerAccessor(&RdmaHw::m_mtu),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("CcMode",
                          "which mode of DCQCN is running",
                          UintegerValue(0),
                          MakeUintegerAccessor(&RdmaHw::m_cc_mode),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("NACKGenerationInterval",
                          "The NACK Generation interval",
                          DoubleValue(500.0),
                          MakeDoubleAccessor(&RdmaHw::m_nack_interval),
                          MakeDoubleChecker<double>())
            .AddAttribute("L2ChunkSize",
                          "Layer 2 chunk size. Disable chunk mode if equals to 0.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&RdmaHw::m_chunk),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("L2AckInterval",
                          "Layer 2 Ack intervals. Disable ack if equals to 0.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&RdmaHw::m_ack_interval),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("L2BackToZero",
                          "Layer 2 go back to zero transmission.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RdmaHw::m_backto0),
                          MakeBooleanChecker())
            .AddAttribute("EwmaGain",
                          "Control gain parameter which determines the level of rate decrease",
                          DoubleValue(1.0 / 16),
                          MakeDoubleAccessor(&RdmaHw::m_g),
                          MakeDoubleChecker<double>())
            .AddAttribute("RateOnFirstCnp",
                          "the fraction of rate on first CNP",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&RdmaHw::m_rateOnFirstCNP),
                          MakeDoubleChecker<double>())
            .AddAttribute("ClampTargetRate",
                          "Clamp target rate.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RdmaHw::m_EcnClampTgtRate),
                          MakeBooleanChecker())
            .AddAttribute("RPTimer",
                          "The rate increase timer at RP in microseconds",
                          DoubleValue(1500.0),
                          MakeDoubleAccessor(&RdmaHw::m_rpgTimeReset),
                          MakeDoubleChecker<double>())
            .AddAttribute("RateDecreaseInterval",
                          "The interval of rate decrease check",
                          DoubleValue(4.0),
                          MakeDoubleAccessor(&RdmaHw::m_rateDecreaseInterval),
                          MakeDoubleChecker<double>())
            .AddAttribute("FastRecoveryTimes",
                          "The rate increase timer at RP",
                          UintegerValue(5),
                          MakeUintegerAccessor(&RdmaHw::m_rpgThreshold),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("AlphaResumInterval",
                          "The interval of resuming alpha",
                          DoubleValue(55.0),
                          MakeDoubleAccessor(&RdmaHw::m_alpha_resume_interval),
                          MakeDoubleChecker<double>())
            .AddAttribute("RateAI",
                          "Rate increment unit in AI period",
                          DataRateValue(DataRate("5Mb/s")),
                          MakeDataRateAccessor(&RdmaHw::m_rai),
                          MakeDataRateChecker())
            .AddAttribute("RateHAI",
                          "Rate increment unit in hyperactive AI period",
                          DataRateValue(DataRate("50Mb/s")),
                          MakeDataRateAccessor(&RdmaHw::m_rhai),
                          MakeDataRateChecker())
            .AddAttribute("VarWin",
                          "Use variable window size or not",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RdmaHw::m_var_win),
                          MakeBooleanChecker())
            .AddAttribute("FastReact",
                          "Fast React to congestion feedback",
                          BooleanValue(true),
                          MakeBooleanAccessor(&RdmaHw::m_fast_react),
                          MakeBooleanChecker())
            .AddAttribute("MiThresh",
                          "Threshold of number of consecutive AI before MI",
                          UintegerValue(5),
                          MakeUintegerAccessor(&RdmaHw::m_miThresh),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("TargetUtil",
                          "The Target Utilization of the bottleneck bandwidth, by default 95%",
                          DoubleValue(0.95),
                          MakeDoubleAccessor(&RdmaHw::m_targetUtil),
                          MakeDoubleChecker<double>())
            .AddAttribute(
                "UtilHigh",
                "The upper bound of Target Utilization of the bottleneck bandwidth, by default 98%",
                DoubleValue(0.98),
                MakeDoubleAccessor(&RdmaHw::m_utilHigh),
                MakeDoubleChecker<double>())
            .AddAttribute("RateBound",
                          "Bound packet sending by rate, for test only",
                          BooleanValue(true),
                          MakeBooleanAccessor(&RdmaHw::m_rateBound),
                          MakeBooleanChecker())
            .AddAttribute("MultiRate",
                          "Maintain multiple rates in HPCC",
                          BooleanValue(true),
                          MakeBooleanAccessor(&RdmaHw::m_multipleRate),
                          MakeBooleanChecker())
            .AddAttribute("SampleFeedback",
                          "Whether sample feedback or not",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RdmaHw::m_sampleFeedback),
                          MakeBooleanChecker())
            .AddAttribute("TimelyAlpha",
                          "Alpha of TIMELY",
                          DoubleValue(0.875),
                          MakeDoubleAccessor(&RdmaHw::m_tmly_alpha),
                          MakeDoubleChecker<double>())
            .AddAttribute("TimelyBeta",
                          "Beta of TIMELY",
                          DoubleValue(0.8),
                          MakeDoubleAccessor(&RdmaHw::m_tmly_beta),
                          MakeDoubleChecker<double>())
            .AddAttribute("TimelyTLow",
                          "TLow of TIMELY (ns)",
                          UintegerValue(50000),
                          MakeUintegerAccessor(&RdmaHw::m_tmly_TLow),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("TimelyTHigh",
                          "THigh of TIMELY (ns)",
                          UintegerValue(500000),
                          MakeUintegerAccessor(&RdmaHw::m_tmly_THigh),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("TimelyMinRtt",
                          "MinRtt of TIMELY (ns)",
                          UintegerValue(20000),
                          MakeUintegerAccessor(&RdmaHw::m_tmly_minRtt),
                          MakeUintegerChecker<uint64_t>())
            // 50us RTTref comes from paper SQCC: Stable Queue Congestion Control
            .AddAttribute("TimelyPatchedRttRef", //
                          "RTTref of Patched TIMELY (ns)",
                          UintegerValue(500000),
                          MakeUintegerAccessor(&RdmaHw::m_ptmly_RttRef),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("TimelyPatchedBeta",
                          "Beta of PatchedTIMELY",
                          DoubleValue(0.008),
                          MakeDoubleAccessor(&RdmaHw::m_ptmly_beta),
                          MakeDoubleChecker<double>())
            .AddAttribute("DctcpRateAI",
                          "DCTCP's Rate increment unit in AI period",
                          DataRateValue(DataRate("1000Mb/s")),
                          MakeDataRateAccessor(&RdmaHw::m_dctcp_rai),
                          MakeDataRateChecker())
            .AddAttribute("PintSmplThresh",
                          "PINT's sampling threshold in rand()%65536",
                          UintegerValue(65536),
                          MakeUintegerAccessor(&RdmaHw::pint_smpl_thresh),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("PowerTCPEnabled",
                          "to enable PowerTCP",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RdmaHw::PowerTCPEnabled),
                          MakeBooleanChecker())
            .AddAttribute("PowerTCPdelay",
                          "to enable PowerTCP in delaymode",
                          BooleanValue(false),
                          MakeBooleanAccessor(&RdmaHw::PowerTCPdelay),
                          MakeBooleanChecker())
            // Swift parameters from code of paper Burst-tolerant datacenter networks with Vertigo
            .AddAttribute("SwiftAi",
                          "Swift's additive increment",
                          UintegerValue(1000),
                          MakeUintegerAccessor(&RdmaHw::swift_ai),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("SwiftBeta",
                          "Swift's multiplicative decrease constant",
                          DoubleValue(0.8),
                          MakeDoubleAccessor(&RdmaHw::swift_beta),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftMaxMdf",
                          "Swift's maximum multiplicative decrease factor",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&RdmaHw::swift_max_mdf),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftBaseTarget",
                          "Swift's base fabric target RTT (ns)",
                          UintegerValue(60000),
                          MakeUintegerAccessor(&RdmaHw::swift_base_target),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("SwiftHopScale",
                          "Swift's per hop RTT scaling factor",
                          DoubleValue(30000),
                          MakeDoubleAccessor(&RdmaHw::swift_hop_scale),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftFsMaxCwnd",
                          "Swift's max cwnd limit that enables flow-based scaling",
                          DoubleValue(100.0),
                          MakeDoubleAccessor(&RdmaHw::swift_fs_max_cwnd),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftFsMinCwnd",
                          "Swift's min cwnd limit that enables flow-based scaling",
                          DoubleValue(0.1),
                          MakeDoubleAccessor(&RdmaHw::swift_fs_min_cwnd),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftFsRange",
                          "Swift's flow-based scheduling max scaling range",
                          DoubleValue(0.000300),
                          MakeDoubleAccessor(&RdmaHw::swift_fs_range),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftMinCwnd",
                          "Swift's min cwnd it can exceed (not fs)",
                          DoubleValue(0.001),
                          MakeDoubleAccessor(&RdmaHw::swift_min_cwnd),
                          MakeDoubleChecker<double>())
            .AddAttribute("SwiftMaxCwnd",
                          "Swift's max cwnd it can exceed (not fs)",
                          DoubleValue(800000),
                          MakeDoubleAccessor(&RdmaHw::swift_max_cwnd),
                          MakeDoubleChecker<double>())
            // seems that in emulator there is no endpoint delay
            .AddAttribute("SwiftTargetEndpointDelay",
                          "Swift's target endpoint delay (ns)",
                          UintegerValue(1000000),
                          MakeUintegerAccessor(&RdmaHw::swift_target_endpoint_delay),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("RttQcnTmin",
                          "RTT-QCN's max RTT value to generate no ECN",
                          UintegerValue(3000),
                          MakeUintegerAccessor(&RdmaHw::rtt_qcn_tmin),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("RttQcnTmax",
                          "RTT-QCN's min RTT value to generate full ECN",
                          UintegerValue(5000),
                          MakeUintegerAccessor(&RdmaHw::rtt_qcn_tmax),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("RttQcnAlpha",
                          "Additive increase when cwnd < mss",
                          DoubleValue(0.5),
                          MakeDoubleAccessor(&RdmaHw::rtt_qcn_alpha),
                          MakeDoubleChecker<double>())
            .AddAttribute("RttQcnBeta",
                          "Multiplicative decrease when cwnd < mss",
                          DoubleValue(0.25),
                          MakeDoubleAccessor(&RdmaHw::rtt_qcn_beta),
                          MakeDoubleChecker<double>())
            .AddAttribute("PowerQcnGradMin",
                          "Minimum gradient of PowerQCN",
                          DoubleValue(-0.2),
                          MakeDoubleAccessor(&RdmaHw::powerqcn_grad_min),
                          MakeDoubleChecker<double>())
            .AddAttribute("PowerQcnGradMax",
                          "Maximum gradient of PowerQCN",
                          DoubleValue(0.6),
                          MakeDoubleAccessor(&RdmaHw::powerqcn_grad_max),
                          MakeDoubleChecker<double>())
            .AddAttribute("SrHwBitmapSize",
                          "Size (in packets) of the hardware bitmap used by Selective Retransmission",
                          UintegerValue(64),
                          MakeUintegerAccessor(&RdmaHw::m_srK),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("SrEnabled",
                          "Enable the new selective-retransmission bitmap mechanism",
                          BooleanValue(true),
                          MakeBooleanAccessor(&RdmaHw::m_srEnabled),
                          MakeBooleanChecker())
            .AddAttribute("SrMode",
                          "0=Legacy, 1=RaLL (default), 2=SRNIC, 3=IRN",
                          UintegerValue(1),
                          MakeUintegerAccessor(&RdmaHw::m_srMode),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("SrnicAsyncNs",
                          "SRNIC software bitmap async delay (ns) per OOO event",
                          UintegerValue(1000),
                          MakeUintegerAccessor(&RdmaHw::m_srnicAsyncNs),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("RallAsyncNs",
                          "RaLL-scheme software bitmap async delay (ns) per request",
                          UintegerValue(500),
                          MakeUintegerAccessor(&RdmaHw::m_rallAsyncNs),
                          MakeUintegerChecker<uint64_t>());
    return tid;
}

RdmaHw::RdmaHw()
{
    m_srK = 64;
    m_srEnabled = true;
    m_srMode = 1; // RaLL
    m_srnicAsyncNs = 1000;
    m_rallAsyncNs = 500;
}

void
RdmaHw::SetNode(Ptr<Node> node)
{
    m_node = node;
}

void
RdmaHw::Setup(QpCompleteCallback cb)
{
    for (uint32_t i = 0; i < m_nic.size(); i++)
    {
        Ptr<QbbNetDevice> dev = m_nic[i].dev;
        if (dev == NULL)
        {
            continue;
        }
        // share data with NIC
        dev->m_rdmaEQ->m_qpGrp = m_nic[i].qpGrp;
        // setup callback
        dev->m_rdmaReceiveCb = MakeCallback(&RdmaHw::Receive, this);
        dev->m_rdmaLinkDownCb = MakeCallback(&RdmaHw::SetLinkDown, this);
        dev->m_rdmaPktSent = MakeCallback(&RdmaHw::PktSent, this);
        // config NIC
        dev->m_rdmaEQ->m_rdmaGetNxtPkt = MakeCallback(&RdmaHw::GetNxtPacket, this);
    }
    // setup qp complete callback
    m_qpCompleteCallback = cb;
}

uint32_t
RdmaHw::GetNicIdxOfQp(Ptr<RdmaQueuePair> qp)
{
    auto& v = m_rtTable[qp->dip.Get()];
    if (!v.empty())
    {
        return v[qp->GetHash() % v.size()];
    }
    else
    {
        NS_ASSERT_MSG(false, "We assume at least one NIC is alive");
    }
}

uint64_t
RdmaHw::GetQpKey(uint32_t dip, uint16_t sport, uint16_t pg)
{
    return ((uint64_t)dip << 32) | ((uint64_t)sport << 16) | (uint64_t)pg;
}

Ptr<RdmaQueuePair>
RdmaHw::GetQp(uint32_t dip, uint16_t sport, uint16_t pg)
{
    uint64_t key = GetQpKey(dip, sport, pg);
    auto it = m_qpMap.find(key);
    if (it != m_qpMap.end())
    {
        return it->second;
    }
    return NULL;
}

void
RdmaHw::AddQueuePair(uint64_t size,
                     uint16_t pg,
                     Ipv4Address sip,
                     Ipv4Address dip,
                     uint16_t sport,
                     uint16_t dport,
                     uint32_t win,
                     uint64_t baseRtt,
                     Callback<void> notifyAppFinish,
                     Time stopTime)
{
    // create qp
    Ptr<RdmaQueuePair> qp = CreateObject<RdmaQueuePair>(pg, sip, dip, sport, dport);
    qp->SetSize(size);
    qp->SetWin(win);
    qp->SetBaseRtt(baseRtt);
    qp->SetVarWin(m_var_win);
    qp->SetAppNotifyCallback(notifyAppFinish);
    qp->stopTime = stopTime;

    if (stopTime == Simulator::GetMaximumSimulationTime() - MicroSeconds(1))
    {
        qp->incastFlow = 1;
    }
    else
    {
        qp->incastFlow = 0;
    }

    // add qp
    uint32_t nic_idx = GetNicIdxOfQp(qp);
    m_nic[nic_idx].qpGrp->AddQp(qp);
    uint64_t key = GetQpKey(dip.Get(), sport, pg);
    m_qpMap[key] = qp;

    qp->powerEnabled = PowerTCPEnabled;

    // set init variables
    if (m_nic[nic_idx].dev == NULL)
    {
        std::cout << "sip " << sip << " dip " << dip << " sport " << sport << " dport " << dport
                  << std::endl;
    }
    DataRate m_bps = m_nic[nic_idx].dev->GetDataRate();
    if (win)
    {
        qp->SetWin(m_bps.GetBitRate() * 1 * baseRtt * 1e-9 / 8);
    }
    qp->m_rate = m_bps; // transmission starts at full rate
    qp->m_max_rate = m_bps;
    switch (m_cc_mode)
    {
    case CC_MODE::MLX_CNP:
        qp->mlx.m_targetRate = m_bps;
        break;
    case CC_MODE::HPCC:
        qp->hp.m_curRate = m_bps;
        if (m_multipleRate)
        {
            for (uint32_t i = 0; i < IntHeader::maxHop; i++)
            {
                qp->hp.hopState[i].Rc = m_bps;
            }
        }
        break;
    case CC_MODE::PATCHED_TIMELY:
    case CC_MODE::TIMELY:
        qp->tmly.m_curRate = m_bps;
        break;
    case CC_MODE::HPCC_PINT:
        qp->hpccPint.m_curRate = m_bps;
        break;
    case CC_MODE::SWIFT:
        qp->swift.m_curRate = m_bps;
    }

    qp->test.last_updateInterval = baseRtt;
    // Notify Nic
    m_nic[nic_idx].dev->NewQp(qp);
}

void
RdmaHw::DeleteQueuePair(Ptr<RdmaQueuePair> qp)
{
    // remove qp from the m_qpMap
    uint64_t key = GetQpKey(qp->dip.Get(), qp->sport, qp->m_pg);
    m_qpMap.erase(key);
}

Ptr<RdmaRxQueuePair>
RdmaHw::GetRxQp(uint32_t sip,
                uint32_t dip,
                uint16_t sport,
                uint16_t dport,
                uint16_t pg,
                bool create)
{
    uint64_t key = ((uint64_t)dip << 32) | ((uint64_t)pg << 16) | (uint64_t)dport;
    auto it = m_rxQpMap.find(key);
    if (it != m_rxQpMap.end())
    {
        return it->second;
    }
    if (create)
    {
        // create new rx qp
        Ptr<RdmaRxQueuePair> q = CreateObject<RdmaRxQueuePair>();
        // init the qp
        q->sip = sip;
        q->dip = dip;
        q->sport = sport;
        q->dport = dport;
        q->m_ecn_source.qIndex = pg;
        // store in map
        m_rxQpMap[key] = q;
        return q;
    }
    return NULL;
}

uint32_t
RdmaHw::GetNicIdxOfRxQp(Ptr<RdmaRxQueuePair> q)
{
    auto& v = m_rtTable[q->dip];
    if (!v.empty())
    {
        return v[q->GetHash() % v.size()];
    }
    else
    {
        NS_ASSERT_MSG(false, "We assume at least one NIC is alive");
    }
}

void
RdmaHw::DeleteRxQp(uint32_t dip, uint16_t pg, uint16_t dport)
{
    uint64_t key = ((uint64_t)dip << 32) | ((uint64_t)pg << 16) | (uint64_t)dport;
    m_rxQpMap.erase(key);
}

int
RdmaHw::ReceiveUdp(Ptr<Packet> p, CustomHeader& ch)
{
    uint8_t ecnbits = ch.GetIpv4EcnBits();

    uint32_t payload_size = p->GetSize() - ch.GetSerializedSize();

    // TODO find corresponding rx queue pair
    Ptr<RdmaRxQueuePair> rxQp =
        GetRxQp(ch.dip, ch.sip, ch.udp.dport, ch.udp.sport, ch.udp.pg, true);
    if (ecnbits != 0)
    {
        rxQp->m_ecn_source.ecnbits |= ecnbits;
        rxQp->m_ecn_source.qfb++;
    }
    rxQp->m_ecn_source.total++;
    rxQp->m_milestone_rx = m_ack_interval;

    int x = ReceiverCheckSeq(ch.udp.seq, rxQp, payload_size);
    if (x == 1 || x == 2)
    { // generate ACK or NACK
        qbbHeader seqh;
        //seqh.SetSeq(rxQp->ReceiverNextExpectedSeq);
        seqh.SetSeq(ch.udp.seq);
        // Carry the receiver's *current* ePSN so the sender can fast-forward
        // its bitmap/snd_una when the receiver has walked past holes - the
        // bare seq field above only tells the sender about this single
        // packet, which can lag the true ePSN after a hole-fill.
        seqh.SetEpsn(rxQp->ReceiverNextExpectedSeq);
        seqh.SetPG(ch.udp.pg);
        seqh.SetSport(ch.udp.dport);
        seqh.SetDport(ch.udp.sport);
        seqh.SetIntHeader(ch.udp.ih); // ACK will preserve the INT header from the UDP
        if (ecnbits) {
            seqh.SetCnp();
        }

        Ptr<Packet> newp = Create<Packet>(std::max(60 - 14 - 20 - (int)seqh.GetSerializedSize(), 0));
        newp->AddHeader(seqh);

        Ipv4Header head; // Prepare IPv4 header
        head.SetDestination(Ipv4Address(ch.sip));
        head.SetSource(Ipv4Address(ch.dip));
        head.SetProtocol(x == 1 ? 0xFC : 0xFD); // ack=0xFC nack=0xFD
        head.SetTtl(64);
        head.SetPayloadSize(newp->GetSize());
        head.SetIdentification(rxQp->m_ipid++);

        newp->AddHeader(head);
        AddHeader(newp, 0x800); // Attach PPP header
        // send
        uint32_t nic_idx = GetNicIdxOfRxQp(rxQp);
        m_nic[nic_idx].dev->RdmaEnqueueHighPrioQ(newp);
        m_nic[nic_idx].dev->TriggerTransmit();
    }
    return 0;
}

int
RdmaHw::ReceiveCnp(Ptr<Packet> p, CustomHeader& ch)
{
    // QCN on NIC
    // This is a Congestion signal
    // Then, extract data from the congestion packet.
    // We assume, without verify, the packet is destinated to me
    uint32_t qIndex = ch.cnp.qIndex;
    if (qIndex == 1)
    { // DCTCP
        std::cout << "TCP--ignore\n";
        return 0;
    }
    uint16_t udpport = ch.cnp.fid; // corresponds to the sport
    uint8_t ecnbits = ch.cnp.ecnBits;
    uint16_t qfb = ch.cnp.qfb;
    uint16_t total = ch.cnp.total;

    uint32_t i;
    // get qp
    Ptr<RdmaQueuePair> qp = GetQp(ch.sip, udpport, qIndex);
    if (qp == NULL)
    {
        std::cout << "ERROR: QCN NIC cannot find the flow\n";
    }
    // get nic
    uint32_t nic_idx = GetNicIdxOfQp(qp);
    Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;

    if (qp->m_rate == 0) // lazy initialization
    {
        qp->m_rate = dev->GetDataRate();
        if (m_cc_mode == CC_MODE::MLX_CNP)
        {
            qp->mlx.m_targetRate = dev->GetDataRate();
        }
        else if (m_cc_mode == CC_MODE::HPCC)
        {
            qp->hp.m_curRate = dev->GetDataRate();
            if (m_multipleRate)
            {
                for (uint32_t i = 0; i < IntHeader::maxHop; i++)
                {
                    qp->hp.hopState[i].Rc = dev->GetDataRate();
                }
            }
        }
        else if (m_cc_mode == CC_MODE::TIMELY || m_cc_mode == CC_MODE::PATCHED_TIMELY)
        {
            qp->tmly.m_curRate = dev->GetDataRate();
        }
        else if (m_cc_mode == CC_MODE::HPCC_PINT)
        {
            qp->hpccPint.m_curRate = dev->GetDataRate();
        }
        else if (m_cc_mode == CC_MODE::SWIFT)
        {
            qp->swift.m_curRate = dev->GetDataRate();
        }
    }
    return 0;
}

int
RdmaHw::ReceiveAck(Ptr<Packet> p, CustomHeader& ch)
{
    uint16_t qIndex = ch.ack.pg;
    uint16_t port = ch.ack.dport;
    uint32_t seq = ch.ack.seq;
    uint8_t cnp = (ch.ack.flags >> qbbHeader::FLAG_CNP) & 1;
    int i;
    Ptr<RdmaQueuePair> qp = GetQp(ch.sip, port, qIndex);
    if (qp == NULL)
    {
        std::cout << "ERROR: "
                  << "node:" << m_node->GetId() << ' ' << (ch.l3Prot == 0xFC ? "ACK" : "NACK")
                  << " NIC cannot find the flow\n";
        return 0;
    }

    uint32_t nic_idx = GetNicIdxOfQp(qp);
    Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;
    if (m_ack_interval == 0)
    {
        std::cout << "ERROR: shouldn't receive ack\n";
    }
    else
    {
        // if (!m_backto0)
        // {
        //     qp->Acknowledge(seq);
        //     qp->Acknowledge(qp->snd_una + m_mtu);
        // }
        // else
        // {
        //     uint32_t goback_seq = seq / m_chunk * m_chunk;
        //     qp->Acknowledge(goback_seq);
        // }
        if (qp->IsFinished())
        {
            QpComplete(qp);
            return 0;
        }
    }
    if (seq >= qp->snd_una) {
       HandleAckForSR(qp,seq,ch);
    }
    // After HandleAckForSR has processed the ack (and called Acknowledge),
    // snd_una may now equal m_size. The pre-HandleAckForSR IsFinished check
    // above sees the stale snd_una; check again here so the flow completes
    // on the actual final ACK rather than relying on legacy go-back-N
    // duplicates to re-trigger the early check on a later ACK.
    if (qp->IsFinished())
    {
        QpComplete(qp);
        return 0;
    }
    /*
    if (ch.l3Prot == 0xFD) //如果收到sack包
    { // NACK
        //printf("n ack rate: %lu\n",qp->m_rate.GetBitRate());
        //RecoverQueue(qp);
            printf("sack seq: %u expected ack-seq: %lu \n",seq, qp->snd_una);
            if (qp->test.recoveryState == 0) { //第一次乱序
                qp->test.bitMap.resize(((seq - qp->snd_una) / m_mtu) + 1,0);
                qp->test.bitMap[(seq - qp->snd_una) / m_mtu] = 1;
                qp->test.targetRecoverySeq = qp->snd_nxt;
                qp->snd_nxt = qp->snd_una;
                qp->test.recoveryTag = qp->snd_una;
                qp->test.rightSeq =seq;
                qp->test.recoveryState = 1;
            } else { //不是第一次乱序
                printf("sack seq: %u right: %u \n",seq, qp->test.rightSeq);
                if (qp->test.rightSeq < seq) {
                    qp->test.bitMap.resize(((seq - qp->snd_una) / m_mtu) + 1,0);
                    qp->test.rightSeq = seq;
                }
                qp->test.bitMap[(seq - qp->snd_una) / m_mtu] = 1;
                // 发送所有空隙包
            }
            std::cout << " ----Bitmap: ";
            for (int bit : qp->test.bitMap) {
                std::cout << bit << " ";
            }
            std::cout << std::endl;
    } else if (ch.l3Prot == 0xFC) { //如果收到ack包：可以认为收到ack之前的包都收到了
        printf("ack seq: %u expected ack-seq: %lu \n",seq, qp->snd_una);
        if (qp->test.recoveryState == 1) { //还处于乱序状态
            std::vector<int>::iterator k = qp->test.bitMap.begin() + (seq - qp->snd_una) / m_mtu + 1;
            int index = 0;
            for (int i = (seq - qp->snd_una) / m_mtu + 1; i <= (qp->test.rightSeq - qp->snd_una) / m_mtu; i++) {
                index = i;
                if (qp->test.bitMap[i] == 0) {
                    //index = i;
                    break;
                } else if (i == (qp->test.rightSeq - qp->snd_una) / m_mtu) {
                    index = i + 1;
                }
                k++;
            }
            std::cout << " ====Bitmap: ";
            for (int bit : qp->test.bitMap) {
                std::cout << bit << " ";
            }
            std::cout << std::endl;
            qp->test.bitMap.erase(qp->test.bitMap.begin(),k);
            qp->Acknowledge(qp->snd_una + index * m_mtu);
            qp->test.last_updateTime = Simulator::Now();
            qp->test.recoveryTag = qp->snd_una;
            if (qp->snd_una >= qp->test.targetRecoverySeq) { //所有乱序包都收到了
                qp->test.recoveryState = 0;
            }
            qp->snd_nxt = qp->snd_una; //重发哪一个数据包待商榷，如何连续控制snd_nxt增长
        } else {
            qp->Acknowledge(seq + m_mtu);
            qp->test.last_updateTime = Simulator::Now();
        }
    }
    */
    // handle cnp
    if (cnp)
    {
        if (m_cc_mode == CC_MODE::MLX_CNP)
        { // mlx version
            cnp_received_mlx(qp);
        }
    }
    
    switch (m_cc_mode)
    {
    case CC_MODE::HPCC:
        // Not only HPCC, but also PowerTCP!
        HandleAckHp(qp, p, ch);
        break;
    case CC_MODE::TIMELY:
        HandleAckTimely(qp, p, ch);
        break;
    case CC_MODE::DCTCP:
        HandleAckDctcp(qp, p, ch);
        break;
    case CC_MODE::HPCC_PINT:
        HandleAckHpPint(qp, p, ch);
        break;
    case CC_MODE::PATCHED_TIMELY:
        HandleAckPatchedTimely(qp, p, ch);
        break;
    case CC_MODE::SWIFT:
        HandleAckSwift(qp, p, ch);
        break;
    case CC_MODE::RTT_QCN:
        HandleAckRttQcn(qp, p, ch);
        break;
    case CC_MODE::POWERQCN:
        HandleAckPowerQcn(qp, p, ch);
        break;
    case CC_MODE::MLX_CNP:
        break;
    default:
        NS_ABORT_MSG("Unknown CC mode");
        break;
    }
    // ACK may advance the on-the-fly window, allowing more packets to send
    dev->TriggerTransmit();
    return 0;
}

// Forward decl: SrTxRecordBit is a static inline helper defined further down
// in the file (alongside the other SR helpers). HandleAckForSR calls it for
// the ePSN fast-sync path.
static inline void SrTxRecordBit(Ptr<RdmaQueuePair> qp, uint32_t seq,
                                 uint32_t mtu, uint32_t K);

void RdmaHw::HandleAckForSR(Ptr<RdmaQueuePair> qp,uint32_t seq,CustomHeader& ch) {
    if (m_srEnabled && m_srMode != 0)
    {
        // Fast-sync from receiver's ePSN. The bare seq field only tells us
        // about this one packet; ePSN reflects how far the receiver has
        // walked past holes. When ePSN > snd_una, mark every PSN in
        // [snd_una, ePSN) as received in the sender's bitmap so the next
        // SrTxAdvanceEACK / IrnTxHandleAck can short-circuit through it.
        // For SRNIC the swBitMap doesn't shift on cumulative advance, so we
        // just rely on Acknowledge(ePSN); for RaLL/IRN we fold the bits in.
        uint32_t epsn = ch.ack.epsn;
        if (epsn > qp->snd_una)
        {
            if (m_srMode == 1)
            {
                // RaLL: proper jump-ahead promote. Slides hwAnchor and
                // soft_eACK in lockstep with snd_una so the bitmap invariant
                // (anchors track snd_una closely) is preserved, even when
                // the receiver leaps far past the sender's view.
                SrTxFastSyncFromEpsn(qp, epsn);
            }
            else if (m_srMode == 3)
            {
                if (qp->sr.hwBitMap.size() != m_srK)
                {
                    qp->sr.hwBitMap.assign(m_srK, 0);
                }
                // Slide IRN's hwBitMap: shift left by (epsn-snd_una)/MTU.
                uint32_t shift = (epsn - qp->snd_una) / m_mtu;
                qp->Acknowledge(epsn);
                if (shift >= m_srK)
                {
                    for (uint32_t i = 0; i < m_srK; i++)
                    {
                        qp->sr.hwBitMap[i] = 0;
                    }
                }
                else
                {
                    for (uint32_t i = shift; i < m_srK; i++)
                    {
                        qp->sr.hwBitMap[i - shift] = qp->sr.hwBitMap[i];
                    }
                    for (uint32_t i = m_srK - shift; i < m_srK; i++)
                    {
                        qp->sr.hwBitMap[i] = 0;
                    }
                }
                // Re-validate the queued retransmit against the shifted
                // bitmap. The mode-specific TxHandleAck below will run
                // IrnTxRequeueNextHole again if there's still a gap.
                IrnTxRequeueNextHole(qp, qp->snd_una);
            }
            else if (m_srMode == 2)
            {
                uint32_t shift = (epsn - qp->snd_una) / m_mtu;
                qp->Acknowledge(epsn);
                // SRNIC: erase the leading shift entries (they cover already
                // cumulatively-acked PSNs and would otherwise mis-anchor the
                // bitmap once snd_una jumped).
                if (shift > 0 && !qp->sr.swBitMap.empty())
                {
                    size_t toErase = std::min((size_t)shift,
                                              qp->sr.swBitMap.size());
                    qp->sr.swBitMap.erase(qp->sr.swBitMap.begin(),
                                          qp->sr.swBitMap.begin() + toErase);
                }
                // Drop confirmed entries from the in-flight retransmit set.
                while (!qp->sr.srnicInflightRetx.empty() &&
                       *qp->sr.srnicInflightRetx.begin() < qp->snd_una)
                {
                    qp->sr.srnicInflightRetx.erase(
                        qp->sr.srnicInflightRetx.begin());
                }
            }
            else if (m_srMode == 4)
            {
                // CX6 / GBN: advance snd_una to receiver's ePSN.
                qp->Acknowledge(epsn);
            }
            if (qp->snd_una >= qp->sr.sackHigh + m_mtu)
            {
                qp->sr.SendOutOfOrder = false;
                qp->test.recoveryState = 0;
            }
            // Drop stale entries from the retrans queue
            while (!qp->sr.retransQueue.empty() &&
                   qp->sr.retransQueue.front() < qp->snd_una)
            {
                qp->sr.retransQueue.erase(qp->sr.retransQueue.begin());
            }
        }
        uint32_t expectedPSN = epsn;
        if (ch.l3Prot == 0xFC)
        {
            switch (m_srMode)
            {
                case 2: SrnicTxHandleAck(qp, seq); break;
                case 3: IrnTxHandleAck(qp, seq);   break;
                case 4: Cx6TxHandleAck(qp, seq);   break;
                case 1:
                default: SrTxHandleAck(qp, seq);   break;
            }
        }
        else if (ch.l3Prot == 0xFD)
        {
            switch (m_srMode)
            {
                case 2: SrnicTxHandleSack(qp, seq); break;
                case 3: IrnTxHandleSack(qp, seq);   break;
                case 4: Cx6TxHandleSack(qp, seq);   break;
                case 1:
                default: SrTxHandleSack(qp, seq, expectedPSN); break;
            }
        }
        qp->test.last_updateTime = Simulator::Now();
        // FACK accounting + RTO. We no longer do `snd_nxt = snd_una` (legacy
        // go-back-N reset); instead the sender stays at snd_nxt, IsWinBound
        // subtracts sackedBytes from OnTheFly to free cwnd, and RTO catches
        // any retransmit that itself was lost. Note: we do NOT cancel/rearm
        // the timer on every ACK/SACK - that would push it forward forever
        // under high ACK rate. The timer fires at its scheduled slot; on fire
        // it computes elapsed-since-lastAckTime and either acts or reschedules
        // for the remaining gap.
        SrTxRecomputeSackedBytes(qp);

        // Fast retransmit: if snd_una hasn't advanced past sackHigh (a hole
        // exists at the head) and retransQueue is empty, push snd_una so the
        // device picks it up before RTO times out. Throttled to once per
        // 1*baseRTT per snd_una value to avoid retrans storms when many SACKs
        // arrive in burst for the same hole.
        if (qp->sr.SendOutOfOrder && qp->sr.sackHigh > qp->snd_una &&
            qp->sr.retransQueue.empty())
        {
            // Throttle: 2*baseRTT, floored at 30us. Below this we'd fire faster
            // than retrans round-trip, generating duplicates.
            uint64_t throttleNs = std::max<uint64_t>(30000ull, 2ull * qp->m_baseRtt);
            if (qp->sr.lastFastRetransPsn != qp->snd_una ||
                (Simulator::Now() - qp->sr.lastFastRetransTime).GetNanoSeconds()
                    >= (int64_t)throttleNs)
            {
                qp->sr.retransQueue.push_back(qp->snd_una);
                qp->sr.lastFastRetransPsn = qp->snd_una;
                qp->sr.lastFastRetransTime = Simulator::Now();
            }
        }
        qp->sr.lastAckTime = Simulator::Now();
        if (!qp->sr.rtoTimer.IsRunning())
        {
            SrTxArmRTO(qp);
        }
        return;
    }

    if (ch.l3Prot == 0xFD) //如果收到sack包
    { // NACK
        //printf("n ack rate: %lu\n",qp->m_rate.GetBitRate());
        //RecoverQueue(qp);
            printf("sack seq: %u expected ack-seq: %lu \n",seq, qp->snd_una);
            if (qp->test.recoveryState == 0) { //第一次乱序
                qp->test.bitMap.resize(((seq - qp->snd_una) / m_mtu) + 1,0);
                qp->test.bitMap[(seq - qp->snd_una) / m_mtu] = 1;
                qp->test.targetRecoverySeq = qp->snd_nxt;
                qp->snd_nxt = qp->snd_una;
                qp->test.recoveryTag = qp->snd_una;
                qp->test.rightSeq =seq;
                qp->test.recoveryState = 1;
            } else { //不是第一次乱序
                printf("sack seq: %u right: %u \n",seq, qp->test.rightSeq);
                if (qp->test.rightSeq < seq) {
                    qp->test.bitMap.resize(((seq - qp->snd_una) / m_mtu) + 1,0);
                    qp->test.rightSeq = seq;
                }
                qp->test.bitMap[(seq - qp->snd_una) / m_mtu] = 1;
                // 发送所有空隙包
            }
            std::cout << " ----Bitmap: ";
            for (int bit : qp->test.bitMap) {
                std::cout << bit << " ";
            }
            std::cout << std::endl;
    } else if (ch.l3Prot == 0xFC) { //如果收到ack包：可以认为收到ack之前的包都收到了
        printf("ack seq: %u expected ack-seq: %lu \n",seq, qp->snd_una);
        if (qp->test.recoveryState == 1) { //还处于乱序状态
            if (seq >= qp->test.rightSeq) {
                qp->test.bitMap.erase(qp->test.bitMap.begin(),qp->test.bitMap.end());
                qp->Acknowledge(seq + m_mtu);
                qp->test.rightSeq = seq;
            } else {
                std::vector<int>::iterator k = qp->test.bitMap.begin() + (seq - qp->snd_una) / m_mtu + 1;
                int index = 0;
                for (int i = (seq - qp->snd_una) / m_mtu + 1; i <= (qp->test.rightSeq - qp->snd_una) / m_mtu; i++) {
                    index = i;
                    if (qp->test.bitMap[i] == 0) {
                        //index = i;
                        break;
                    } else if (i == (qp->test.rightSeq - qp->snd_una) / m_mtu) {
                        index = i + 1;
                    }
                    k++;
                }
                std::cout << " ====Bitmap: ";
                for (int bit : qp->test.bitMap) {
                    std::cout << bit << " ";
                }
                std::cout << std::endl;
                qp->test.bitMap.erase(qp->test.bitMap.begin(),k);
                qp->Acknowledge(qp->snd_una + index * m_mtu);
            }
            qp->test.last_updateTime = Simulator::Now();
            qp->test.recoveryTag = qp->snd_una;
            if (qp->snd_una >= qp->test.targetRecoverySeq) { //所有乱序包都收到了
                qp->test.recoveryState = 0;
            }
            qp->snd_nxt = qp->snd_una; //重发哪一个数据包待商榷，如何连续控制snd_nxt增长
            printf("update:::ack seq: %u expected ack-seq: %lu \n",seq, qp->snd_una);
        } else {
            qp->Acknowledge(seq + m_mtu);
            qp->test.last_updateTime = Simulator::Now();
        }
    }
}

int
RdmaHw::Receive(Ptr<Packet> p, CustomHeader& ch)
{
    if (ch.l3Prot == 0x11)
    { // UDP
        ReceiveUdp(p, ch);
    }
    else if (ch.l3Prot == 0xFF)
    { // CNP
        ReceiveCnp(p, ch);
    }
    else if (ch.l3Prot == 0xFD)
    { // NACK
        ReceiveAck(p, ch);
    }
    else if (ch.l3Prot == 0xFC)
    { // ACK
        ReceiveAck(p, ch);
    }
    return 0;
}

int
RdmaHw::ReceiverCheckSeq(uint32_t seq, Ptr<RdmaRxQueuePair> q, uint32_t size)
{
    // 判断状态+填充位图，更新nextseq，注意区分字节数和包
    uint32_t expected = q->ReceiverNextExpectedSeq;

    if (m_srEnabled)
    {
        // New hardware/software bitmap path. The helper updates ePSN, the
        // hwBitMap and may issue a sw update. After it returns, the only
        // thing we still need to decide is the ACK type (ACK vs NACK).
        bool wasInOrder       = (seq == expected) && !q->sr.ReceiveOutOfOrder;
        bool isDuplicate      = (seq < expected);
        // Dispatch to mode-specific receiver. Without this, IRN/SRNIC reuse
        // RaLL's swBitMap-based receiver which accepts arbitrarily-far OOO
        // packets and SACKs them all -- but the IRN sender expects SACKs
        // bounded by snd_una+K*MTU. The mismatch causes IRN sackHigh to
        // drift far beyond hwLimit, breaking IrnTxFindNextHole's hole
        // detection (only scans up to hwLimit) and stalling flows under
        // burst loss with K << BDP.
        switch (m_srMode)
        {
            case 2: SrnicRxHandlePacket(q, seq); break;
            case 3: IrnRxHandlePacket(q, seq);   break;
            case 4: Cx6RxHandlePacket(q, seq);   break;
            case 1:
            default: SrRxHandlePacket(q, seq);   break;
        }

        if (isDuplicate)
        {
            return 1; // duplicate -> still ack so sender can move on
        }
        if (wasInOrder)
        {
            if (q->ReceiverNextExpectedSeq >= q->m_milestone_rx)
            {
                q->m_milestone_rx += m_ack_interval;
            }
            return 1; // generate ACK
        }
        if (seq == expected && q->sr.ReceiveOutOfOrder == false)
        {
            // We were in SR mode but advanced past the gap with this packet
            return 1; // generate ACK reflecting the new ePSN
        }
        if (seq == expected)
        {
            // Filled the next expected slot but more holes remain
            return 1;
        }
        // Out-of-order arrival; reply SACK so the sender learns the gap
        if (q->test.sack_high < seq)
        {
            q->test.sack_high = seq;
        }
        if (q->m_lastNACK != q->ReceiverNextExpectedSeq)
        {
            q->m_lastNACK = q->ReceiverNextExpectedSeq;
            if (m_backto0)
            {
                q->ReceiverNextExpectedSeq =
                    q->ReceiverNextExpectedSeq / m_chunk * m_chunk;
            }
            return 2;
        }
        return 2;
    }

    // ----- legacy path (kept for reference / fallback) -----
    if (seq == expected) {
        //printf("seq : %u\n",seq);
        //q->ReceiverNextExpectedSeq = expected + size;
        if(q->test.recoveryState == 1) {//处于乱序状态，即将恢复reset
            // q->test.recoveryState = 0;
            std::cout << "last_seq: " << expected / m_mtu;
            std::cout << " Bitmap: ";
            bool flag = false;
            int index = 0;
            std::vector<int>::iterator k = q->test.bitMap.begin();
            for (int i = 0; i < q->test.bitMap.size(); i++) {
                std::cout << q->test.bitMap[i] << " ";
                if (i == 0) {
                    k++;
                } else if(q->test.bitMap[i] == 1 && !flag) {
                    k++;
                } else if (q->test.bitMap[i] == 0 && !flag) {
                    flag = 1;
                    q->ReceiverNextExpectedSeq += i * m_mtu;
                }
                if (i == q->test.bitMap.size() - 1 && !flag) {
                    q->ReceiverNextExpectedSeq += (i + 1) * m_mtu;
                }
            }
            std::cout << std::endl;
            //q->test.bitMap.resize(1,0);
            q->test.bitMap.erase(q->test.bitMap.begin(),k);
            if (q->ReceiverNextExpectedSeq >= q->test.sack_high) {
                q->test.recoveryState = 0;
            }                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 
        } else {
            q->ReceiverNextExpectedSeq += m_mtu;
        }
        if (q->ReceiverNextExpectedSeq >= q->m_milestone_rx) {
            q->m_milestone_rx += m_ack_interval;
            return 1; // Generate ACK
        }
        else if (q->ReceiverNextExpectedSeq % m_chunk == 0) {
            return 1;
        }
        else {
            // return 5;
            return 1;
        }
    } else if (seq > expected) {
        // Generate NACK
        if (q->test.sack_high < seq) {
            q->test.sack_high = seq;
        }
        printf("nack seq: %u expected: %u \n",seq, expected);
        if (q->test.recoveryState != 1) {
            q->test.recoveryState = 1;
            q->test.bitMap.resize(1,0);
        }
        if (q->test.recoveryState == 1) {
            if((seq - expected)/m_mtu + 1> q->test.bitMap.size()) { //否则前面空隙包重传包也丢失了
                q->test.bitMap.resize((seq - expected)/m_mtu + 1,0);
            }
            q->test.bitMap[(seq - expected)/m_mtu] = 1; 

            std::cout << " Bitmap: ";
            for (int bit : q->test.bitMap) {
                std::cout << bit << " ";
            }
            std::cout << std::endl;
        }
        //if (Simulator::Now() >= q->m_nackTimer || q->m_lastNACK != expected) {
        printf("last Nack-seq: %u expected: %u \n",q->m_lastNACK, expected);
        if (q->m_lastNACK != expected) {
            //q->m_nackTimer = Simulator::Now() + MicroSeconds(m_nack_interval);
            q->m_lastNACK = expected;
            if (m_backto0) {
                q->ReceiverNextExpectedSeq = q->ReceiverNextExpectedSeq / m_chunk * m_chunk;
            }
            return 2;
        }
        else {
            // return 4;
            return 2; //要逐包回复
        }
    }
    else {
        // Duplicate.
        // return 3; 已经来过不回复的话，后面ack可以一起回掉，但是可能这个ack也丢掉了
        return 1;
    }
}

void
RdmaHw::AddHeader(Ptr<Packet> p, uint16_t protocolNumber)
{
    PppHeader ppp;
    ppp.SetProtocol(EtherToPpp(protocolNumber));
    p->AddHeader(ppp);
}

uint16_t
RdmaHw::EtherToPpp(uint16_t proto)
{
    switch (proto)
    {
    case 0x0800:
        return 0x0021; // IPv4
    case 0x86DD:
        return 0x0057; // IPv6
    default:
        NS_ASSERT_MSG(false, "PPP Protocol number not defined!");
    }
    return 0;
}

void
RdmaHw::RecoverQueue(Ptr<RdmaQueuePair> qp)
{
    qp->snd_nxt = qp->snd_una;
}

void
RdmaHw::QpComplete(Ptr<RdmaQueuePair> qp)
{
    NS_ASSERT(!m_qpCompleteCallback.IsNull());
    if (m_cc_mode == CC_MODE::MLX_CNP)
    {
        Simulator::Cancel(qp->mlx.m_eventUpdateAlpha);
        Simulator::Cancel(qp->mlx.m_eventDecreaseRate);
        Simulator::Cancel(qp->mlx.m_rpTimer);
    }
    if (qp->sr.rtoTimer.IsRunning())
    {
        Simulator::Cancel(qp->sr.rtoTimer);
    }

    // This callback will log info
    // It may also delete the rxQp on the receiver
    m_qpCompleteCallback(qp);

    qp->m_notifyAppFinish();

    // delete the qp
    DeleteQueuePair(qp);
}

void
RdmaHw::SetLinkDown(Ptr<QbbNetDevice> dev)
{
    printf("RdmaHw: node:%u a link down\n", m_node->GetId());
}

void
RdmaHw::AddTableEntry(Ipv4Address& dstAddr, uint32_t intf_idx)
{
    uint32_t dip = dstAddr.Get();
    m_rtTable[dip].push_back(intf_idx);
}

void
RdmaHw::ClearTable()
{
    m_rtTable.clear();
}

void
RdmaHw::RedistributeQp()
{
    // clear old qpGrp
    for (uint32_t i = 0; i < m_nic.size(); i++)
    {
        if (m_nic[i].dev == NULL)
        {
            continue;
        }
        m_nic[i].qpGrp->Clear();
    }

    // redistribute qp
    for (auto& it : m_qpMap)
    {
        Ptr<RdmaQueuePair> qp = it.second;
        uint32_t nic_idx = GetNicIdxOfQp(qp);
        m_nic[nic_idx].qpGrp->AddQp(qp);
        // Notify Nic
        m_nic[nic_idx].dev->ReassignedQp(qp);
    }
}

Ptr<Packet>
RdmaHw::GetNxtPacket(Ptr<RdmaQueuePair> qp)
{
    // Decide PSN: drain retransQueue first; otherwise advance forward.
    uint32_t pktSeq = qp->snd_nxt;
    bool isRetrans = false;
    if (m_srEnabled && m_srMode != 0)
    {
        while (!qp->sr.retransQueue.empty())
        {
            uint32_t psn = qp->sr.retransQueue.front();
            qp->sr.retransQueue.erase(qp->sr.retransQueue.begin());
            if (psn < qp->snd_una)
            {
                continue; // already cumulatively ACKed
            }
            // Skip if already individually SACKed. SrTxIsPsnAcked walks the
            // mode-appropriate bitmap layout (RaLL: hw+compressed+sw, IRN: hw,
            // SRNIC: sw anchored at snd_una). With the SRNIC swBitMap-shift
            // bug fixed (SrnicTxHandleAck now erases the leading cum_shift
            // entries), consulting SRNIC's swBitMap is safe and avoids
            // retransmitting a PSN whose SACK arrived in the async window
            // between the queue push and this dequeue.
            if (m_srMode == 1 || m_srMode == 2)
            {
                if (SrTxIsPsnAcked(qp, psn))
                {
                    continue;
                }
            }
            else if (m_srMode == 3)
            {
                uint32_t hwLimit = qp->snd_una + m_srK * m_mtu;
                if (psn >= qp->snd_una && psn < hwLimit)
                {
                    if (qp->sr.hwBitMap[(psn - qp->snd_una) / m_mtu] == 1)
                    {
                        continue;
                    }
                }
            }
            pktSeq = psn;
            isRetrans = true;
            // For IRN: after picking this PSN, scan bitmap for the NEXT hole
            // above it and queue that as the candidate for the next slot.
            // This mirrors the official IRN findNewHole flow (txFree.cpp):
            // every successful retransmit triggers a fresh hole-scan rather
            // than relying on a pre-enumerated queue.
            if (m_srMode == 3)
            {
                IrnTxRequeueNextHole(qp, psn + m_mtu);
            }
            // SRNIC: same single-in-flight invariant, software-side hole scan.
            // Paper §4.4.2: software pushes one retransmit at a time into
            // RetryQ; bitmap is the source of truth.
            else if (m_srMode == 2)
            {
                qp->sr.srnicInflightRetx.insert(psn);
                SrnicTxRequeueNextHole(qp, psn + m_mtu);
            }
            break;
        }
    }

    // Payload size: forward send uses GetBytesLeft (capped at MTU); retrans
    // assumes MTU-sized original packet.
    uint64_t payload_size;
    if (isRetrans)
    {
        payload_size = m_mtu;
    }
    else
    {
        payload_size = qp->GetBytesLeft();
        if (m_mtu < payload_size)
        {
            payload_size = m_mtu;
        }
    }
    Ptr<Packet> p = Create<Packet>(payload_size);
    uint32_t sentBytes = qp->m_size - qp->GetBytesLeft();
    uint32_t nic_idx = GetNicIdxOfQp(qp);
    DataRate m_bps = m_nic[nic_idx].dev->GetDataRate();
    double bdp = m_bps.GetBitRate() * 1 * qp->m_baseRtt * 1e-9 / 8;
    UnSchedTag unschedtag;
    if (sentBytes <= bdp)
    {
        unschedtag.SetValue(1);
    }
    else
    {
        unschedtag.SetValue(0);
    }
    p->AddPacketTag(unschedtag);
    // add SeqTsHeader
    SeqTsHeader seqTs;
    seqTs.SetSeq(pktSeq);
    seqTs.SetPG(qp->m_pg);
    p->AddHeader(seqTs);
    // add udp header
    UdpHeader udpHeader;
    udpHeader.SetDestinationPort(qp->dport);
    udpHeader.SetSourcePort(qp->sport);
    p->AddHeader(udpHeader);
    // add ipv4 header
    Ipv4Header ipHeader;
    ipHeader.SetSource(qp->sip);
    ipHeader.SetDestination(qp->dip);
    ipHeader.SetProtocol(0x11);
    ipHeader.SetPayloadSize(p->GetSize());
    ipHeader.SetTtl(64);
    ipHeader.SetTos(0);
    ipHeader.SetIdentification(qp->m_ipid);
    p->AddHeader(ipHeader);
    // add ppp header
    PppHeader ppp;
    ppp.SetProtocol(0x0021); // EtherToPpp(0x800), see point-to-point-net-device.cc
    p->AddHeader(ppp);

    // Advance forward state only for non-retransmissions.
    if (!isRetrans)
    {
        updateSendNxt(qp, payload_size);
    }
    qp->m_ipid++;

    // Arm RTO on first send so tail-loss doesn't silently stall a flow that
    // never enters SR mode. Subsequent ACK/SACK arrivals refresh lastAckTime
    // and rearm; if no ACK arrives for 2*baseRTT, RTO scans for unacked PSNs.
    if (m_srEnabled && m_srMode != 0 && !qp->sr.rtoTimer.IsRunning())
    {
        qp->sr.lastAckTime = Simulator::Now();
        SrTxArmRTO(qp);
    }

    // return
    return p;
}

void RdmaHw::updateSendNxt(Ptr<RdmaQueuePair> qp,uint64_t payload_size) {
    // Legacy logic (used by all modes when not in selective retransmit)
    if (qp->test.recoveryState == 0 || (qp->test.bitMap.size() == 0)) {
        qp->snd_nxt += payload_size;
        qp->test.targetRecoverySeq = qp->snd_nxt;
    } else if (Simulator::Now().GetNanoSeconds() >= (qp->test.last_updateTime.GetNanoSeconds() + qp->test.last_updateInterval)) {
        qp->snd_nxt = qp->snd_una;
        qp->test.recoveryTag = qp->snd_nxt;
        qp->test.last_updateTime = Simulator::Now();
    } else if (qp->test.recoveryTag < qp->test.rightSeq) {
        int index = (qp->test.recoveryTag - qp->snd_una) / m_mtu + 1;
        int right = (qp->test.rightSeq - qp->snd_una) / m_mtu;
        for (int i = index; i <= right; i++) {
            if (qp->test.bitMap[i] == 0) {
                qp->snd_nxt = qp->snd_una + i * m_mtu;
                qp->test.recoveryTag = qp->snd_nxt;
                break;
            }
            if (i == right) { //跳到recover or 根据recover继续++
                qp->test.recoveryTag = qp->test.rightSeq;
                if (qp->snd_nxt >= qp->test.targetRecoverySeq && qp->snd_nxt + payload_size < qp->m_size) {
                    qp->snd_nxt += payload_size;
                    qp->test.targetRecoverySeq = qp->snd_nxt;
                } else if (qp->snd_nxt >= qp->test.targetRecoverySeq && qp->snd_nxt + payload_size >= qp->m_size) {
                    qp->snd_nxt = qp->snd_una;
                    qp->test.recoveryTag = qp->snd_nxt;
                    qp->test.last_updateTime = Simulator::Now();
                } else if (qp->test.targetRecoverySeq == qp->test.rightSeq) {
                    qp->snd_nxt = qp->snd_una;
                    qp->test.recoveryTag = qp->snd_nxt;
                    qp->test.last_updateTime = Simulator::Now();
                }
                else
                    qp->snd_nxt = qp->test.targetRecoverySeq;
            }
        }
    } else if (qp->test.recoveryTag >= qp->test.rightSeq && qp->snd_nxt + payload_size < qp->m_size) {
        qp->snd_nxt += payload_size;
        qp->test.targetRecoverySeq = qp->snd_nxt;
    } else if (qp->test.recoveryTag >= qp->test.rightSeq && qp->snd_nxt + payload_size >= qp->m_size) {
        qp->snd_nxt = qp->snd_una;
        qp->test.recoveryTag = qp->snd_nxt;
    }
    /*
    if (((qp->test.targetRecoverySeq + payload_size) < qp->m_size) && ((qp->test.recoveryState == 0) || (qp->test.bitMap.size() == 0) || (qp->test.recoveryTag >= qp->test.rightSeq) && (Simulator::Now().GetNanoSeconds() < (qp->test.last_updateTime.GetNanoSeconds() + qp->test.last_updateInterval)))) {
        if (qp->snd_nxt + payload_size == qp->m_size)
            std::cout<<" qp->snd_nxt + payload_size == qp->m_size "<<std::endl;
        qp->snd_nxt += payload_size;
        qp->test.targetRecoverySeq = qp->snd_nxt;
    } else {
        int index = (qp->test.recoveryTag - qp->snd_una) / m_mtu + 1;
        int right = (qp->test.rightSeq - qp->snd_una) / m_mtu;
        if (Simulator::Now().GetNanoSeconds() >= qp->test.last_updateTime.GetNanoSeconds() + qp->test.last_updateInterval || (qp->test.targetRecoverySeq + payload_size) >= qp->m_size) {
            std::cout<<"last update Time:"<<qp->test.last_updateTime<<" :now:"<<Simulator::Now()<<" :interval:"<<qp->test.last_updateInterval<<std::endl;
            qp->test.recoveryTag = qp->snd_una;
            qp->test.last_updateTime = Simulator::Now();
            index = 0;
        }
        for (int i = index; i <= right; i++) {
            if (qp->test.bitMap[i] == 0) {
                qp->snd_nxt = qp->snd_una + i * m_mtu;
                qp->test.recoveryTag = qp->snd_nxt;
                break;
            }
            if (i == right) {
                qp->test.recoveryTag = qp->test.rightSeq;
                if (qp->snd_nxt >= qp->test.targetRecoverySeq) {
                    qp->snd_nxt += payload_size;
                    qp->test.targetRecoverySeq = qp->snd_nxt;
                }
                else 
                    qp->snd_nxt = qp->test.targetRecoverySeq;
            }
        }
    }
    std::cout <<"GetNxtPacket: update"<< qp->snd_nxt << " " << qp->snd_una<<std::endl;
    */
}

void
RdmaHw::PktSent(Ptr<RdmaQueuePair> qp, Ptr<Packet> pkt, Time interframeGap) const
{
    qp->lastPktSize = pkt->GetSize();
    //	SeqTsHeader seqTs;
    //	pkt->PeekHeader(seqTs);
    uint32_t seq = qp->snd_nxt;
    qp->rates[qp->snd_nxt] = Simulator::Now().GetNanoSeconds();
    UpdateNextAvail(qp, interframeGap, pkt->GetSize());
}

void
RdmaHw::UpdateNextAvail(Ptr<RdmaQueuePair> qp, Time interframeGap, uint32_t pkt_size) const
{
    Time sendingTime;
    if (m_rateBound)
    {
        sendingTime = interframeGap + qp->m_rate.CalculateBytesTxTime(pkt_size);
    }
    else
    {
        sendingTime = interframeGap + qp->m_max_rate.CalculateBytesTxTime(pkt_size);
    }
    qp->m_nextAvail = std::max(Simulator::Now() + sendingTime, qp->m_nextAvail);
}

void
RdmaHw::ChangeRate(Ptr<RdmaQueuePair> qp, DataRate new_rate)
{
#if 1
    Time sendingTime = qp->m_rate.CalculateBytesTxTime(qp->lastPktSize);
    Time new_sendintTime = new_rate.CalculateBytesTxTime(qp->lastPktSize);
    qp->m_nextAvail = qp->m_nextAvail + new_sendintTime - sendingTime;
    // update nic's next avail event
    uint32_t nic_idx = GetNicIdxOfQp(qp);
    m_nic[nic_idx].dev->UpdateNextAvail(qp->m_nextAvail);
#endif
    // change to new rate
    qp->m_rate = new_rate;
}

#define PRINT_LOG 0

/******************************
 * Mellanox's version of DCQCN
 *****************************/
void
RdmaHw::UpdateAlphaMlx(Ptr<RdmaQueuePair> q)
{
#if PRINT_LOG
    // std::cout << Simulator::Now() << " alpha update:" << m_node->GetId() << ' ' << q->mlx.m_alpha
    // << ' ' << (int)q->mlx.m_alpha_cnp_arrived << '\n'; printf("%lu alpha update: %08x %08x %u %u
    // %.6lf->", Simulator::Now().GetTimeStep(), q->sip.Get(), q->dip.Get(), q->sport, q->dport,
    // q->mlx.m_alpha);
#endif
    if (q->mlx.m_alpha_cnp_arrived)
    {
        q->mlx.m_alpha = (1 - m_g) * q->mlx.m_alpha + m_g; // binary feedback
    }
    else
    {
        q->mlx.m_alpha = (1 - m_g) * q->mlx.m_alpha; // binary feedback
    }
#if PRINT_LOG
    // printf("%.6lf\n", q->mlx.m_alpha);
#endif
    q->mlx.m_alpha_cnp_arrived = false; // clear the CNP_arrived bit
    ScheduleUpdateAlphaMlx(q);
}

void
RdmaHw::ScheduleUpdateAlphaMlx(Ptr<RdmaQueuePair> q)
{
    q->mlx.m_eventUpdateAlpha = Simulator::Schedule(MicroSeconds(m_alpha_resume_interval),
                                                    &RdmaHw::UpdateAlphaMlx,
                                                    this,
                                                    q);
}

void
RdmaHw::cnp_received_mlx(Ptr<RdmaQueuePair> q)
{
    q->mlx.m_alpha_cnp_arrived = true;    // set CNP_arrived bit for alpha update
    q->mlx.m_decrease_cnp_arrived = true; // set CNP_arrived bit for rate decrease
    if (q->mlx.m_first_cnp)
    {
        // init alpha
        q->mlx.m_alpha = 1;
        q->mlx.m_alpha_cnp_arrived = false;
        // schedule alpha update
        ScheduleUpdateAlphaMlx(q);
        // schedule rate decrease
        ScheduleDecreaseRateMlx(q, 1); // add 1 ns to make sure rate decrease is after alpha update
        // set rate on first CNP
        q->mlx.m_targetRate = q->m_rate = m_rateOnFirstCNP * q->m_rate;
        q->mlx.m_first_cnp = false;
    }
}

void
RdmaHw::CheckRateDecreaseMlx(Ptr<RdmaQueuePair> q)
{
    ScheduleDecreaseRateMlx(q, 0);
    if (q->mlx.m_decrease_cnp_arrived)
    {
#if PRINT_LOG
        printf("%lu rate dec: %08x %08x %u %u (%0.3lf %.3lf)->",
               Simulator::Now().GetTimeStep(),
               q->sip.Get(),
               q->dip.Get(),
               q->sport,
               q->dport,
               q->mlx.m_targetRate.GetBitRate() * 1e-9,
               q->m_rate.GetBitRate() * 1e-9);
#endif
        bool clamp = true;
        if (!m_EcnClampTgtRate)
        {
            if (q->mlx.m_rpTimeStage == 0)
            {
                clamp = false;
            }
        }
        if (clamp)
        {
            q->mlx.m_targetRate = q->m_rate;
        }
        q->m_rate = std::max(m_minRate, q->m_rate * (1 - q->mlx.m_alpha / 2));
        // reset rate increase related things
        q->mlx.m_rpTimeStage = 0;
        q->mlx.m_decrease_cnp_arrived = false;
        Simulator::Cancel(q->mlx.m_rpTimer);
        q->mlx.m_rpTimer = Simulator::Schedule(MicroSeconds(m_rpgTimeReset),
                                               &RdmaHw::RateIncEventTimerMlx,
                                               this,
                                               q);
#if PRINT_LOG
        printf("(%.3lf %.3lf)\n",
               q->mlx.m_targetRate.GetBitRate() * 1e-9,
               q->m_rate.GetBitRate() * 1e-9);
#endif
    }
}

void
RdmaHw::ScheduleDecreaseRateMlx(Ptr<RdmaQueuePair> q, uint32_t delta)
{
    q->mlx.m_eventDecreaseRate =
        Simulator::Schedule(MicroSeconds(m_rateDecreaseInterval) + NanoSeconds(delta),
                            &RdmaHw::CheckRateDecreaseMlx,
                            this,
                            q);
}

void
RdmaHw::RateIncEventTimerMlx(Ptr<RdmaQueuePair> q)
{
    q->mlx.m_rpTimer =
        Simulator::Schedule(MicroSeconds(m_rpgTimeReset), &RdmaHw::RateIncEventTimerMlx, this, q);
    RateIncEventMlx(q);
    q->mlx.m_rpTimeStage++;
}

void
RdmaHw::RateIncEventMlx(Ptr<RdmaQueuePair> q)
{
    // check which increase phase: fast recovery, active increase, hyper increase
    if (q->mlx.m_rpTimeStage < m_rpgThreshold)
    { // fast recovery
        FastRecoveryMlx(q);
    }
    else if (q->mlx.m_rpTimeStage == m_rpgThreshold)
    { // active increase
        ActiveIncreaseMlx(q);
    }
    else
    { // hyper increase
        HyperIncreaseMlx(q);
    }
}

void
RdmaHw::FastRecoveryMlx(Ptr<RdmaQueuePair> q)
{
#if PRINT_LOG
    printf("%lu fast recovery: %08x %08x %u %u (%0.3lf %.3lf)->",
           Simulator::Now().GetTimeStep(),
           q->sip.Get(),
           q->dip.Get(),
           q->sport,
           q->dport,
           q->mlx.m_targetRate.GetBitRate() * 1e-9,
           q->m_rate.GetBitRate() * 1e-9);
#endif
    q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
#if PRINT_LOG
    printf("(%.3lf %.3lf)\n",
           q->mlx.m_targetRate.GetBitRate() * 1e-9,
           q->m_rate.GetBitRate() * 1e-9);
#endif
}

void
RdmaHw::ActiveIncreaseMlx(Ptr<RdmaQueuePair> q)
{
#if PRINT_LOG
    printf("%lu active inc: %08x %08x %u %u (%0.3lf %.3lf)->",
           Simulator::Now().GetTimeStep(),
           q->sip.Get(),
           q->dip.Get(),
           q->sport,
           q->dport,
           q->mlx.m_targetRate.GetBitRate() * 1e-9,
           q->m_rate.GetBitRate() * 1e-9);
#endif
    // get NIC
    uint32_t nic_idx = GetNicIdxOfQp(q);
    Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;
    // increate rate
    q->mlx.m_targetRate += m_rai;
    if (q->mlx.m_targetRate > dev->GetDataRate())
    {
        q->mlx.m_targetRate = dev->GetDataRate();
    }
    q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
#if PRINT_LOG
    printf("(%.3lf %.3lf)\n",
           q->mlx.m_targetRate.GetBitRate() * 1e-9,
           q->m_rate.GetBitRate() * 1e-9);
#endif
}

void
RdmaHw::HyperIncreaseMlx(Ptr<RdmaQueuePair> q)
{
#if PRINT_LOG
    printf("%lu hyper inc: %08x %08x %u %u (%0.3lf %.3lf)->",
           Simulator::Now().GetTimeStep(),
           q->sip.Get(),
           q->dip.Get(),
           q->sport,
           q->dport,
           q->mlx.m_targetRate.GetBitRate() * 1e-9,
           q->m_rate.GetBitRate() * 1e-9);
#endif
    // get NIC
    uint32_t nic_idx = GetNicIdxOfQp(q);
    Ptr<QbbNetDevice> dev = m_nic[nic_idx].dev;
    // increate rate
    q->mlx.m_targetRate += m_rhai;
    if (q->mlx.m_targetRate > dev->GetDataRate())
    {
        q->mlx.m_targetRate = dev->GetDataRate();
    }
    q->m_rate = (q->m_rate / 2) + (q->mlx.m_targetRate / 2);
#if PRINT_LOG
    printf("(%.3lf %.3lf)\n",
           q->mlx.m_targetRate.GetBitRate() * 1e-9,
           q->m_rate.GetBitRate() * 1e-9);
#endif
}

/***********************
 * High Precision CC
 ***********************/
void
RdmaHw::HandleAckHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    uint32_t ack_seq = ch.ack.seq;
    // update rate
    if (ack_seq > qp->hp.m_lastUpdateSeq)
    { // if full RTT feedback is ready, do full update
        if (PowerTCPEnabled)
        {
            UpdateRatePower(qp, p, ch, false);
        }
        else
        {
            UpdateRateHp(qp, p, ch, false);
        }
    }
    else
    { // do fast react
        if (PowerTCPEnabled)
        {
            FastReactPower(qp, p, ch);
        }
        else
        {
            FastReactHp(qp, p, ch);
        }
    }
}

void
RdmaHw::UpdateRateHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool fast_react)
{
    uint32_t next_seq = qp->snd_nxt;
    bool print = true;

    if (qp->hp.m_lastUpdateSeq == 0)
    { // first RTT

        qp->hp.m_lastUpdateSeq = next_seq;
        // store INT
        IntHeader& ih = ch.ack.ih;
        NS_ASSERT(ih.nhop <= IntHeader::maxHop);
        for (uint32_t i = 0; i < ih.nhop; i++)
        {
            qp->hp.hop[i] = ih.hop[i];
        }
#if PRINT_LOG
        if (print)
        {
            printf("%lu %s %08x %08x %u %u [%u,%u,%u]",
                   Simulator::Now().GetTimeStep(),
                   fast_react ? "fast" : "update",
                   qp->sip.Get(),
                   qp->dip.Get(),
                   qp->sport,
                   qp->dport,
                   qp->hp.m_lastUpdateSeq,
                   ch.ack.seq,
                   next_seq);
            for (uint32_t i = 0; i < ih.nhop; i++)
                printf(" %u %lu %lu",
                       ih.hop[i].GetQlen(),
                       ih.hop[i].GetBytes(),
                       ih.hop[i].GetTime());
            printf("\n");
        }
#endif
    }
    else
    {
        // check packet INT
        IntHeader& ih = ch.ack.ih;
        if (ih.nhop <= IntHeader::maxHop)
        {
            double max_c = 0;
            bool inStable = false;
#if PRINT_LOG
            if (print)
                printf("%lu %s %08x %08x %u %u [%u,%u,%u]",
                       Simulator::Now().GetTimeStep(),
                       fast_react ? "fast" : "update",
                       qp->sip.Get(),
                       qp->dip.Get(),
                       qp->sport,
                       qp->dport,
                       qp->hp.m_lastUpdateSeq,
                       ch.ack.seq,
                       next_seq);
#endif
            // check each hop
            double U = 0;
            uint64_t dt = 0;
            bool updated[IntHeader::maxHop] = {false};
            bool updated_any = false;
            NS_ASSERT(ih.nhop <= IntHeader::maxHop);
            for (uint32_t i = 0; i < ih.nhop; i++)
            {
                if (m_sampleFeedback)
                {
                    if (ih.hop[i].GetQlen() == 0 and fast_react)
                    {
                        continue;
                    }
                }
                updated[i] = updated_any = true;
#if PRINT_LOG
                if (print)
                    printf(" %u(%u) %lu(%lu) %lu(%lu)",
                           ih.hop[i].GetQlen(),
                           qp->hp.hop[i].GetQlen(),
                           ih.hop[i].GetBytes(),
                           qp->hp.hop[i].GetBytes(),
                           ih.hop[i].GetTime(),
                           qp->hp.hop[i].GetTime());
#endif
                uint64_t tau = ih.hop[i].GetTimeDelta(qp->hp.hop[i]);
                double duration = tau * 1e-9;
                double txRate = (ih.hop[i].GetBytesDelta(qp->hp.hop[i])) * 8 / duration;

                double u;
                u = txRate / ih.hop[i].GetLineRate() +
                    (double)std::min(ih.hop[i].GetQlen(), qp->hp.hop[i].GetQlen()) *
                        qp->m_max_rate.GetBitRate() / ih.hop[i].GetLineRate() / qp->m_win;

#if PRINT_LOG
                if (print)
                    printf(" %.3lf %.3lf", txRate, u);
#endif
                if (!m_multipleRate)
                {
                    // for aggregate (single R)
                    if (u > U)
                    {
                        U = u;
                        dt = tau;
                    }
                }
                else
                {
                    // for per hop (per hop R)
                    if (tau > qp->m_baseRtt)
                    {
                        tau = qp->m_baseRtt;
                    }
                    qp->hp.hopState[i].u =
                        (qp->hp.hopState[i].u * (qp->m_baseRtt - tau) + u * tau) /
                        double(qp->m_baseRtt);
                }
                qp->hp.hop[i] = ih.hop[i];
            }

            DataRate new_rate;
            int32_t new_incStage;
            DataRate new_rate_per_hop[IntHeader::maxHop];
            int32_t new_incStage_per_hop[IntHeader::maxHop];
            if (!m_multipleRate)
            {
                // for aggregate (single R)
                if (updated_any)
                {
                    if (dt > 1.0 * qp->m_baseRtt)
                    {
                        dt = 1.0 * qp->m_baseRtt;
                    }

                    qp->hp.u = (qp->hp.u * (qp->m_baseRtt - dt) + U * dt) / double(qp->m_baseRtt);
                    max_c = qp->hp.u / m_targetUtil;

                    if (max_c >= 1 || qp->hp.m_incStage >= m_miThresh)
                    {
                        new_rate = qp->hp.m_curRate / max_c + m_rai;
                        new_incStage = 0;
                    }
                    else
                    {
                        new_rate = qp->hp.m_curRate + m_rai;
                        new_incStage = qp->hp.m_incStage + 1;
                    }

                    if (new_rate < m_minRate)
                    {
                        new_rate = m_minRate;
                    }
                    if (new_rate > qp->m_max_rate)
                    {
                        new_rate = qp->m_max_rate;
                    }
#if PRINT_LOG
                    if (print)
                        printf(" u=%.6lf U=%.3lf dt=%u max_c=%.3lf", qp->hp.u, U, dt, max_c);
#endif
#if PRINT_LOG
                    if (print)
                        printf(" rate:%.3lf->%.3lf\n",
                               qp->hp.m_curRate.GetBitRate() * 1e-9,
                               new_rate.GetBitRate() * 1e-9);
#endif
                }
            }
            else
            {
                // for per hop (per hop R)
                new_rate = qp->m_max_rate;
                for (uint32_t i = 0; i < ih.nhop; i++)
                {
                    if (updated[i])
                    {
                        double c = qp->hp.hopState[i].u / m_targetUtil;
                        if (c >= 1 || qp->hp.hopState[i].incStage >= m_miThresh)
                        {
                            new_rate_per_hop[i] = qp->hp.hopState[i].Rc / c + m_rai;
                            new_incStage_per_hop[i] = 0;
                        }
                        else
                        {
                            new_rate_per_hop[i] = qp->hp.hopState[i].Rc + m_rai;
                            new_incStage_per_hop[i] = qp->hp.hopState[i].incStage + 1;
                        }
                        // bound rate
                        if (new_rate_per_hop[i] < m_minRate)
                        {
                            new_rate_per_hop[i] = m_minRate;
                        }
                        if (new_rate_per_hop[i] > qp->m_max_rate)
                        {
                            new_rate_per_hop[i] = qp->m_max_rate;
                        }
                        // find min new_rate
                        if (new_rate_per_hop[i] < new_rate)
                        {
                            new_rate = new_rate_per_hop[i];
                        }
#if PRINT_LOG
                        if (print)
                            printf(" [%u]u=%.6lf c=%.3lf", i, qp->hp.hopState[i].u, c);
#endif
#if PRINT_LOG
                        if (print)
                            printf(" %.3lf->%.3lf",
                                   qp->hp.hopState[i].Rc.GetBitRate() * 1e-9,
                                   new_rate.GetBitRate() * 1e-9);
#endif
                    }
                    else
                    {
                        if (qp->hp.hopState[i].Rc < new_rate)
                        {
                            new_rate = qp->hp.hopState[i].Rc;
                        }
                    }
                }
#if PRINT_LOG
                printf("\n");
#endif
            }

            if (updated_any)
            {
                ChangeRate(qp, new_rate);
            }
            if (!fast_react)
            {
                if (updated_any)
                {
                    qp->hp.m_curRate = new_rate;
                    qp->hp.m_incStage = new_incStage;
                }
                if (m_multipleRate)
                {
                    // for per hop (per hop R)
                    for (uint32_t i = 0; i < ih.nhop; i++)
                    {
                        if (updated[i])
                        {
                            qp->hp.hopState[i].Rc = new_rate_per_hop[i];
                            qp->hp.hopState[i].incStage = new_incStage_per_hop[i];
                        }
                    }
                }
            }
        }
        if (!fast_react)
        {
            if (next_seq > qp->hp.m_lastUpdateSeq)
            {
                qp->hp.m_lastUpdateSeq = next_seq; //+ rand() % 2 * m_mtu;
            }
        }
    }
}

void
RdmaHw::FastReactHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    if (m_fast_react)
    {
        UpdateRateHp(qp, p, ch, true);
    }
}

/**********************
 * PowerTCP (Int/Delay versions) called from HandleAckHp function at the moment
 *********************/

void
RdmaHw::UpdateRatePower(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool fast_react)
{
    uint32_t next_seq = qp->snd_nxt;
    bool print = true;
    double prevRtt = qp->m_baseRtt;
    double prevCompletion = Simulator::Now().GetNanoSeconds();
    std::map<uint32_t, double>::iterator it = qp->rates.find(ch.ack.seq);
    DataRate old;
    double rtt;

    if (it != qp->rates.end())
    {
        qp->rates.erase(it);
        prevRtt = Simulator::Now().GetNanoSeconds() - it->second;
        if (PowerTCPdelay)
        {
            qp->m_baseRtt =
                std::min(uint64_t(Simulator::Now().GetNanoSeconds() - it->second), qp->m_baseRtt);
        }
        prevCompletion = Simulator::Now().GetNanoSeconds();
    }
    if (qp->hp.m_lastUpdateSeq == 0 && !PowerTCPdelay)
    {
        qp->prevRtt = prevRtt;
        qp->prevCompletion = Simulator::Now().GetNanoSeconds();
        qp->hp.m_lastUpdateSeq = next_seq;
        // store INT
        IntHeader& ih = ch.ack.ih;
        NS_ASSERT(ih.nhop <= IntHeader::maxHop);
        for (uint32_t i = 0; i < ih.nhop; i++)
        {
            qp->hp.hop[i] = ih.hop[i];
        }
    }
    else
    {
        // check packet INT
        IntHeader& ih = ch.ack.ih;
        if (ih.nhop <= IntHeader::maxHop)
        {
            double max_c = 0;
            bool inStable = false;
            // check each hop
            double U = 0;
            uint64_t dt = 0;
            bool updated[IntHeader::maxHop] = {false};
            bool updated_any = false;
            NS_ASSERT(ih.nhop <= IntHeader::maxHop);
            for (uint32_t i = 0; i < ih.nhop; i++)
            {
                if (m_sampleFeedback)
                {
                    if (ih.hop[i].GetQlen() == 0 and fast_react)
                    {
                        continue;
                    }
                }
                updated[i] = updated_any = true;

                uint64_t tau = ih.hop[i].GetTimeDelta(qp->hp.hop[i]);
                double duration = tau * 1e-9;
                double rxRate = (ih.hop[i].GetBytesDelta(qp->hp.hop[i])) * 8.0 / duration;

                double u;

                if (!PowerTCPdelay)
                {
                    double A = rxRate;
                    // double A = txRate + (double(ih.hop[i].GetQlen() * 8.0) -
                    // double(qp->hp.hop[i].GetQlen() * 8.0)) / duration;
                    double power = (A) * (double(ih.hop[i].GetQlen() * 8.0) +
                                          ih.hop[i].GetLineRate() * (qp->m_baseRtt * 1e-9));
                    double powerx = (power) / (ih.hop[i].GetLineRate() *
                                               (ih.hop[i].GetLineRate() * qp->m_baseRtt * 1e-9));
                    u = powerx; // PowerTCP
                }
                else
                {
                    // delay approach
                    double A =
                        (double(prevRtt - qp->prevRtt) / (prevCompletion - qp->prevCompletion) + 1);
                    if (A < 0.5)
                    {
                        A = 0.5;
                    }
                    double power = (A) * (prevRtt);
                    double powerx = (power) / (1.05 * qp->m_baseRtt);
                    u = powerx; // theta-PowerTCP
                }
                if (u > U)
                {
                    U = u;
                    if (PowerTCPdelay)
                    {
                        dt = prevCompletion - qp->prevCompletion;
                    }
                    else
                    {
                        dt = tau;
                    }
                }
                qp->hp.hop[i] = ih.hop[i];
            }

            DataRate new_rate;
            int32_t new_incStage = 0;
            DataRate new_rate_per_hop[IntHeader::maxHop];
            int32_t new_incStage_per_hop[IntHeader::maxHop];

            if (updated_any)
            {
                if (dt > 1.0 * qp->m_baseRtt)
                {
                    dt = 1.0 * qp->m_baseRtt;
                }

                if (U < 0)
                {
                    U = qp->hp.u;
                }
                qp->hp.u =
                    (qp->hp.u * (1.0 * qp->m_baseRtt - dt) + U * dt) / double(1.0 * qp->m_baseRtt);
                if (!PowerTCPdelay)
                {
                    max_c = qp->hp.u / m_targetUtil;
                    new_rate = (0.9 * (qp->hp.m_curRate / max_c + DataRate("150Mbps")) +
                                0.1 * qp->hp.m_curRate); // gamma (EWMA param) = 0.9 for delay
                }
                else
                {
                    max_c = qp->hp.u;
                    new_rate = (0.7 * (qp->hp.m_curRate / max_c + DataRate("150Mbps")) +
                                0.3 * qp->hp.m_curRate); // gamma (EWMA param) = 0.7
                }
                if (new_rate < m_minRate)
                {
                    new_rate = m_minRate;
                }
                if (new_rate > qp->m_max_rate)
                {
                    new_rate = qp->m_max_rate;
                }
            }
            qp->prevRtt = prevRtt;
            qp->prevCompletion = Simulator::Now().GetNanoSeconds();
            if (updated_any)
            {
                ChangeRate(qp, new_rate);
            }
            if (!fast_react)
            {
                if (updated_any)
                {
                    qp->hp.m_curRate = new_rate;
                    qp->hp.m_incStage = new_incStage;
                }
            }
        }
        if (!fast_react)
        {
            if (next_seq > qp->hp.m_lastUpdateSeq)
            {
                qp->hp.m_lastUpdateSeq = next_seq;
            }
        }
    }
}

void
RdmaHw::FastReactPower(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    if (m_fast_react)
    {
        UpdateRatePower(qp, p, ch, true);
    }
}

/**********************
 * TIMELY
 *********************/
void
RdmaHw::HandleAckTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    uint32_t ack_seq = ch.ack.seq;
    // update rate
    if (ack_seq > qp->tmly.m_lastUpdateSeq)
    { // if full RTT feedback is ready, do full update
        UpdateRateTimely(qp, p, ch, false);
    }
    else
    { // do fast react
        FastReactTimely(qp, p, ch);
    }
}

void
RdmaHw::UpdateRateTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool us) const
{
    uint32_t next_seq = qp->snd_nxt;
    uint64_t rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.ts;
    bool print = !us;
    if (qp->tmly.m_lastUpdateSeq != 0)
    { // not first RTT
        int64_t new_rtt_diff = (int64_t)rtt - (int64_t)qp->tmly.lastRtt;
        double rtt_diff = (1 - m_tmly_alpha) * qp->tmly.rttDiff + m_tmly_alpha * new_rtt_diff;
        double gradient = rtt_diff / m_tmly_minRtt;
        bool inc = false;
        double c = 0;
#if PRINT_LOG
        if (print)
            printf("%lu node:%u rtt:%lu rttDiff:%.0lf gradient:%.3lf rate:%.3lf",
                   Simulator::Now().GetTimeStep(),
                   m_node->GetId(),
                   rtt,
                   rtt_diff,
                   gradient,
                   qp->tmly.m_curRate.GetBitRate() * 1e-9);
#endif
        if (rtt < m_tmly_TLow)
        {
            inc = true;
        }
        else if (rtt > m_tmly_THigh)
        {
            c = 1 - m_tmly_beta * (1 - (double)m_tmly_THigh / rtt);
            inc = false;
        }
        else if (gradient <= 0)
        {
            inc = true;
        }
        else
        {
            c = 1 - m_tmly_beta * gradient;
            if (c < 0)
            {
                c = 0;
            }
            inc = false;
        }
        if (inc)
        {
            if (qp->tmly.m_incStage < 5)
            // WTF does this mean?
            {
                qp->m_rate = qp->tmly.m_curRate + m_rai;
            }
            else
            {
                qp->m_rate = qp->tmly.m_curRate + m_rhai;
            }
            if (qp->m_rate > qp->m_max_rate)
            {
                qp->m_rate = qp->m_max_rate;
            }
            if (!us)
            {
                qp->tmly.m_curRate = qp->m_rate;
                qp->tmly.m_incStage++;
                qp->tmly.rttDiff = rtt_diff;
            }
        }
        else
        {
            qp->m_rate = std::max(m_minRate, qp->tmly.m_curRate * c);
            if (!us)
            {
                qp->tmly.m_curRate = qp->m_rate;
                qp->tmly.m_incStage = 0;
                qp->tmly.rttDiff = rtt_diff;
            }
        }
#if PRINT_LOG
        if (print)
        {
            printf(" %c %.3lf\n", inc ? '^' : 'v', qp->m_rate.GetBitRate() * 1e-9);
        }
#endif
    }
    if (!us && next_seq > qp->tmly.m_lastUpdateSeq)
    {
        qp->tmly.m_lastUpdateSeq = next_seq;
        // update
        qp->tmly.lastRtt = rtt;
    }
}

void
RdmaHw::FastReactTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
}

/**********************
 * Patched TIMELY
 *********************/

void
RdmaHw::HandleAckPatchedTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    uint32_t ack_seq = ch.ack.seq;
    // update rate
    if (ack_seq > qp->tmly.m_lastUpdateSeq)
    { // if full RTT feedback is ready, do full update
        UpdateRatePatchedTimely(qp, p, ch, false);
    }
    else
    { // do fast react
        FastReactPatchedTimely(qp, p, ch);
    }
}

void
RdmaHw::UpdateRatePatchedTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool us)
{
    uint32_t next_seq = qp->snd_nxt;
    uint64_t currTime = Simulator::Now().GetNanoSeconds();
    uint64_t rtt = currTime - ch.ack.ih.ts;
    bool print = !us;
    if (qp->tmly.m_lastUpdateSeq != 0)
    { // not first RTT
        int64_t new_rtt_diff = (int64_t)rtt - (int64_t)qp->tmly.lastRtt;
        qp->tmly.lastRtt = rtt;
        double rtt_diff = (1 - m_tmly_alpha) * qp->tmly.rttDiff + m_tmly_alpha * new_rtt_diff;
        double gradient = rtt_diff / m_tmly_minRtt;
        double weight;
        DataRate new_rate;
        double error;
#if PRINT_LOG
        if (print)
            printf("%lu node:%u rtt:%lu rttDiff:%.0lf gradient:%.3lf rate:%.3lf",
                   Simulator::Now().GetTimeStep(),
                   m_node->GetId(),
                   rtt,
                   rtt_diff,
                   gradient,
                   qp->tmly.m_curRate.GetBitRate() * 1e-9);
#endif
        if (rtt < m_tmly_TLow) // newRTT < Tlow
        {
            // rate = rate + rai
            new_rate = qp->tmly.m_curRate + m_rai;
        }
        else if (rtt > m_tmly_THigh) // newRTT > Thigh
        {
            //  rate = rate * (1 - beta(1 - Thigh / new_rtt))
            new_rate = qp->tmly.m_curRate * (1 - m_tmly_beta * (1 - (double)m_tmly_THigh / rtt));
        }
        else
        {
            // weight = w(rttGradient)
            if (gradient <= -0.25)
            {
                weight = 0.0;
            }
            else if (gradient >= 0.25)
            {
                weight = 1.0;
            }
            else
            {
                weight = 2.0 * gradient + 0.5;
            }
            error = ((double)rtt - (double)m_ptmly_RttRef) / m_ptmly_RttRef;
            new_rate =
                m_rai * (1 - weight) + qp->tmly.m_curRate * (1 - m_ptmly_beta * error * weight);
        }
        qp->tmly.m_curRate = std::max(m_minRate, std::min(qp->m_max_rate, new_rate));
        ChangeRate(qp, new_rate);
        qp->tmly.rttDiff = rtt_diff;
#if PRINT_LOG
        if (print)
        {
            printf(" %c %.3lf\n", inc ? '^' : 'v', qp->m_rate.GetBitRate() * 1e-9);
        }
#endif
    }
    if (!us && next_seq > qp->tmly.m_lastUpdateSeq)
    {
        qp->tmly.m_lastUpdateSeq = next_seq;
        // update
        qp->tmly.lastRtt = rtt;
    }
    std::cout << "[PTMLY] node:" << m_node->GetId() << " rate:" << qp->m_rate << " RTT:" << rtt
              << std::endl;
}

void
RdmaHw::FastReactPatchedTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
}

/**********************
 * DCTCP
 *********************/
void
RdmaHw::HandleAckDctcp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch) const
{
    uint32_t ack_seq = ch.ack.seq;
    uint8_t cnp = (ch.ack.flags >> qbbHeader::FLAG_CNP) & 1;
    bool new_batch = false;

    // update alpha
    qp->dctcp.m_ecnCnt += (cnp > 0);
    if (ack_seq > qp->dctcp.m_lastUpdateSeq)
    { // if full RTT feedback is ready, do alpha update
#if PRINT_LOG
        printf("%lu %s %08x %08x %u %u [%u,%u,%u] %.3lf->",
               Simulator::Now().GetTimeStep(),
               "alpha",
               qp->sip.Get(),
               qp->dip.Get(),
               qp->sport,
               qp->dport,
               qp->dctcp.m_lastUpdateSeq,
               ch.ack.seq,
               qp->snd_nxt,
               qp->dctcp.m_alpha);
#endif
        new_batch = true;
        if (qp->dctcp.m_lastUpdateSeq == 0)
        { // first RTT
            qp->dctcp.m_lastUpdateSeq = qp->snd_nxt;
            qp->dctcp.m_batchSizeOfAlpha = qp->snd_nxt / m_mtu + 1;
        }
        else
        {
            double frac = std::min(1.0, double(qp->dctcp.m_ecnCnt) / qp->dctcp.m_batchSizeOfAlpha);
            qp->dctcp.m_alpha = (1 - m_g) * qp->dctcp.m_alpha + m_g * frac;
            qp->dctcp.m_lastUpdateSeq = qp->snd_nxt;
            qp->dctcp.m_ecnCnt = 0;
            qp->dctcp.m_batchSizeOfAlpha = (qp->snd_nxt - ack_seq) / m_mtu + 1;
#if PRINT_LOG
            printf("%.3lf F:%.3lf", qp->dctcp.m_alpha, frac);
#endif
        }
#if PRINT_LOG
        printf("\n");
#endif
    }

    // check cwr exit
    if (qp->dctcp.m_caState == 1)
    {
        if (ack_seq > qp->dctcp.m_highSeq)
        {
            qp->dctcp.m_caState = 0;
        }
    }

    // check if need to reduce rate: ECN and not in CWR
    if (cnp && qp->dctcp.m_caState == 0)
    {
#if PRINT_LOG
        printf("%lu %s %08x %08x %u %u %.3lf->",
               Simulator::Now().GetTimeStep(),
               "rate",
               qp->sip.Get(),
               qp->dip.Get(),
               qp->sport,
               qp->dport,
               qp->m_rate.GetBitRate() * 1e-9);
#endif
        qp->m_rate = std::max(m_minRate, qp->m_rate * (1 - qp->dctcp.m_alpha / 2));
#if PRINT_LOG
        printf("%.3lf\n", qp->m_rate.GetBitRate() * 1e-9);
#endif
        qp->dctcp.m_caState = 1;
        qp->dctcp.m_highSeq = qp->snd_nxt;
    }

    // additive inc
    if (qp->dctcp.m_caState == 0 && new_batch)
    {
        qp->m_rate = std::min(qp->m_max_rate, qp->m_rate + m_dctcp_rai);
    }
}

/*********************
 * HPCC-PINT
 ********************/
void
RdmaHw::SetPintSmplThresh(double p)
{
    pint_smpl_thresh = (uint32_t)(65536 * p);
}

void
RdmaHw::HandleAckHpPint(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    uint32_t ack_seq = ch.ack.seq;
    if (rand() % 65536 >= pint_smpl_thresh)
    {
        return;
    }
    // update rate
    if (ack_seq > qp->hpccPint.m_lastUpdateSeq)
    { // if full RTT feedback is ready, do full update
        UpdateRateHpPint(qp, p, ch, false);
    }
    else
    { // do fast react
        UpdateRateHpPint(qp, p, ch, true);
    }
}

void
RdmaHw::UpdateRateHpPint(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool fast_react)
{
    uint32_t next_seq = qp->snd_nxt;
    if (qp->hpccPint.m_lastUpdateSeq == 0)
    { // first RTT
        qp->hpccPint.m_lastUpdateSeq = next_seq;
    }
    else
    {
        // check packet INT
        IntHeader& ih = ch.ack.ih;
        double U = Pint::decode_u(ih.GetPower());

        DataRate new_rate;
        int32_t new_incStage;
        double max_c = U / m_targetUtil;

        if (max_c >= 1 || qp->hpccPint.m_incStage >= m_miThresh)
        {
            new_rate = qp->hpccPint.m_curRate / max_c + m_rai;
            new_incStage = 0;
        }
        else
        {
            new_rate = qp->hpccPint.m_curRate + m_rai;
            new_incStage = qp->hpccPint.m_incStage + 1;
        }
        if (new_rate < m_minRate)
        {
            new_rate = m_minRate;
        }
        if (new_rate > qp->m_max_rate)
        {
            new_rate = qp->m_max_rate;
        }
        ChangeRate(qp, new_rate);
        if (!fast_react)
        {
            qp->hpccPint.m_curRate = new_rate;
            qp->hpccPint.m_incStage = new_incStage;
        }
        if (!fast_react)
        {
            if (next_seq > qp->hpccPint.m_lastUpdateSeq)
            {
                qp->hpccPint.m_lastUpdateSeq = next_seq; //+ rand() % 2 * m_mtu;
            }
        }
    }
}

/*********************
 * Swift
 ********************/

void
RdmaHw::HandleAckSwift(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch) const
{
    auto ih = ch.ack.ih.swift;
    // std::cout << "[SWIFT] Hops: " << ih.nhop << ", Remote Delay: " << ih.remote_delay <<
    // std::endl;
    // uint32_t ack_seq = ch.ack.seq;
    auto rtt = Simulator::Now().GetNanoSeconds() - ih.ts;
    auto fabric_delay = rtt - ih.remote_delay;

    // on receiving ack
    qp->swift.m_retransmit_cnt = 0;
    auto target_fab_delay = TargetFabDelaySwift(qp, p, ch);
    auto fab_cwnd = GetCwndSwift(qp, p, ch, target_fab_delay, fabric_delay);
    auto endpoint_cwnd = GetCwndSwift(qp, p, ch, swift_target_endpoint_delay, ih.remote_delay);
    // cap max and min cwnd, and get the lower one in fab and endpoint
    auto cwnd =
        std::max(swift_min_cwnd, std::min(swift_max_cwnd, std::min(fab_cwnd, endpoint_cwnd)));
    if (cwnd < qp->m_win)
    {
        qp->swift.m_t_last_decrease = Simulator::Now();
    }
    if (cwnd < 1)
    {
        qp->swift.m_pacing_delay = rtt * 1.0 / cwnd;
        qp->SetWin(INT_MAX); // to make sure sending is only pacing-bound, but not window-bound
        qp->swift.m_real_win = cwnd;
        qp->UpdatePacing();
    }
    else
    {
        qp->swift.m_pacing_delay = 0;
        qp->SetWin((uint32_t)cwnd);
        qp->swift.m_real_win = cwnd;
    }
    std::cout << "[SWIFT] node: " << m_node->GetId() << ", cwnd: " << cwnd
              << ", delay: " << fabric_delay << std::endl;
}

// calculate target fabric delay
uint64_t
RdmaHw::TargetFabDelaySwift(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch) const
{
    double fcwnd = qp->swift.m_real_win;
    auto num_hops = ch.ack.ih.swift.nhop;
    auto alpha =
        swift_fs_range / (std::pow(swift_fs_min_cwnd, -0.5) - std::pow(swift_fs_max_cwnd, -0.5));
    auto beta = -1 * alpha / std::sqrt(swift_fs_max_cwnd);
    auto t = swift_base_target + num_hops * swift_hop_scale +
             std::max(0.0, std::min(swift_fs_range, alpha * std::pow(fcwnd, -0.5) + beta));
    return t;
}

// calculate cwnd for fabric/endpoint
double
RdmaHw::GetCwndSwift(Ptr<RdmaQueuePair> qp,
                     Ptr<Packet> p,
                     CustomHeader& ch,
                     uint64_t target_delay,
                     uint64_t curr_delay) const
{
    bool canDecrease = ch.ack.ih.swift.ts > qp->swift.m_t_last_decrease.GetNanoSeconds();
    double cwnd = qp->swift.m_real_win;
    if (curr_delay < target_delay)
    {
        if (cwnd >= 1)
        {
            // num_acked is actually the number of packets IN EVERY ACK
            // so that we can assure incrementing approx. swift_ai per RTT
            cwnd = cwnd + (double)swift_ai * (m_mtu / cwnd);
        }
        else
        {
            cwnd = cwnd + swift_ai * m_mtu;
        }
    }
    else if (canDecrease)
    {
        cwnd =
            std::max(1 - swift_beta * (curr_delay - target_delay) / curr_delay, 1 - swift_max_mdf) *
            cwnd;
    }
    return cwnd;
}

void
RdmaHw::HandleAckRttQcn(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch) const
{
    uint64_t rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.GetTs();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distr(0, 1000);
    bool ecn = false;
    if (rtt <= rtt_qcn_tmin)
    {
        ecn = false;
    }
    else if (rtt <= rtt_qcn_tmax)
    {
        auto thresh = (rtt - rtt_qcn_tmin) * 1000.0 / (rtt_qcn_tmax - rtt_qcn_tmin);
        auto rand_num = distr(gen);
        ecn = rand_num < thresh;
    }
    else
    {
        ecn = true;
    }

    // window in mtu (1000), not in bytes / seq#
    auto cwnd = qp->rttqcn.curr_win;
    if (cwnd < m_mtu)
    {
        if (ecn)
        {
            cwnd *= 1 - rtt_qcn_beta;
        }
        else
        {
            cwnd += rtt_qcn_alpha * m_mtu;
        }
    }
    else
    {
        if (ecn)
        {
            cwnd -= 0.5 * m_mtu;
        }
        else
        {
            // attempt to improve: multiply by 10
            cwnd += m_mtu * 10.0 / cwnd;
        }
    }
    std::cout << "[RTT-QCN] node: " << m_node->GetId() << ", cwnd: " << qp->rttqcn.curr_win << "->"
              << cwnd << ", RTT: " << rtt << ", ecn: " << ecn << std::endl;
    qp->rttqcn.curr_win = cwnd;
    qp->m_win = (uint32_t)cwnd;
}

void
RdmaHw::HandleAckPowerQcn(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch)
{
    uint64_t rtt = Simulator::Now().GetTimeStep() - ch.ack.ih.GetTs();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distr(0, 1000);
    uint64_t prev_rtt = qp->powerqcn.prev_rtt == 0 ? rtt : qp->powerqcn.prev_rtt;
    if (qp->powerqcn.prev_rtt < ch.ack.ih.GetTs())
    {
        qp->powerqcn.prev_rtt = rtt;
        qp->powerqcn.last_update = Simulator::Now().GetTimeStep();
    }
    double rtt_gradient = (rtt - prev_rtt) / rtt_qcn_tmin;

    bool rtt_ecn = false;
    if (rtt <= rtt_qcn_tmin)
    {
        rtt_ecn = false;
    }
    else if (rtt <= rtt_qcn_tmax)
    {
        auto thresh = (rtt - rtt_qcn_tmin) * 1000.0 / (rtt_qcn_tmax - rtt_qcn_tmin);
        auto rand_num = distr(gen);
        rtt_ecn = rand_num < thresh;
    }
    else
    {
        rtt_ecn = true;
    }

    bool gradient_ecn = false;
    if (rtt_gradient <= powerqcn_grad_min)
    {
        gradient_ecn = false;
    }
    else if (rtt_gradient <= powerqcn_grad_max)
    {
        auto thresh =
            (rtt_gradient - powerqcn_grad_min) * 1000.0 / (powerqcn_grad_max - powerqcn_grad_min);
        auto rand_num = distr(gen);
        gradient_ecn = rand_num < thresh;
    }
    else
    {
        gradient_ecn = true;
    }

    // window in mtu (1000), not in bytes / seq#
    auto cwnd = qp->rttqcn.curr_win;
    if (cwnd < m_mtu)
    {
        if (rtt_ecn)
        {
            cwnd *= 1 - rtt_qcn_beta;
        }
        else
        {
            cwnd += rtt_qcn_alpha * m_mtu;
        }
    }
    else
    {
        if (rtt_ecn)
        {
            // cwnd -= 0.5 * m_mtu;
            if (gradient_ecn)
            {
                cwnd -= 0.7 * m_mtu;
            }
            else
            {
                cwnd -= 0.5 * m_mtu;
            }
        }
        if (!rtt_ecn)
        {
            // cwnd += m_mtu * 10.0 / cwnd;
            if (gradient_ecn)
            {
                cwnd += m_mtu * 8.0 / cwnd;
            }
            else
            {
                cwnd += m_mtu * 20.0 / cwnd;
            }
        }
    }
    std::cout << "[RTT-QCN] node: " << m_node->GetId() << ", cwnd: " << qp->rttqcn.curr_win << "->"
              << cwnd << ", RTT: " << rtt << ", ecn: " << rtt_ecn << gradient_ecn << std::endl;
    qp->rttqcn.curr_win = cwnd;
    qp->m_win = (uint32_t)cwnd;
}

/******************************************************************************
 * Selective Retransmission (SR) - hardware/software bitmap implementation
 *
 * Concepts (mapped to existing fields):
 *   eACK   <-> qp->snd_una      (sender's next-expected ack, in bytes)
 *   ePSN   <-> q->ReceiverNextExpectedSeq  (receiver's next-expected seq, in bytes)
 *
 * The bitmap is indexed in MTU-sized packets. PSN/MTU is a packet index.
 *
 * Hardware bitmap (size K) anchored at eACK/ePSN; software bitmap (variable)
 * anchored at soft_eACK/soft_ePSN. The two bitmaps describe disjoint windows;
 * between them is a "compressed" range that is implicitly all-ones.
 *
 * The implementation simulates the async behaviour of a real NIC by serving the
 * software request synchronously inside the helper - the algorithm itself does
 * not depend on whether the response is delayed, only on the order of state
 * updates.  Tracking sendLeft/sendRight (recvLeft/recvRight) lets us merge late
 * arrivals into the post-update bitmap as the spec requires.
 *****************************************************************************/

/* --- receiver helpers ----------------------------------------------------
 *
 * Layout (anchored on hwAnchor):
 *
 *   [hwAnchor, hwAnchor+K*MTU)        : hardware bitmap (K bits)
 *   [hwAnchor+K*MTU, soft_ePSN)       : compressed region (implicitly all-1)
 *   [soft_ePSN, ...)                  : software bitmap (variable size)
 *
 * Invariants:
 *   - hwAnchor is FROZEN; it only ever changes at "promote" time.
 *   - ePSN is the next expected sequence number; it advances inside
 *     [hwAnchor, hwAnchor+K*MTU] tracking the first 0 in hwBitMap.
 *   - swBitMap is anchored at soft_ePSN: swBitMap[i] => PSN soft_ePSN+i*MTU.
 *   - Promote (the only event that moves hwAnchor) fires when the entire
 *     hardware bitmap becomes all 1s. It slides the bitmap to soft_ePSN, then
 *     asks the software side to merge swBitMap back in and locate the next
 *     real hole. During the async window swPending=true and hwAnchor stays put.
 */

void
RdmaHw::SrInitRxIfNeeded(Ptr<RdmaRxQueuePair> q)
{
    if (q->sr.hwBitMap.size() != m_srK)
    {
        q->sr.hwBitMap.assign(m_srK, 0);
        q->sr.hwAnchor  = q->ReceiverNextExpectedSeq;
        q->sr.soft_ePSN = q->ReceiverNextExpectedSeq + m_srK * m_mtu;
    }
}

// Walk ePSN forward through consecutive 1s starting at its current bit. The
// hardware bitmap is NOT shifted - bits cleared as we walk represent the
// information that ePSN now carries. If the walk reaches the K boundary the
// whole window is full of 1s and we trigger a promote (unless one is already
// in flight, in which case we just leave ePSN at the boundary and let the sw
// fire path complete the promote).
void
RdmaHw::SrRxAdvanceEPSN(Ptr<RdmaRxQueuePair> q)
{
    uint32_t idx = (q->ReceiverNextExpectedSeq - q->sr.hwAnchor) / m_mtu;
    while (idx < m_srK && q->sr.hwBitMap[idx] == 1)
    {
        q->sr.hwBitMap[idx] = 0;
        q->ReceiverNextExpectedSeq += m_mtu;
        idx++;
    }
    if (idx < m_srK || q->sr.swPending)
    {
        return; // not full yet, or another promote is already in flight
    }

    // Promote: hardware window slides as a whole to soft_ePSN. The compressed
    // range [hwAnchor+K, soft_ePSN) is implicitly all-1, which is what tells
    // us the next gap is at soft_ePSN. The K-bit data that lives in
    // swBitMap[0..K-1] (covering [oldSoft, oldSoft+K)) does NOT travel back
    // to the NIC synchronously - in real hardware that K-bit transfer is
    // bundled with the sw-update PCIe response. So during swPending the
    // hardware bitmap is "freshly cleared" and only retransmit/late OOO
    // arrivals fill it; the OR-merge with swBitMap[0..K-1] happens at
    // SrRxApplySwUpdate time. swBitMap stays anchored at oldSoft until the
    // merge completes.
    uint32_t oldSoft = q->sr.soft_ePSN;
    q->sr.hwAnchor = oldSoft;
    q->ReceiverNextExpectedSeq = oldSoft;
    for (uint32_t i = 0; i < m_srK; i++) { q->sr.hwBitMap[i] = 0; }
    q->sr.soft_ePSN = oldSoft + m_srK * m_mtu;     // placeholder; sw fire refines

    // Always schedule the async refinement when swBitMap is non-empty (it
    // holds both the K-bit window data and any beyond-window OOO data).
    // Empty swBitMap => silent slide, no PCIe cost.
    if (!q->sr.swBitMap.empty())
    {
        q->sr.recvLeft  = q->sr.receiveHigh;
        q->sr.recvRight = q->sr.receiveHigh;
        SrRxRequestSwUpdate(q);
    }
}

// Two-phase async simulation: SrRxRequestSwUpdate sets swPending and schedules
// SrRxApplySwUpdate `m_rallAsyncNs` later.
void
RdmaHw::SrRxRequestSwUpdate(Ptr<RdmaRxQueuePair> q)
{
    if (q->sr.swPending)
    {
        return;
    }
    q->sr.swPending = true;
    q->sr.swDeadlineNs = Simulator::Now().GetNanoSeconds() + m_rallAsyncNs;
    if (m_rallAsyncNs == 0)
    {
        SrRxApplySwUpdate(q);
    }
    else
    {
        Simulator::Schedule(NanoSeconds(m_rallAsyncNs),
                            &RdmaHw::SrRxApplySwUpdate, this, q);
    }
}

// Async sw worker. This is what the PCIe response delivers back to the NIC:
//   (1) the K-bit slice covering [hwAnchor, hwAnchor+K) so it can OR-merge
//       into the (partially-filled-by-retx) hwBitMap;
//   (2) the refined soft_ePSN, located by scanning swBitMap[K..] for the
//       first real 0.
//
// Invariants on entry:
//   hwAnchor frozen since promote.
//   swBitMap is anchored at hwAnchor (i.e., swBitMap[i] => PSN hwAnchor+i*MTU).
//   swBitMap[0..K-1] is the "K-bit window data" to merge into hw.
//   hwBitMap may have been partially filled by retransmits / late OOO during
//   the async window.
//   soft_ePSN may have grown past hwAnchor+K via compressed-slot races.
void
RdmaHw::SrRxApplySwUpdate(Ptr<RdmaRxQueuePair> q)
{
    // Step 1: PCIe response delivers the K-bit window data; OR into hw.
    size_t pull = std::min((size_t)m_srK, q->sr.swBitMap.size());
    for (size_t i = 0; i < pull; i++)
    {
        if (q->sr.swBitMap[i] == 1)
        {
            q->sr.hwBitMap[i] = 1;
        }
    }
    if (pull > 0)
    {
        q->sr.swBitMap.erase(q->sr.swBitMap.begin(),
                             q->sr.swBitMap.begin() + pull);
    }
    // swBitMap is now anchored at hwAnchor + K*MTU.

    // Step 2: reconcile compressed-slot growth that raced this async window.
    // soft_ePSN may already point past hwAnchor+K; the corresponding leading
    // entries in swBitMap (now post-K erase) describe PSNs in the new
    // compressed range and should be discarded.
    uint32_t base = q->sr.hwAnchor + m_srK * m_mtu;
    if (q->sr.soft_ePSN > base)
    {
        size_t consumed = (q->sr.soft_ePSN - base) / m_mtu;
        size_t toErase = std::min(consumed, q->sr.swBitMap.size());
        if (toErase > 0)
        {
            q->sr.swBitMap.erase(q->sr.swBitMap.begin(),
                                 q->sr.swBitMap.begin() + toErase);
        }
    }

    // Step 3: scan leading 1s; each is an implicitly-received PSN, advance
    // soft_ePSN.
    size_t trim = 0;
    while (trim < q->sr.swBitMap.size() && q->sr.swBitMap[trim] == 1)
    {
        trim++;
    }
    if (trim > 0)
    {
        q->sr.soft_ePSN += trim * m_mtu;
        q->sr.swBitMap.erase(q->sr.swBitMap.begin(),
                             q->sr.swBitMap.begin() + trim);
    }

    q->sr.swPending = false;

    // Step 4: walk ePSN through any newly-merged 1s. Note that the K-bit
    // pull may itself have made hwBitMap all 1, triggering the next promote.
    SrRxAdvanceEPSN(q);

    if (q->ReceiverNextExpectedSeq >= q->sr.receiveHigh + m_mtu)
    {
        q->sr.ReceiveOutOfOrder = false;
        q->test.recoveryState = 0;
    }
}

// Main entry: handle one received packet on the SR receiver path.
//
// Region dispatch (using saved oldHigh = receiveHigh BEFORE this packet):
//   seq < ePSN                              -> duplicate, drop
//   seq in [hwAnchor, hwAnchor+K*MTU)       -> mark hwBitMap[idx]=1, walk
//   seq in [hwAnchor+K*MTU, soft_ePSN)      -> compressed (already covered)
//   seq == soft_ePSN && seq >= oldHigh      -> compressed-slot grow: soft_ePSN++
//   seq > soft_ePSN                         -> swBitMap[(seq-soft_ePSN)/MTU]=1
void
RdmaHw::SrRxHandlePacket(Ptr<RdmaRxQueuePair> q, uint32_t seq)
{
    SrInitRxIfNeeded(q);

    if (seq < q->ReceiverNextExpectedSeq)
    {
        return; // duplicate
    }

    // State A (pure in-order, no recovery): hw bitmap is "not in use".
    // Just bump ePSN; do not touch hwAnchor / hwBitMap / soft_ePSN.
    if (seq == q->ReceiverNextExpectedSeq && !q->sr.ReceiveOutOfOrder)
    {
        q->ReceiverNextExpectedSeq += m_mtu;
        if (q->sr.receiveHigh < seq)
        {
            q->sr.receiveHigh = seq;
        }
        if (q->test.sack_high < seq)
        {
            q->test.sack_high = seq;
        }
        return;
    }

    // First-OOO transition (A -> B): open the hardware bitmap NOW. hwAnchor
    // is locked at the current ePSN so all bit indices stay consistent for
    // the rest of this recovery window.
    if (!q->sr.ReceiveOutOfOrder)
    {
        for (uint32_t i = 0; i < m_srK; i++) { q->sr.hwBitMap[i] = 0; }
        q->sr.swBitMap.clear();
        q->sr.hwAnchor  = q->ReceiverNextExpectedSeq;
        q->sr.soft_ePSN = q->ReceiverNextExpectedSeq + m_srK * m_mtu;
    }

    // Update the receiveHigh tracker.
    if (seq > q->sr.receiveHigh)
    {
        if (seq == q->sr.recvRight + m_mtu)
        {
            q->sr.recvRight = seq;
        }
        else if (seq > q->sr.receiveHigh + m_mtu)
        {
            q->sr.recvLeft  = seq;
            q->sr.recvRight = seq;
        }
        q->sr.receiveHigh = seq;
    }
    if (q->test.sack_high < seq)
    {
        q->test.sack_high = seq;
    }

    uint32_t hwLimit = q->sr.hwAnchor + m_srK * m_mtu;

    if (seq < hwLimit)
    {
        // In hardware window. Mark the bit and walk if this filled ePSN's hole.
        uint32_t idx = (seq - q->sr.hwAnchor) / m_mtu;
        q->sr.hwBitMap[idx] = 1;
        if (seq != q->ReceiverNextExpectedSeq)
        {
            q->sr.ReceiveOutOfOrder = true;
            q->test.recoveryState = 1;
        }
        SrRxAdvanceEPSN(q);
    }
    else if (seq < q->sr.soft_ePSN)
    {
        // Compressed range: already implicitly received. Drop as duplicate.
        return;
    }
    else if (seq == q->sr.soft_ePSN)
    {
        // Boundary fill: extend the implicit-1 region by one slot. Chase
        // through any leading 1s in swBitMap is only valid when swBitMap is
        // anchored at soft_ePSN (i.e., not during swPending where it stays
        // anchored at hwAnchor for the K-bit-merge to happen).
        q->sr.soft_ePSN += m_mtu;
        if (!q->sr.swPending)
        {
            while (!q->sr.swBitMap.empty() && q->sr.swBitMap.front() == 1)
            {
                q->sr.soft_ePSN += m_mtu;
                q->sr.swBitMap.erase(q->sr.swBitMap.begin());
            }
        }
    }
    else
    {
        // seq > soft_ePSN: swBitMap region. swBitMap anchor depends on phase:
        // - normal: anchored at soft_ePSN
        // - swPending: anchored at hwAnchor (= old soft_ePSN frozen at promote)
        uint32_t swAnchor = q->sr.swPending ? q->sr.hwAnchor : q->sr.soft_ePSN;
        uint32_t idx = (seq - swAnchor) / m_mtu;
        if (idx >= q->sr.swBitMap.size())
        {
            q->sr.swBitMap.resize(idx + 1, 0);
        }
        q->sr.swBitMap[idx] = 1;
        q->sr.ReceiveOutOfOrder = true;
        q->test.recoveryState = 1;
    }

    if (q->ReceiverNextExpectedSeq >= q->sr.receiveHigh + m_mtu)
    {
        q->sr.ReceiveOutOfOrder = false;
        q->test.recoveryState = 0;
    }
}

/* --- sender helpers ------------------------------------------------------
 *
 * Mirror of the receiver layout (substituting eACK for ePSN, sackHigh for
 * receiveHigh, soft_eACK for soft_ePSN). hwAnchor is the frozen anchor that
 * only moves at promote time. snd_una plays ePSN's role and walks through 1s
 * inside the K-window without shifting the bitmap.
 */

void
RdmaHw::SrInitTxIfNeeded(Ptr<RdmaQueuePair> qp)
{
    if (qp->sr.hwBitMap.size() != m_srK)
    {
        qp->sr.hwBitMap.assign(m_srK, 0);
        qp->sr.hwAnchor  = qp->snd_una;
        qp->sr.soft_eACK = qp->snd_una + m_srK * m_mtu;
    }
}

// Walk snd_una through consecutive 1s in hwBitMap. No shift. If the window
// becomes all 1s, promote: slide hwAnchor to soft_eACK and synchronously pull
// swBitMap[0..K-1] into hwBitMap. The async sw work that follows only refines
// soft_eACK by trimming further leading 1s in swBitMap.
void
RdmaHw::SrTxAdvanceEACK(Ptr<RdmaQueuePair> qp)
{
    uint32_t idx = (qp->snd_una - qp->sr.hwAnchor) / m_mtu;
    while (idx < m_srK && qp->sr.hwBitMap[idx] == 1)
    {
        qp->sr.hwBitMap[idx] = 0;
        qp->Acknowledge(qp->snd_una + m_mtu);
        idx++;
    }
    if (idx < m_srK || qp->sr.swPending)
    {
        return;
    }

    uint32_t oldSoft = qp->sr.soft_eACK;
    qp->sr.hwAnchor = oldSoft;
    qp->Acknowledge(oldSoft);    // fast-forward snd_una to the slide point
    for (uint32_t i = 0; i < m_srK; i++) { qp->sr.hwBitMap[i] = 0; }
    qp->sr.soft_eACK = oldSoft + m_srK * m_mtu;

    if (!qp->sr.swBitMap.empty())
    {
        qp->sr.sendLeft  = qp->sr.sackHigh;
        qp->sr.sendRight = qp->sr.sackHigh;
        SrTxRequestSwUpdate(qp);
    }
}

void
RdmaHw::SrTxRequestSwUpdate(Ptr<RdmaQueuePair> qp)
{
    if (qp->sr.swPending)
    {
        return;
    }
    qp->sr.swPending = true;
    qp->sr.swDeadlineNs = Simulator::Now().GetNanoSeconds() + m_rallAsyncNs;
    if (m_rallAsyncNs == 0)
    {
        SrTxApplySwUpdate(qp);
    }
    else
    {
        Simulator::Schedule(NanoSeconds(m_rallAsyncNs),
                            &RdmaHw::SrTxApplySwUpdate, this, qp);
    }
}

void
RdmaHw::SrTxApplySwUpdate(Ptr<RdmaQueuePair> qp)
{
    // Step 1: PCIe response delivers K-bit window data; OR into hwBitMap.
    size_t pull = std::min((size_t)m_srK, qp->sr.swBitMap.size());
    for (size_t i = 0; i < pull; i++)
    {
        if (qp->sr.swBitMap[i] == 1)
        {
            qp->sr.hwBitMap[i] = 1;
        }
    }
    if (pull > 0)
    {
        qp->sr.swBitMap.erase(qp->sr.swBitMap.begin(),
                              qp->sr.swBitMap.begin() + pull);
    }

    // Step 2: reconcile compressed-slot growth.
    uint32_t base = qp->sr.hwAnchor + m_srK * m_mtu;
    if (qp->sr.soft_eACK > base)
    {
        size_t consumed = (qp->sr.soft_eACK - base) / m_mtu;
        size_t toErase = std::min(consumed, qp->sr.swBitMap.size());
        if (toErase > 0)
        {
            qp->sr.swBitMap.erase(qp->sr.swBitMap.begin(),
                                  qp->sr.swBitMap.begin() + toErase);
        }
    }

    // Step 3: trim leading 1s; advance soft_eACK.
    size_t trim = 0;
    while (trim < qp->sr.swBitMap.size() && qp->sr.swBitMap[trim] == 1)
    {
        trim++;
    }
    if (trim > 0)
    {
        qp->sr.soft_eACK += trim * m_mtu;
        qp->sr.swBitMap.erase(qp->sr.swBitMap.begin(),
                              qp->sr.swBitMap.begin() + trim);
    }

    qp->sr.swPending = false;

    // Step 4: walk snd_una through merged 1s; may chain into next promote.
    SrTxAdvanceEACK(qp);

    if (qp->snd_una > qp->sr.sackHigh)
    {
        qp->sr.SendOutOfOrder  = false;
        qp->test.recoveryState = 0;
        qp->sr.retransQueue.clear();
    }
    else if (!qp->sr.retransQueue.empty())
    {
        while (!qp->sr.retransQueue.empty() &&
               qp->sr.retransQueue.front() < qp->snd_una)
        {
            qp->sr.retransQueue.erase(qp->sr.retransQueue.begin());
        }
    }
    SrTxRecomputeSackedBytes(qp);
}

// FACK accounting: compute total SACKed bytes in [snd_una, snd_nxt) and store
// in qp->sr.sackedBytes. O(K + sw.size()). Layout per SR mode:
//   mode 1 (RaLL): hwBitMap (K bits at hwAnchor) + compressed range
//                  [hwAnchor+K*MTU, soft_eACK) + swBitMap.
//   mode 2 (SRNIC): swBitMap only (anchored at snd_una).
//   mode 3 (IRN):   hwBitMap only (K bits at snd_una).
// Inactive (no SR / not OutOfOrder) returns 0. Capped at OnTheFly so
// IsWinBound's effective in-flight never goes negative.
void
RdmaHw::SrTxRecomputeSackedBytes(Ptr<RdmaQueuePair> qp)
{
    if (!m_srEnabled || m_srMode == 0 || !qp->sr.SendOutOfOrder)
    {
        qp->sr.sackedBytes = 0;
        return;
    }
    uint64_t bytes = 0;
    for (size_t i = 0; i < qp->sr.hwBitMap.size(); i++)
    {
        if (qp->sr.hwBitMap[i] == 1)
        {
            bytes += m_mtu;
        }
    }
    if (m_srMode == 1)
    {
        uint32_t hwLimit = qp->sr.hwAnchor + m_srK * m_mtu;
        if (qp->sr.soft_eACK > hwLimit)
        {
            bytes += (qp->sr.soft_eACK - hwLimit);
        }
    }
    for (size_t i = 0; i < qp->sr.swBitMap.size(); i++)
    {
        if (qp->sr.swBitMap[i] == 1)
        {
            bytes += m_mtu;
        }
    }
    uint64_t onTheFly = qp->GetOnTheFly();
    qp->sr.sackedBytes = std::min(bytes, onTheFly);
}

void
RdmaHw::SrTxFastSyncFromEpsn(Ptr<RdmaQueuePair> qp, uint32_t epsn)
{
    if (epsn <= qp->snd_una)
    {
        return;
    }
    SrInitTxIfNeeded(qp);

    if (!qp->sr.SendOutOfOrder)
    {
        // Bare cumulative advance: there is no bitmap state to maintain
        // outside SR mode.
        qp->Acknowledge(epsn);
        return;
    }
    if (qp->sr.swPending)
    {
        // A regular promote merge is in flight; touching swBitMap/hwAnchor
        // here races with SrTxApplySwUpdate. Skip and let the regular path
        // catch up.
        return;
    }

    uint32_t hwLimit = qp->sr.hwAnchor + m_srK * m_mtu;

    // Cases 1 & 2: epsn within hw window or compressed range.
    // Just like a normal cumulative ACK reaching epsn — fill the affected
    // hwBitMap bits and walk. Promote chains automatically when the K
    // window becomes all 1s.
    if (epsn <= qp->sr.soft_eACK)
    {
        uint32_t fillEnd = std::min(epsn, hwLimit);
        for (uint32_t s = qp->snd_una; s < fillEnd; s += m_mtu)
        {
            uint32_t idx = (s - qp->sr.hwAnchor) / m_mtu;
            if (idx < m_srK)
            {
                qp->sr.hwBitMap[idx] = 1;
            }
        }
        SrTxAdvanceEACK(qp);
        return;
    }

    // Case 3: epsn > soft_eACK. Hardware jump-ahead.
    //   - hwAnchor → epsn
    //   - soft_eACK → epsn + K*MTU (tentative; refined by leading-1 scan)
    //   - new hwBitMap[i] = old swBitMap[(epsn-oldSoftEAck)/MTU + i]
    //   - swBitMap trim: drop entries < (epsn + K*MTU)
    //   - extend soft_eACK through any consecutive leading 1s in trimmed sw
    //
    // !swPending here, so swBitMap is anchored at oldSoftEAck.
    uint32_t oldSwAnchor = qp->sr.soft_eACK;
    uint32_t newHwAnchor = epsn;
    uint32_t newSoftEAck = epsn + m_srK * m_mtu;

    // Build new hwBitMap from old swBitMap[hwSrcOff .. hwSrcOff + K - 1]
    int64_t hwSrcOff = (int64_t)((newHwAnchor - oldSwAnchor) / m_mtu);
    for (uint32_t i = 0; i < m_srK; i++)
    {
        int64_t srcOff = hwSrcOff + (int64_t)i;
        if (srcOff >= 0 && (size_t)srcOff < qp->sr.swBitMap.size() &&
            qp->sr.swBitMap[(size_t)srcOff] == 1)
        {
            qp->sr.hwBitMap[i] = 1;
        }
        else
        {
            qp->sr.hwBitMap[i] = 0;
        }
    }

    // Trim swBitMap up through (epsn + K*MTU). After this, swBitMap is
    // anchored at newSoftEAck.
    size_t totalTrim = (size_t)((newSoftEAck - oldSwAnchor) / m_mtu);
    if (totalTrim >= qp->sr.swBitMap.size())
    {
        qp->sr.swBitMap.clear();
    }
    else
    {
        qp->sr.swBitMap.erase(qp->sr.swBitMap.begin(),
                              qp->sr.swBitMap.begin() + totalTrim);
    }

    // Extend newSoftEAck past any leading 1s in (now anchored at) swBitMap.
    size_t lead = 0;
    while (lead < qp->sr.swBitMap.size() && qp->sr.swBitMap[lead] == 1)
    {
        lead++;
    }
    if (lead > 0)
    {
        newSoftEAck += lead * m_mtu;
        qp->sr.swBitMap.erase(qp->sr.swBitMap.begin(),
                              qp->sr.swBitMap.begin() + lead);
    }

    qp->sr.hwAnchor = newHwAnchor;
    qp->sr.soft_eACK = newSoftEAck;
    qp->Acknowledge(epsn);

    // Walk through any leading-1 bits in the freshly-populated hwBitMap;
    // may chain promote if all K are 1.
    SrTxAdvanceEACK(qp);
}

bool
RdmaHw::SrTxIsPsnAcked(Ptr<RdmaQueuePair> qp, uint32_t psn)
{
    if (psn < qp->snd_una)
    {
        return true;
    }
    if (m_srMode == 1)
    {
        // RaLL layout depends on phase. During swPending the K-window data
        // lives in swBitMap[0..K-1] (anchored at hwAnchor), and hwBitMap is
        // freshly cleared (only late OOO / retx fills it). In the normal
        // phase swBitMap is anchored at soft_eACK; the K-window data is in
        // hwBitMap.
        uint32_t hwLimit = qp->sr.hwAnchor + m_srK * m_mtu;
        if (qp->sr.swPending)
        {
            if (psn >= qp->sr.hwAnchor && psn < hwLimit)
            {
                uint32_t off = (psn - qp->sr.hwAnchor) / m_mtu;
                // Either source counts as SACKed. hwBitMap holds late retx
                // hits; swBitMap[0..K-1] holds the pre-promote K-merge data.
                if (off < qp->sr.hwBitMap.size() && qp->sr.hwBitMap[off] == 1)
                {
                    return true;
                }
                if (off < qp->sr.swBitMap.size() && qp->sr.swBitMap[off] == 1)
                {
                    return true;
                }
                return false;
            }
            if (psn >= hwLimit && psn < qp->sr.soft_eACK)
            {
                return true; // compressed range
            }
            if (psn >= qp->sr.soft_eACK)
            {
                uint32_t off = (psn - qp->sr.hwAnchor) / m_mtu;
                return off < qp->sr.swBitMap.size() &&
                       qp->sr.swBitMap[off] == 1;
            }
            return false;
        }
        // Normal phase
        if (psn >= qp->sr.hwAnchor && psn < hwLimit)
        {
            uint32_t off = (psn - qp->sr.hwAnchor) / m_mtu;
            return off < qp->sr.hwBitMap.size() && qp->sr.hwBitMap[off] == 1;
        }
        if (psn >= hwLimit && psn < qp->sr.soft_eACK)
        {
            return true; // RaLL compressed range = already SACKed
        }
        if (psn >= qp->sr.soft_eACK)
        {
            uint32_t off = (psn - qp->sr.soft_eACK) / m_mtu;
            return off < qp->sr.swBitMap.size() &&
                   qp->sr.swBitMap[off] == 1;
        }
        return false;
    }
    if (m_srMode == 3)
    {
        uint32_t hwLimit = qp->snd_una + m_srK * m_mtu;
        if (psn >= qp->snd_una && psn < hwLimit)
        {
            uint32_t off = (psn - qp->snd_una) / m_mtu;
            return off < qp->sr.hwBitMap.size() && qp->sr.hwBitMap[off] == 1;
        }
        return false;
    }
    if (m_srMode == 2)
    {
        // SRNIC: swBitMap anchored at snd_una
        if (psn >= qp->snd_una)
        {
            uint32_t off = (psn - qp->snd_una) / m_mtu;
            return off < qp->sr.swBitMap.size() && qp->sr.swBitMap[off] == 1;
        }
        return false;
    }
    return false;
}

void
RdmaHw::SrTxArmRTO(Ptr<RdmaQueuePair> qp)
{
    if (!m_srEnabled || m_srMode == 0)
    {
        return;
    }
    if (qp->sr.rtoTimer.IsRunning())
    {
        Simulator::Cancel(qp->sr.rtoTimer);
    }
    if (qp->IsFinished())
    {
        return;
    }
    if (qp->m_baseRtt == 0)
    {
        return; // can't pick an interval without baseRtt
    }
    // Generous RTO: 8x baseRTT, floored at 50us. Smaller values misfire under
    // pacing or moderate queueing where ACK gaps routinely exceed 2*baseRTT.
    uint64_t intervalNs = std::max<uint64_t>(50000ull, 8ull * qp->m_baseRtt);
    Time interval = NanoSeconds(intervalNs);
    qp->sr.rtoTimer = Simulator::Schedule(interval, &RdmaHw::SrTxOnRTOFire, this, qp);
}

void
RdmaHw::SrTxOnRTOFire(Ptr<RdmaQueuePair> qp)
{
    if (qp->IsFinished())
    {
        return;
    }
    if (qp->snd_una >= qp->snd_nxt)
    {
        // Nothing in flight; rearm and wait for the next send to start activity
        SrTxArmRTO(qp);
        return;
    }
    Time now = Simulator::Now();
    Time elapsed = now - qp->sr.lastAckTime;
    uint64_t thresholdNs = std::max<uint64_t>(50000ull, 8ull * qp->m_baseRtt);
    if (elapsed.GetNanoSeconds() < (int64_t)thresholdNs)
    {
        // ACK arrived between scheduling and firing. Reschedule so that we
        // fire `threshold` after the most recent lastAckTime, not `threshold`
        // from now (the latter would never fire under high ACK rate).
        int64_t remaining = (int64_t)thresholdNs - elapsed.GetNanoSeconds();
        if (remaining < 1000) { remaining = 1000; }
        qp->sr.rtoTimer = Simulator::Schedule(NanoSeconds(remaining),
                                              &RdmaHw::SrTxOnRTOFire, this, qp);
        return;
    }

    // Suppress retrans-storm in normal operation. We only enqueue when:
    //   (a) sender has entered SR mode (real loss detected via SACK), OR
    //   (b) sender is done transmitting but snd_una hasn't reached m_size
    //       (tail-loss: no later packet exists to trigger SACK).
    // If !SendOutOfOrder AND snd_nxt < m_size, the flow is still issuing
    // forward sends normally; a timer fire here only means ACKs are slow,
    // not that data is lost.
    bool tailLossSuspected = (qp->snd_nxt >= qp->m_size) &&
                             (qp->snd_una < qp->m_size);
    if (!qp->sr.SendOutOfOrder && !tailLossSuspected)
    {
        SrTxArmRTO(qp);
        return;
    }

    // Real timeout. Build a set of PSNs already queued for retransmit so we
    // don't enqueue duplicates.
    std::set<uint32_t> queued(qp->sr.retransQueue.begin(),
                              qp->sr.retransQueue.end());

    // Sweep [snd_una, snd_nxt) and enqueue any unacked PSN not already queued.
    uint32_t enqueued = 0;
    for (uint32_t psn = qp->snd_una; psn < qp->snd_nxt; psn += m_mtu)
    {
        if (SrTxIsPsnAcked(qp, psn))
        {
            continue;
        }
        if (queued.count(psn))
        {
            continue;
        }
        qp->sr.retransQueue.push_back(psn);
        queued.insert(psn);
        enqueued++;
    }

    if (enqueued > 0 && !qp->sr.SendOutOfOrder)
    {
        // Tail-loss / no-OOO scenario: we never entered SR mode, but RTO has
        // determined packets are lost. Flip into SR mode so GetNxtPacket
        // consumes retransQueue before forward sends.
        qp->sr.SendOutOfOrder = true;
        qp->test.recoveryState = 1;
    }

    qp->sr.lastAckTime = now;
    SrTxArmRTO(qp);

    // Wake up the device if we enqueued anything. Without this, the per-NIC
    // scheduler may be idle (no flow was eligible at last Dequeue) and won't
    // poll our newly-eligible flow until some other event triggers it.
    if (enqueued > 0)
    {
        uint32_t nic_idx = GetNicIdxOfQp(qp);
        m_nic[nic_idx].dev->TriggerTransmit();
    }
}

// Common region dispatch for ACK/SACK on the sender. Same shape as the
// receiver dispatch in SrRxHandlePacket.
static inline void
SrTxRecordBit(Ptr<RdmaQueuePair> qp, uint32_t seq, uint32_t mtu, uint32_t K)
{
    uint32_t hwLimit = qp->sr.hwAnchor + K * mtu;
    if (seq < qp->sr.hwAnchor)
    {
        return; // already covered
    }
    if (seq < hwLimit)
    {
        uint32_t idx = (seq - qp->sr.hwAnchor) / mtu;
        qp->sr.hwBitMap[idx] = 1;
        return;
    }
    if (seq < qp->sr.soft_eACK)
    {
        return; // compressed (already-acked) range
    }
    if (seq == qp->sr.soft_eACK)
    {
        qp->sr.soft_eACK += mtu;
        if (!qp->sr.swPending)
        {
            while (!qp->sr.swBitMap.empty() && qp->sr.swBitMap.front() == 1)
            {
                qp->sr.soft_eACK += mtu;
                qp->sr.swBitMap.erase(qp->sr.swBitMap.begin());
            }
        }
        return;
    }
    uint32_t swAnchor = qp->sr.swPending ? qp->sr.hwAnchor : qp->sr.soft_eACK;
    uint32_t idx = (seq - swAnchor) / mtu;
    if (idx >= qp->sr.swBitMap.size())
    {
        qp->sr.swBitMap.resize(idx + 1, 0);
    }
    qp->sr.swBitMap[idx] = 1;
}

// Handle ACK packets on the sender side under SR mode.
void
RdmaHw::SrTxHandleAck(Ptr<RdmaQueuePair> qp, uint32_t seq)
{
    SrInitTxIfNeeded(qp);
    if (!qp->sr.SendOutOfOrder)
    {
        // State A: pure in-order ack. hw bitmap is "not in use"; just bump
        // snd_una.  No bitmap maintenance needed.
        qp->Acknowledge(seq + m_mtu);
        return;
    }

    if (seq < qp->snd_una)
    {
        return; // duplicate ack
    }

    if (qp->sr.sackHigh < seq)
    {
        qp->sr.sackHigh = seq;
    }

    // Cumulative ack semantics: seq means "receiver has all of [0, seq+MTU)".
    // When seq > snd_una the receiver's expected PSN has jumped past holes
    // (a retransmit filled the head, exposing previously-OOO packets). Mark
    // every bit from snd_una up to seq as acked so SrTxAdvanceEACK can walk
    // forward.
    if (seq > qp->snd_una)
    {
        for (uint32_t s = qp->snd_una; s <= seq; s += m_mtu)
        {
            SrTxRecordBit(qp, s, m_mtu, m_srK);
        }
    }
    else
    {
        SrTxRecordBit(qp, seq, m_mtu, m_srK);
    }

    SrTxAdvanceEACK(qp);

    if (qp->snd_una > qp->sr.sackHigh)
    {
        qp->sr.SendOutOfOrder  = false;
        qp->test.recoveryState = 0;
        qp->sr.retransQueue.clear();
    }
    else if (!qp->sr.retransQueue.empty())
    {
        while (!qp->sr.retransQueue.empty() &&
               qp->sr.retransQueue.front() < qp->snd_una)
        {
            qp->sr.retransQueue.erase(qp->sr.retransQueue.begin());
        }
    }
}

// Handle SACK packets on the sender side under SR mode.
void
RdmaHw::SrTxHandleSack(Ptr<RdmaQueuePair> qp, uint32_t seq, uint32_t expectedPSN)
{
    SrInitTxIfNeeded(qp);

    // First-OOO transition (mirrors the receiver): the hw bitmap is "not in
    // use" during in-order flow; the moment we see a SACK we open the window
    // anchored at the current snd_una.
    if (!qp->sr.SendOutOfOrder)
    {
        for (uint32_t i = 0; i < m_srK; i++) { qp->sr.hwBitMap[i] = 0; }
        qp->sr.swBitMap.clear();
        qp->sr.hwAnchor  = qp->snd_una;
        qp->sr.soft_eACK = qp->snd_una + m_srK * m_mtu;
    }

    // Step 1: align eACK with the receiver's expectedPSN if the ack hasn't
    // caught up. Mark bits between snd_una and expectedPSN as received.
    if (expectedPSN > qp->snd_una)
    {
        uint32_t startBit = (qp->snd_una - qp->sr.hwAnchor) / m_mtu;
        uint32_t pkts = (expectedPSN - qp->snd_una) / m_mtu;
        for (uint32_t i = 0; i < pkts && startBit + i < m_srK; i++)
        {
            qp->sr.hwBitMap[startBit + i] = 1;
        }
        SrTxAdvanceEACK(qp);
    }

    if (seq < qp->snd_una)
    {
        return; // already acked
    }

    bool newGap = (seq > qp->sr.sackHigh);
    if (newGap)
    {
        // sackHigh == 0 is the "no SACK seen yet" sentinel: PSN 0 is impossible
        // as an OOO arrival (it's always the head). When it's 0 we use snd_una
        // as the lower bound, otherwise we resume after the previously highest
        // SACKed PSN. Without this, when snd_una < MTU and the very first lost
        // packet is PSN 0, the loop range collapses to empty and PSN 0 never
        // gets enqueued for retransmission.
        uint32_t lo = (qp->sr.sackHigh > 0)
                          ? std::max((uint32_t)qp->snd_una, qp->sr.sackHigh + m_mtu)
                          : (uint32_t)qp->snd_una;
        for (uint32_t s = lo; s < seq; s += m_mtu)
        {
            qp->sr.retransQueue.push_back(s);
        }
        qp->sr.sackHigh = seq;
    }

    if (!qp->sr.SendOutOfOrder)
    {
        qp->sr.SendOutOfOrder  = true;
        qp->test.recoveryState = 1;
    }

    SrTxRecordBit(qp, seq, m_mtu, m_srK);

    // The freshly-set bit may be the one that completes the K window
    // (e.g. seq == hwAnchor and the previous K-1 bits were already 1, or
    // a SACK that fills the head hole). Without this walk the K window
    // stays all-1 with snd_una stranded behind, never promotes, and the
    // flow stalls until RTO.
    SrTxAdvanceEACK(qp);

    if (qp->snd_una > qp->sr.sackHigh)
    {
        qp->sr.SendOutOfOrder  = false;
        qp->test.recoveryState = 0;
        qp->sr.retransQueue.clear();
    }
}

/******************************************************************************
 * IRN (Mittal et al., SIGCOMM 2018) - hardware bitmap, BDP-bounded window.
 *
 * IRN's NIC keeps a per-flow bitmap of size BDP-cap (in packets). All in-flight
 * packets fit inside this window, so OOO packets always land within the bitmap.
 * Receiver sends per-packet SACK on OOO arrival, sender retransmits exactly
 * the missing PSNs. Loss recovery ends when snd_una catches up with sackHigh.
 *
 * In simulation, m_srK is used as the BDP-sized window. If a packet falls
 * outside the K-window we treat it like RoCE (dropped) - with a properly sized
 * K this does not happen, since BDP-FC bounds the in-flight count to <= K.
 *****************************************************************************/

void
RdmaHw::IrnRxHandlePacket(Ptr<RdmaRxQueuePair> q, uint32_t seq)
{
    // Hardware bitmap of size K, anchored at ePSN
    if (q->sr.hwBitMap.size() != m_srK)
    {
        q->sr.hwBitMap.assign(m_srK, 0);
    }
    uint32_t expected = q->ReceiverNextExpectedSeq;

    if (seq < expected)
    {
        return; // duplicate
    }
    if (seq == expected && !q->sr.ReceiveOutOfOrder)
    {
        // Pure in-order arrival - fast path
        q->ReceiverNextExpectedSeq += m_mtu;
        return;
    }
    if (seq == expected)
    {
        // Filled the head; slide hwBitMap left and walk past any '1's
        q->ReceiverNextExpectedSeq += m_mtu;
        uint32_t advance = 0;
        while (advance + 1 < m_srK && q->sr.hwBitMap[advance + 1] == 1)
        {
            advance++;
        }
        q->ReceiverNextExpectedSeq += advance * m_mtu;
        // Shift the bitmap left by (advance+1) bits
        uint32_t shift = advance + 1;
        for (uint32_t i = shift; i < m_srK; i++)
        {
            q->sr.hwBitMap[i - shift] = q->sr.hwBitMap[i];
        }
        for (uint32_t i = m_srK - shift; i < m_srK; i++)
        {
            q->sr.hwBitMap[i] = 0;
        }
        if (q->ReceiverNextExpectedSeq >= q->sr.receiveHigh + m_mtu)
        {
            q->sr.ReceiveOutOfOrder = false;
            q->test.recoveryState = 0;
        }
        return;
    }
    // Out-of-order arrival within hardware window
    uint32_t hwLimit = expected + m_srK * m_mtu;
    if (seq >= hwLimit)
    {
        // Outside bitmap window: BDP-FC should prevent this. Treat as drop.
        return;
    }
    q->sr.ReceiveOutOfOrder = true;
    q->test.recoveryState = 1;
    if (seq > q->sr.receiveHigh)
    {
        q->sr.receiveHigh = seq;
    }
    if (q->test.sack_high < seq)
    {
        q->test.sack_high = seq;
    }
    uint32_t off = (seq - expected) / m_mtu;
    q->sr.hwBitMap[off] = 1;
}

void
RdmaHw::IrnTxHandleAck(Ptr<RdmaQueuePair> qp, uint32_t seq)
{
    if (qp->sr.hwBitMap.size() != m_srK)
    {
        qp->sr.hwBitMap.assign(m_srK, 0);
    }
    if (seq < qp->snd_una)
    {
        return;
    }
    if (seq == qp->snd_una)
    {
        // Cumulative ack at head: advance and slide bitmap
        qp->Acknowledge(seq + m_mtu);
        uint32_t advance = 0;
        while (advance + 1 < m_srK && qp->sr.hwBitMap[advance + 1] == 1)
        {
            advance++;
        }
        qp->Acknowledge(qp->snd_una + advance * m_mtu);
        uint32_t shift = advance + 1;
        for (uint32_t i = shift; i < m_srK; i++)
        {
            qp->sr.hwBitMap[i - shift] = qp->sr.hwBitMap[i];
        }
        for (uint32_t i = m_srK - shift; i < m_srK; i++)
        {
            qp->sr.hwBitMap[i] = 0;
        }
        if (qp->snd_una > qp->sr.sackHigh)
        {
            qp->sr.SendOutOfOrder = false;
            qp->test.recoveryState = 0;
            qp->sr.retransQueue.clear();
        }
        else
        {
            IrnTxRequeueNextHole(qp, qp->snd_una);
        }
        return;
    }
    // Cumulative ack above snd_una. Receiver advances expected past contiguous
    // OOO arrivals once a hole gets filled by a retransmit, so cumulative ACK
    // can jump multiple MTUs in one shot. Advance snd_una to seq+MTU and
    // shift the bitmap by that amount; then walk forward past any further
    // contiguous SACKed entries.
    uint32_t hwLimit = qp->snd_una + m_srK * m_mtu;
    if (seq >= hwLimit)
    {
        // SACK seq is outside the hardware window. Best-effort: just advance
        // snd_una to seq+MTU. SACKed bits past this are lost from the small
        // bitmap (BDP-FC normally prevents this).
        qp->Acknowledge(seq + m_mtu);
        for (uint32_t i = 0; i < m_srK; i++) { qp->sr.hwBitMap[i] = 0; }
        if (qp->snd_una > qp->sr.sackHigh)
        {
            qp->sr.SendOutOfOrder = false;
            qp->test.recoveryState = 0;
            qp->sr.retransQueue.clear();
        }
        else
        {
            IrnTxRequeueNextHole(qp, qp->snd_una);
        }
        return;
    }

    uint32_t shift = (seq - qp->snd_una) / m_mtu + 1;
    qp->Acknowledge(seq + m_mtu);
    // Walk forward past any SACKed slots adjacent to the new head.
    uint32_t advance = 0;
    while (shift + advance < m_srK && qp->sr.hwBitMap[shift + advance] == 1)
    {
        advance++;
    }
    qp->Acknowledge(qp->snd_una + advance * m_mtu);
    uint32_t totalShift = shift + advance;
    for (uint32_t i = totalShift; i < m_srK; i++)
    {
        qp->sr.hwBitMap[i - totalShift] = qp->sr.hwBitMap[i];
    }
    for (uint32_t i = m_srK - totalShift; i < m_srK; i++)
    {
        qp->sr.hwBitMap[i] = 0;
    }
    if (qp->snd_una > qp->sr.sackHigh)
    {
        qp->sr.SendOutOfOrder = false;
        qp->test.recoveryState = 0;
        qp->sr.retransQueue.clear();
    }
    else
    {
        IrnTxRequeueNextHole(qp, qp->snd_una);
    }
}

void
RdmaHw::IrnTxHandleSack(Ptr<RdmaQueuePair> qp, uint32_t seq)
{
    if (qp->sr.hwBitMap.size() != m_srK)
    {
        qp->sr.hwBitMap.assign(m_srK, 0);
    }
    if (seq < qp->snd_una)
    {
        return;
    }
    if (!qp->sr.SendOutOfOrder)
    {
        qp->sr.SendOutOfOrder = true;
        qp->test.recoveryState = 1;
    }
    if (seq > qp->sr.sackHigh)
    {
        qp->sr.sackHigh = seq;
    }
    // Mark the SACK bit. Official IRN: bitmap |= 1 << (sackNo - lastAckedPsn).
    // We do NOT bulk-enumerate gaps into retransQueue here; that was the old
    // design and produced spurious retransmits when SACKs arrived OOO or after
    // the ePSN fast-sync left sackHigh stale. The bitmap is now the sole
    // authority on "what's missing", and IrnTxRequeueNextHole walks it
    // lazily to pick the next retransmit (mirroring the official Vivado HLS
    // findNewHole flow in receiveAck.cpp + txFree.cpp).
    uint32_t hwLimit = qp->snd_una + m_srK * m_mtu;
    if (seq < hwLimit)
    {
        uint32_t off = (seq - qp->snd_una) / m_mtu;
        qp->sr.hwBitMap[off] = 1;
    }
    // Else: BDP-FC should have prevented this; rely on retransmit timeout.

    IrnTxRequeueNextHole(qp, qp->snd_una);
}

uint32_t
RdmaHw::IrnTxFindNextHole(Ptr<RdmaQueuePair> qp, uint32_t startPsn)
{
    if (qp->sr.hwBitMap.size() != m_srK)
    {
        qp->sr.hwBitMap.assign(m_srK, 0);
    }
    uint32_t hwLimit = qp->snd_una + m_srK * m_mtu;
    if (startPsn < qp->snd_una)
    {
        startPsn = qp->snd_una;
    }
    // Only scan up to (and including) sackHigh — anything beyond sackHigh
    // hasn't been observed missing yet, so it doesn't need preemptive
    // retransmission. This mirrors the paper: "any subsequent packet is
    // considered lost only if another packet with a higher sequence number
    // has been selectively acked."
    uint32_t scanEnd = std::min(hwLimit, qp->sr.sackHigh + m_mtu);
    for (uint32_t psn = startPsn; psn < scanEnd; psn += m_mtu)
    {
        uint32_t off = (psn - qp->snd_una) / m_mtu;
        if (qp->sr.hwBitMap[off] == 0)
        {
            return psn;
        }
    }
    return 0; // sentinel: no hole below sackHigh
}

void
RdmaHw::IrnTxRequeueNextHole(Ptr<RdmaQueuePair> qp, uint32_t startPsn)
{
    // Drop any stale entries (already-ACKed or already-SACKed) from the front
    // of the queue. Official IRN: "if doRetransmit && retransmitSN==sackNo
    // → findNewHole; doRetransmit=false" — we approximate that by re-validating
    // the queued PSN against the current bitmap state.
    if (qp->sr.hwBitMap.size() != m_srK)
    {
        qp->sr.hwBitMap.assign(m_srK, 0);
    }
    uint32_t hwLimit = qp->snd_una + m_srK * m_mtu;
    while (!qp->sr.retransQueue.empty())
    {
        uint32_t psn = qp->sr.retransQueue.front();
        if (psn < qp->snd_una)
        {
            qp->sr.retransQueue.erase(qp->sr.retransQueue.begin());
            continue;
        }
        if (psn < hwLimit && qp->sr.hwBitMap[(psn - qp->snd_una) / m_mtu] == 1)
        {
            qp->sr.retransQueue.erase(qp->sr.retransQueue.begin());
            continue;
        }
        break;
    }
    // If queue still empty, find next hole. Conservatively keep at most ONE
    // entry queued at a time, matching the official ref's single-register
    // retransmitSN. If we already have a head entry, leave it alone.
    if (qp->sr.retransQueue.empty() && qp->sr.SendOutOfOrder)
    {
        uint32_t hole = IrnTxFindNextHole(qp, startPsn);
        if (hole != 0)
        {
            qp->sr.retransQueue.push_back(hole);
        }
    }
}

/******************************************************************************
 * SRNIC (Wang et al., NSDI 2023) - software bitmap, async hw/sw interaction.
 *
 * SRNIC's NIC tracks only ePSN and LACK in hardware; the per-flow bitmap
 * lives in host (software) memory.  In-order packets follow a hardware-only
 * fast path; out-of-order packets are punted to the slow path where software
 * updates an unbounded bitmap and notifies the NIC via control queues.
 *
 * Async model: the OOO packet is parked in `swPendingQueue` and the bitmap
 * update is scheduled `m_srnicAsyncNs` later via Simulator::Schedule.  Until
 * the scheduled callback fires, the packet is invisible to the protocol state
 * machine - if the sender retransmits we may double-mark, but the bitmap is
 * idempotent.  This faithfully models the CtrlQ round-trip cost.
 *****************************************************************************/

void
RdmaHw::SrnicRxApplyOoO(Ptr<RdmaRxQueuePair> q, uint32_t seq)
{
    // Slow-path completion: update the software bitmap now.
    uint32_t expected = q->ReceiverNextExpectedSeq;
    if (seq < expected)
    {
        // The hole has already been closed during the async window; no-op.
        return;
    }
    if (seq == expected)
    {
        // The retransmit arrived while we were scheduling; treat as in-order.
        q->ReceiverNextExpectedSeq += m_mtu;
        if (!q->sr.swBitMap.empty())
        {
            uint32_t advance = 0;
            while (advance < q->sr.swBitMap.size() &&
                   q->sr.swBitMap[advance] == 1)
            {
                advance++;
            }
            q->ReceiverNextExpectedSeq += advance * m_mtu;
            q->sr.swBitMap.erase(q->sr.swBitMap.begin(),
                                 q->sr.swBitMap.begin() + advance);
        }
        if (q->ReceiverNextExpectedSeq >= q->sr.receiveHigh + m_mtu)
        {
            q->sr.ReceiveOutOfOrder = false;
            q->test.recoveryState = 0;
        }
        return;
    }
    uint32_t off = (seq - expected) / m_mtu - 1;
    if (off >= q->sr.swBitMap.size())
    {
        q->sr.swBitMap.resize(off + 1, 0);
    }
    q->sr.swBitMap[off] = 1;
    q->sr.swPending = false;
}

void
RdmaHw::SrnicRxHandlePacket(Ptr<RdmaRxQueuePair> q, uint32_t seq)
{
    uint32_t expected = q->ReceiverNextExpectedSeq;

    if (seq < expected)
    {
        return; // duplicate
    }
    if (seq == expected && !q->sr.ReceiveOutOfOrder)
    {
        // Hardware-only fast path: just bump ePSN
        q->ReceiverNextExpectedSeq += m_mtu;
        return;
    }
    if (seq == expected)
    {
        // Slow-path -> fast-path transition.
        // Paper §4.4.2 / Fig 8 fast-exit race handling: while software is
        // computing new_ePSN through CtrlQ, RNIC has continued to receive
        // OOO packets whose metadata is still in flight. Without race
        // handling, the exit would set ePSN to the (now-stale) software
        // value and need another CtrlQ round-trip to catch up. RNIC
        // tracks the high-water consecutive PSN it has observed
        // (srnicSeqHigh) synchronously, so on exit it can leap ePSN past
        // every contiguous OOO arrival immediately. swBitMap walk remains
        // as a fallback for any non-contiguous late arrivals.
        q->ReceiverNextExpectedSeq += m_mtu;
        if (q->sr.srnicSeqHigh + m_mtu > q->ReceiverNextExpectedSeq)
        {
            uint32_t leap = (q->sr.srnicSeqHigh + m_mtu) -
                            q->ReceiverNextExpectedSeq;
            uint32_t slots = leap / m_mtu;
            q->ReceiverNextExpectedSeq = q->sr.srnicSeqHigh + m_mtu;
            // Trim any bits in swBitMap that the leap consumed.
            if (!q->sr.swBitMap.empty())
            {
                size_t toErase = std::min((size_t)slots, q->sr.swBitMap.size());
                q->sr.swBitMap.erase(q->sr.swBitMap.begin(),
                                     q->sr.swBitMap.begin() + toErase);
            }
        }
        if (!q->sr.swBitMap.empty())
        {
            uint32_t advance = 0;
            while (advance < q->sr.swBitMap.size() &&
                   q->sr.swBitMap[advance] == 1)
            {
                advance++;
            }
            q->ReceiverNextExpectedSeq += advance * m_mtu;
            q->sr.swBitMap.erase(q->sr.swBitMap.begin(),
                                 q->sr.swBitMap.begin() + advance);
        }
        if (q->ReceiverNextExpectedSeq >= q->sr.receiveHigh + m_mtu)
        {
            q->sr.ReceiveOutOfOrder = false;
            q->test.recoveryState = 0;
            q->sr.srnicSeqHigh = 0;
        }
        return;
    }
    // Out-of-order: defer the bitmap update by m_srnicAsyncNs.
    q->sr.ReceiveOutOfOrder = true;
    q->test.recoveryState = 1;
    if (seq > q->sr.receiveHigh)
    {
        q->sr.receiveHigh = seq;
    }
    if (q->test.sack_high < seq)
    {
        q->test.sack_high = seq;
    }
    // Synchronous fast-exit tracker (Fig 8): extend the consecutive run
    // ending at srnicSeqHigh if this PSN extends it. Initialized to the
    // current ePSN so seq == ePSN + MTU bootstraps the run.
    if (q->sr.srnicSeqHigh < expected)
    {
        q->sr.srnicSeqHigh = expected;
    }
    if (seq == q->sr.srnicSeqHigh + m_mtu)
    {
        q->sr.srnicSeqHigh = seq;
    }
    q->sr.swPending = true;
    q->sr.swDeadlineNs = Simulator::Now().GetNanoSeconds() + m_srnicAsyncNs;
    if (m_srnicAsyncNs == 0)
    {
        SrnicRxApplyOoO(q, seq);
    }
    else
    {
        Simulator::Schedule(NanoSeconds(m_srnicAsyncNs),
                            &RdmaHw::SrnicRxApplyOoO, this, q, seq);
    }
}

void
RdmaHw::SrnicTxApplyOoO(Ptr<RdmaQueuePair> qp, uint32_t seq)
{
    if (seq < qp->snd_una)
    {
        return; // already cumulatively acked while waiting in CtrlQ
    }
    uint32_t off = (seq - qp->snd_una) / m_mtu;
    if (off >= qp->sr.swBitMap.size())
    {
        qp->sr.swBitMap.resize(off + 1, 0);
    }
    qp->sr.swBitMap[off] = 1;
    qp->sr.swPending = false;
    qp->sr.srnicInflightRetx.erase(seq);

    // Walk forward through consecutive leading 1s. After this SACK marks a
    // bit, the head of swBitMap may now hold a run of 1s that should be
    // promoted into cumulative ACK (snd_una). Without this walk, a hole that
    // is later "filled" by a sender-side retransmit + receiver fast-sync
    // (which advances snd_una to the bit boundary) won't be promoted further
    // even though every subsequent slot is already 1. The result is snd_una
    // stalled while swBitMap is nearly all 1s, which fools RTO sweep (every
    // PSN looks acked) and stalls the flow.
    if (qp->sr.SendOutOfOrder && !qp->sr.swBitMap.empty())
    {
        uint32_t advance = 0;
        while (advance < qp->sr.swBitMap.size() &&
               qp->sr.swBitMap[advance] == 1)
        {
            advance++;
        }
        if (advance > 0)
        {
            qp->Acknowledge(qp->snd_una + advance * m_mtu);
            qp->sr.swBitMap.erase(qp->sr.swBitMap.begin(),
                                  qp->sr.swBitMap.begin() + advance);
            while (!qp->sr.retransQueue.empty() &&
                   qp->sr.retransQueue.front() < qp->snd_una)
            {
                qp->sr.retransQueue.erase(qp->sr.retransQueue.begin());
            }
            while (!qp->sr.srnicInflightRetx.empty() &&
                   *qp->sr.srnicInflightRetx.begin() < qp->snd_una)
            {
                qp->sr.srnicInflightRetx.erase(
                    qp->sr.srnicInflightRetx.begin());
            }
            if (qp->snd_una > qp->sr.sackHigh)
            {
                qp->sr.SendOutOfOrder = false;
                qp->test.recoveryState = 0;
                qp->sr.retransQueue.clear();
                qp->sr.srnicInflightRetx.clear();
            }
        }
    }
    SrTxRecomputeSackedBytes(qp);

    // Paper §4.4.2: "software ... manipulates the bitmaps ... and makes
    // retransmission decision accordingly. The retransmitted requests are
    // submitted through a Retry Queue (RetryQ)." We do a bulk scan of the
    // bitmap and push every still-missing PSN that is not already issued
    // or queued, matching the paper's single-software-decision per scan.
    SrnicTxRequeueNextHole(qp, qp->snd_una);

    // Wake the device: if the QP scheduler is idle (e.g. cwnd-bound right
    // before this SACK), the newly-queued retransmit must trigger transmit.
    uint32_t nic_idx = GetNicIdxOfQp(qp);
    if (nic_idx < m_nic.size() && m_nic[nic_idx].dev)
    {
        m_nic[nic_idx].dev->TriggerTransmit();
    }
}

void
RdmaHw::SrnicTxRequeueNextHole(Ptr<RdmaQueuePair> qp, uint32_t startPsn)
{
    // Paper §4.4.2: software, after each bitmap update, "makes retransmission
    // decision accordingly". We enumerate ALL missing PSNs in [snd_una,
    // sackHigh] (bitmap slot == 0) that are NOT already in the RetryQ and
    // NOT already issued (srnicInflightRetx), and push them in order. This
    // matches the paper's single-software-decision model: software has the
    // full bitmap view and emits all needed retransmits per scan.
    if (qp->sr.sackHigh < qp->snd_una)
    {
        return;
    }
    std::set<uint32_t> queued(qp->sr.retransQueue.begin(),
                              qp->sr.retransQueue.end());
    uint32_t scanStart = std::max(startPsn, (uint32_t)qp->snd_una);
    for (uint32_t psn = scanStart; psn <= qp->sr.sackHigh; psn += m_mtu)
    {
        uint32_t off = (psn - qp->snd_una) / m_mtu;
        if (off < qp->sr.swBitMap.size() && qp->sr.swBitMap[off] == 1)
        {
            continue; // already SACKed
        }
        if (queued.count(psn))
        {
            continue; // already in RetryQ awaiting dequeue
        }
        if (qp->sr.srnicInflightRetx.count(psn))
        {
            continue; // already retransmitted, awaiting confirmation
        }
        qp->sr.retransQueue.push_back(psn);
        queued.insert(psn);
    }
}

void
RdmaHw::SrnicTxHandleAck(Ptr<RdmaQueuePair> qp, uint32_t seq)
{
    if (seq < qp->snd_una)
    {
        // Stale ACK (already covered by an earlier fast-sync). Still rescan
        // because the shift may have exposed previously-suppressed holes.
        if (qp->sr.SendOutOfOrder)
        {
            SrnicTxRequeueNextHole(qp, qp->snd_una);
        }
        return;
    }
    uint32_t old_una = qp->snd_una;
    qp->Acknowledge(seq + m_mtu);
    // Shift swBitMap left to match the new snd_una frame. The bitmap is
    // anchored at snd_una (slot k = PSN snd_una + k*MTU). Without this
    // shift, after Acknowledge moves snd_una forward by N slots, the bits
    // that were set for the OOO-SACKed PSNs end up misaligned and the
    // walk-forward loop below finds slot 0 = 0 and exits immediately,
    // stranding snd_una behind already-SACKed contiguous PSNs.
    uint32_t cum_shift = (qp->snd_una - old_una) / m_mtu;
    if (cum_shift > 0 && !qp->sr.swBitMap.empty())
    {
        size_t to_erase = std::min((size_t)cum_shift, qp->sr.swBitMap.size());
        qp->sr.swBitMap.erase(qp->sr.swBitMap.begin(),
                              qp->sr.swBitMap.begin() + to_erase);
    }
    if (qp->sr.SendOutOfOrder && !qp->sr.swBitMap.empty())
    {
        uint32_t advance = 0;
        while (advance < qp->sr.swBitMap.size() &&
               qp->sr.swBitMap[advance] == 1)
        {
            advance++;
        }
        if (advance > 0)
        {
            qp->Acknowledge(qp->snd_una + advance * m_mtu);
            qp->sr.swBitMap.erase(qp->sr.swBitMap.begin(),
                                  qp->sr.swBitMap.begin() + advance);
            // Drop any retrans entries that have been satisfied
            while (!qp->sr.retransQueue.empty() &&
                   qp->sr.retransQueue.front() < qp->snd_una)
            {
                qp->sr.retransQueue.erase(qp->sr.retransQueue.begin());
            }
        }
    }
    // Paper §4.4.2: exit loss recovery when "all retransmitted packets are
    // successfully delivered (indicated by ACKs)". snd_una > sackHigh ⇒
    // every SACKed-or-missing PSN has been cumulatively ACKed.
    if (qp->snd_una > qp->sr.sackHigh)
    {
        qp->sr.SendOutOfOrder = false;
        qp->test.recoveryState = 0;
        qp->sr.retransQueue.clear();
        qp->sr.srnicInflightRetx.clear();
    }
    else if (qp->sr.SendOutOfOrder)
    {
        // snd_una advanced (cumulative ACK from a previously-retransmitted
        // hole) but other gaps remain: drop confirmed inflight entries and
        // rescan bitmap for any unsatisfied holes.
        while (!qp->sr.srnicInflightRetx.empty() &&
               *qp->sr.srnicInflightRetx.begin() < qp->snd_una)
        {
            qp->sr.srnicInflightRetx.erase(
                qp->sr.srnicInflightRetx.begin());
        }
        SrnicTxRequeueNextHole(qp, qp->snd_una);
    }
}

void
RdmaHw::SrnicTxHandleSack(Ptr<RdmaQueuePair> qp, uint32_t seq)
{
    if (seq < qp->snd_una)
    {
        return;
    }
    if (!qp->sr.SendOutOfOrder)
    {
        qp->sr.SendOutOfOrder = true;
        qp->test.recoveryState = 1;
    }
    if (seq > qp->sr.sackHigh)
    {
        qp->sr.sackHigh = seq;
    }
    // Paper §4.4.2 / Fig 7 step Á: requester hardware sends OOO metadata
    // (PSN & lACK) over CtrlQ to the software retransmission module, which
    // then "manipulates the bitmaps in host memory ... and makes
    // retransmission decision accordingly". We model the CtrlQ round-trip
    // by deferring both the bitmap mark and the retransmit-scan by
    // m_srnicAsyncNs. No immediate bulk-push to RetryQ — that would mimic
    // a hardware-side bitmap (which SRNIC explicitly omits to save SRAM).
    qp->sr.swPending = true;
    qp->sr.swDeadlineNs = Simulator::Now().GetNanoSeconds() + m_srnicAsyncNs;
    if (m_srnicAsyncNs == 0)
    {
        SrnicTxApplyOoO(qp, seq);
    }
    else
    {
        Simulator::Schedule(NanoSeconds(m_srnicAsyncNs),
                            &RdmaHw::SrnicTxApplyOoO, this, qp, seq);
    }
}

/******************************************************************************
 * CX6 / Classic RoCEv2 go-back-N (Mellanox ConnectX-6 retransmission model).
 *
 * Receiver discards any OOO packet outright (no bitmap, no buffering); it then
 * NACKs back with the cumulative expected PSN. Sender on receiving the NACK
 * rewinds snd_nxt to snd_una so the device's forward-send path replays every
 * PSN from there. There is no selective retransmission, no per-packet SACK,
 * and no software bitmap state — this is the lossy-network baseline that
 * SR/IRN/RaLL/SRNIC are designed to outperform.
 *****************************************************************************/

void
RdmaHw::Cx6RxHandlePacket(Ptr<RdmaRxQueuePair> q, uint32_t seq)
{
    uint32_t expected = q->ReceiverNextExpectedSeq;
    if (seq < expected)
    {
        return; // duplicate — sender's go-back retx, already delivered
    }
    if (seq == expected)
    {
        // In-order arrival. Bump ePSN, leave recovery state.
        q->ReceiverNextExpectedSeq += m_mtu;
        q->sr.ReceiveOutOfOrder = false;
        q->test.recoveryState = 0;
        return;
    }
    // seq > expected: OOO. CX6 drops the packet; the NACK generated by
    // ReceiverCheckSeq carries epsn = ReceiverNextExpectedSeq (unchanged),
    // which tells the sender to roll back to that PSN.
    q->sr.ReceiveOutOfOrder = true;
    q->test.recoveryState = 1;
}

void
RdmaHw::Cx6TxHandleAck(Ptr<RdmaQueuePair> qp, uint32_t seq)
{
    // Fast-sync block in HandleAckForSR already advanced snd_una to epsn.
    // Normal ACK has seq == epsn-MTU which is < snd_una now; nothing to do.
    if (seq >= qp->snd_una)
    {
        qp->Acknowledge(seq + m_mtu);
    }
}

void
RdmaHw::Cx6TxHandleSack(Ptr<RdmaQueuePair> qp, uint32_t seq)
{
    // GBN rewind: any in-flight forward sends past snd_una are presumed lost
    // (since CX6 receiver drops them all when OOO). Set snd_nxt = snd_una so
    // GetNxtPacket's forward path re-issues from there.
    if (qp->snd_nxt > qp->snd_una)
    {
        qp->snd_nxt = qp->snd_una;
    }
    qp->sr.SendOutOfOrder = false;
    qp->test.recoveryState = 0;
    // CX6 keeps no per-PSN state; clear any stale retrans queue (defensive).
    qp->sr.retransQueue.clear();
}

} // namespace ns3
