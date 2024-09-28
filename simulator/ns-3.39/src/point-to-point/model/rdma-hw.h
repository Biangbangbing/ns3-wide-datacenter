#ifndef RDMA_HW_H
#define RDMA_HW_H

// #include <ns3/rdma.h>
#include "qbb-net-device.h"

#include <ns3/custom-header.h>
#include <ns3/node.h>
#include <ns3/rdma-queue-pair.h>

#include <cstdint>
#include <unordered_map>


namespace ns3
{

struct RdmaInterfaceMgr
{
    Ptr<QbbNetDevice> dev;
    Ptr<RdmaQueuePairGroup> qpGrp;

    RdmaInterfaceMgr()
        : dev(NULL),
          qpGrp(NULL)
    {
    }

    RdmaInterfaceMgr(Ptr<QbbNetDevice> _dev)
    {
        dev = _dev;
    }
};

class RdmaHw : public Object
{
  public:
    static TypeId GetTypeId(void);
    RdmaHw();

    Ptr<Node> m_node;
    DataRate m_minRate; //< Min sending rate
    uint32_t m_mtu;
    uint32_t m_cc_mode;
    double m_nack_interval;
    uint32_t m_chunk;
    uint32_t m_ack_interval;
    bool m_backto0;
    bool m_var_win, m_fast_react;
    bool m_rateBound;
    std::vector<RdmaInterfaceMgr> m_nic; // list of running nic controlled by this RdmaHw
    std::unordered_map<uint64_t, Ptr<RdmaQueuePair>> m_qpMap;     // mapping from uint64_t to qp
    std::unordered_map<uint64_t, Ptr<RdmaRxQueuePair>> m_rxQpMap; // mapping from uint64_t to rx qp
    std::unordered_map<uint32_t, std::vector<int>>
        m_rtTable; // map from ip address (u32) to possible ECMP port (index of dev)

    // qp complete callback
    typedef Callback<void, Ptr<RdmaQueuePair>> QpCompleteCallback;
    QpCompleteCallback m_qpCompleteCallback;

    void SetNode(Ptr<Node> node);
    void Setup(QpCompleteCallback cb); // setup shared data and callbacks with the QbbNetDevice
    static uint64_t GetQpKey(uint32_t dip,
                             uint16_t sport,
                             uint16_t pg); // get the lookup key for m_qpMap
    Ptr<RdmaQueuePair> GetQp(uint32_t dip, uint16_t sport, uint16_t pg); // get the qp
    uint32_t GetNicIdxOfQp(Ptr<RdmaQueuePair> qp); // get the NIC index of the qp
    void AddQueuePair(uint64_t size,
                      uint16_t pg,
                      Ipv4Address _sip,
                      Ipv4Address _dip,
                      uint16_t _sport,
                      uint16_t _dport,
                      uint32_t win,
                      uint64_t baseRtt,
                      Callback<void> notifyAppFinish,
                      Time stopTime); // add a new qp (new send)
    void DeleteQueuePair(Ptr<RdmaQueuePair> qp);

    Ptr<RdmaRxQueuePair> GetRxQp(uint32_t sip,
                                 uint32_t dip,
                                 uint16_t sport,
                                 uint16_t dport,
                                 uint16_t pg,
                                 bool create);        // get a rxQp
    uint32_t GetNicIdxOfRxQp(Ptr<RdmaRxQueuePair> q); // get the NIC index of the rxQp
    void DeleteRxQp(uint32_t dip, uint16_t pg, uint16_t dport);

    int ReceiveUdp(Ptr<Packet> p, CustomHeader& ch);
    int ReceiveCnp(Ptr<Packet> p, CustomHeader& ch);
    int ReceiveAck(Ptr<Packet> p, CustomHeader& ch); // handle both ACK and NACK
    int Receive(Ptr<Packet> p,
                CustomHeader&
                    ch); // callback function that the QbbNetDevice should use when receive packets.
                         // Only NIC can call this function. And do not call this upon PFC

    void CheckandSendQCN(Ptr<RdmaRxQueuePair> q);
    int ReceiverCheckSeq(uint32_t seq, Ptr<RdmaRxQueuePair> q, uint32_t size) const;
    void AddHeader(Ptr<Packet> p, uint16_t protocolNumber);
    static uint16_t EtherToPpp(uint16_t protocol);

    void RecoverQueue(Ptr<RdmaQueuePair> qp);
    void QpComplete(Ptr<RdmaQueuePair> qp);
    void SetLinkDown(Ptr<QbbNetDevice> dev);

    // call this function after the NIC is setup
    void AddTableEntry(Ipv4Address& dstAddr, uint32_t intf_idx);
    void ClearTable();
    void RedistributeQp();

    Ptr<Packet> GetNxtPacket(Ptr<RdmaQueuePair> qp); // get next packet to send, inc snd_nxt
    void PktSent(Ptr<RdmaQueuePair> qp, Ptr<Packet> pkt, Time interframeGap) const;
    void UpdateNextAvail(Ptr<RdmaQueuePair> qp, Time interframeGap, uint32_t pkt_size) const;
    void ChangeRate(Ptr<RdmaQueuePair> qp, DataRate new_rate);
    /******************************
     * Mellanox's version of DCQCN
     *****************************/
    double m_g;              // feedback weight
    double m_rateOnFirstCNP; // the fraction of line rate to set on first CNP
    bool m_EcnClampTgtRate;
    double m_rpgTimeReset;
    double m_rateDecreaseInterval;
    uint32_t m_rpgThreshold;
    double m_alpha_resume_interval;
    DataRate m_rai;  //< Rate of additive increase
    DataRate m_rhai; //< Rate of hyper-additive increase

    // the Mellanox's version of alpha update:
    // every fixed time slot, update alpha.
    void UpdateAlphaMlx(Ptr<RdmaQueuePair> q);
    void ScheduleUpdateAlphaMlx(Ptr<RdmaQueuePair> q);

    // Mellanox's version of CNP receive
    void cnp_received_mlx(Ptr<RdmaQueuePair> q);

    // Mellanox's version of rate decrease
    // It checks every m_rateDecreaseInterval if CNP arrived (m_decrease_cnp_arrived).
    // If so, decrease rate, and reset all rate increase related things
    void CheckRateDecreaseMlx(Ptr<RdmaQueuePair> q);
    void ScheduleDecreaseRateMlx(Ptr<RdmaQueuePair> q, uint32_t delta);

    // Mellanox's version of rate increase
    void RateIncEventTimerMlx(Ptr<RdmaQueuePair> q);
    void RateIncEventMlx(Ptr<RdmaQueuePair> q);
    void FastRecoveryMlx(Ptr<RdmaQueuePair> q);
    void ActiveIncreaseMlx(Ptr<RdmaQueuePair> q);
    void HyperIncreaseMlx(Ptr<RdmaQueuePair> q);

    /***********************
     * High Precision CC
     ***********************/
    double m_targetUtil;
    double m_utilHigh;
    uint32_t m_miThresh;
    bool m_multipleRate;
    bool m_sampleFeedback; // only react to feedback every RTT, or qlen > 0
    void HandleAckHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch);
    void UpdateRateHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool fast_react);
    void UpdateRateHpTest(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool fast_react);
    void FastReactHp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch);

    /**********************
     * PowerTCP and Delay-PowerTCP
     *********************/
    bool PowerTCPEnabled;
    bool PowerTCPdelay;
    void UpdateRatePower(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool fast_react);
    void FastReactPower(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch);

    /**********************
     * TIMELY
     *********************/
    double m_tmly_alpha, m_tmly_beta;
    uint64_t m_tmly_TLow, m_tmly_THigh, m_tmly_minRtt;
    void HandleAckTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch);
    void UpdateRateTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool us) const;
    void FastReactTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch);

    /*********************
     * Patched TIMELY
     * Use alpha, Tlow, Thigh from TIMELY
     ********************/
    double m_ptmly_beta;
    uint64_t m_ptmly_RttRef;
    void HandleAckPatchedTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch);
    void UpdateRatePatchedTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool us);
    void FastReactPatchedTimely(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch);

    /**********************
     * DCTCP
     *********************/
    DataRate m_dctcp_rai;
    void HandleAckDctcp(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch) const;

    /*********************
     * HPCC-PINT
     ********************/
    uint32_t pint_smpl_thresh;
    void SetPintSmplThresh(double p);
    void HandleAckHpPint(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch);
    void UpdateRateHpPint(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool fast_react);

    /*********************
     * Swift
     ********************/
    uint32_t swift_ai;          // additive increment
    double swift_beta;          // multiplicative decrease constant
    double swift_max_mdf;       // max multiplicative decrease factor
    uint32_t swift_base_target; // base target RTT
    double swift_hop_scale;     // per hop RTT scaling factor, i.e. ℏ in paper
    double swift_fs_max_cwnd,
        swift_fs_min_cwnd; // max and min cwnd range that flow-based scaling takes effect
    double swift_fs_range; // flow-based scaling max scaling range
    double swift_min_cwnd; // min cwnd Swift can exceed (not fs)
    double swift_max_cwnd; // max cwnd Swift can exceed (not fs)
    double swift_target_endpoint_delay; // target endpoint delay

    void HandleAckSwift(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch) const;
    void UpdateRateSwift(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch, bool fast_react);
    void FastReactSwift(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch);
    uint64_t TargetFabDelaySwift(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch) const;
    double GetCwndSwift(Ptr<RdmaQueuePair> qp,
                        Ptr<Packet> p,
                        CustomHeader& ch,
                        uint64_t target_delay,
                        uint64_t curr_delay) const;

    /*********************
     * RTT-QCN
     ********************/
    uint64_t rtt_qcn_tmin; // max rtt value to generate NO ecn
    uint64_t rtt_qcn_tmax; // min rtt value to ALWAYS generate ecn
    double
        rtt_qcn_alpha; // additive increase when cwnd < 1; use original value, don't multiply by mtu
    double rtt_qcn_beta; // multiplicative decrease when cwnd > 1; use original value, don't
                         // multiply by mtu
    void HandleAckRttQcn(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch) const;

    /*********************
     * PowerQCN
     * Improve RTT-QCN by using gradient alongside RTT
     ********************/
    double powerqcn_grad_min, powerqcn_grad_max; // similar to rtt_qcn_tmin, rtt_qcn_tmax
    void HandleAckPowerQcn(Ptr<RdmaQueuePair> qp, Ptr<Packet> p, CustomHeader& ch);
};

enum CC_MODE
{
    MLX_CNP = 1,
    HPCC = 3,
    POWERTCP = 3,
    THETA_POWERTCP = 3,
    TIMELY = 7,
    DCTCP = 8,
    HPCC_PINT = 10,
    PATCHED_TIMELY = 11,
    SWIFT = 12,
    RTT_QCN = 13,
    POWERQCN = 14,
};

} /* namespace ns3 */

#endif /* RDMA_HW_H */
