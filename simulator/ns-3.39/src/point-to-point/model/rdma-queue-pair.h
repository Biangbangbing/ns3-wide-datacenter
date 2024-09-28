#ifndef RDMA_QUEUE_PAIR_H
#define RDMA_QUEUE_PAIR_H

#include <ns3/custom-header.h>
#include <ns3/data-rate.h>
#include <ns3/event-id.h>
#include <ns3/int-header.h>
#include <ns3/ipv4-address.h>
#include <ns3/object.h>
#include <ns3/packet.h>

#include <cstdint>
#include <vector>
// vamsi
#include <map>

namespace ns3
{

// Queue pair stores runtime information, including window, src and dst ip, and runtime status of CC
// algorithm, etc
class RdmaQueuePair : public Object
{
  public:
    Time startTime;
    Ipv4Address sip, dip;
    uint16_t sport, dport;
    uint64_t m_size;
    uint64_t snd_nxt, snd_una; // next seq to send, the highest unacked seq
    uint16_t m_pg;
    uint16_t m_ipid;
    uint32_t m_win;      // bound of on-the-fly packets (bytes?)
    uint64_t m_baseRtt;  // base RTT of this qp
    DataRate m_max_rate; // max rate
    bool m_var_win;      // variable window size
    Time m_nextAvail;    //< Soonest time of next send
    uint32_t wp;         // current window of packets
    uint32_t lastPktSize;
    Callback<void> m_notifyAppFinish;

    // vamsi
    std::map<uint32_t, double> rates;
    double prevRtt;
    double prevCompletion;
    bool powerEnabled;
    Time stopTime;

    /******************************
     * runtime states
     *****************************/
    DataRate m_rate; //< Current rate

    struct
    {
        DataRate m_targetRate; //< Target rate
        EventId m_eventUpdateAlpha;
        double m_alpha;
        bool m_alpha_cnp_arrived; // indicate if CNP arrived in the last slot
        bool m_first_cnp;         // indicate if the current CNP is the first CNP
        EventId m_eventDecreaseRate;
        bool m_decrease_cnp_arrived; // indicate if CNP arrived in the last slot
        uint32_t m_rpTimeStage;
        EventId m_rpTimer;
    } mlx;

    struct
    {
        uint32_t m_lastUpdateSeq;
        DataRate m_curRate;
        IntHop hop[IntHeader::maxHop];
        uint32_t keep[IntHeader::maxHop];
        uint32_t m_incStage;
        double m_lastGap;
        double u;

        struct
        {
            double u;
            double qRate;
            DataRate Rc;
            uint32_t incStage;
        } hopState[IntHeader::maxHop];
    } hp;

    struct
    {
        uint32_t m_lastUpdateSeq;
        DataRate m_curRate;
        IntHop hop[IntHeader::maxHop];
        uint32_t keep[IntHeader::maxHop];
        uint32_t m_incStage;
        double m_lastGap;
        double u;
        bool useInt;

        struct
        {
            double u;
            DataRate Rc;
            uint32_t incStage;
        } hopState[IntHeader::maxHop];
    } power;



    struct
    {
        uint32_t m_lastUpdateSeq;
        DataRate m_curRate;
        uint32_t m_incStage;
        uint64_t lastRtt;
        double rttDiff;
    } tmly;

    struct
    {
        uint32_t m_lastUpdateSeq;
        uint32_t m_caState;
        uint32_t m_highSeq; // when to exit cwr
        double m_alpha;
        uint32_t m_ecnCnt;
        uint32_t m_batchSizeOfAlpha;
    } dctcp;

    struct
    {
        uint32_t m_lastUpdateSeq;
        DataRate m_curRate;
        uint32_t m_incStage;
    } hpccPint;

    struct
    {
        // seq num of packet which last triggered update
        uint32_t m_lastUpdateSeq;
        // last endpoint delay, stored for EWMA
        uint32_t m_lastEndpointDelay;
        // current rate
        DataRate m_curRate;
        // retransmit time count
        uint16_t m_retransmit_cnt;
        // time (nanosecond) of last Multiple Decrease
        Time m_t_last_decrease;
        // pacing delay, i.e. sending interval
        uint64_t m_pacing_delay;
        // real window size
        //
        // swift sets window size to INT_MAX to avoid packet capping during pacing (win<1)
        double m_real_win;
    } swift;

    struct
    {
        // There's no packet pacing in rtt-qcn; however m_win is uint32_t, which makes additive increase really
        // slow. Using a double to store real window helps a lot.
        double curr_win;
    } rttqcn;

    struct {
        uint64_t prev_rtt;// previous RTT, used to calculate gradient
        uint64_t last_update;// last time we update prev_rtt
    } powerqcn;

     struct
    {
        uint8_t recoveryState;
        //uint32_t sack_high;
        uint32_t targetRecoverySeq;
        uint32_t rightSeq;
        std::vector<int> bitMap;

    } test;

    /***********
     * methods
     **********/
    static TypeId GetTypeId(void);
    RdmaQueuePair(uint16_t pg,
                  Ipv4Address _sip,
                  Ipv4Address _dip,
                  uint16_t _sport,
                  uint16_t _dport);
    // Sets the size of the data to be transferred over this queue pair, which is crucial for
    // knowing when the data transfer is complete.
    void SetSize(uint64_t size);
    // Sets the size of the congestion window (cwnd), affecting how many packets can be on-the-fly
    // before waiting for acknowledgments.
    void SetWin(uint32_t win);
    // Sets the base Round-Trip Time (RTT), which can be used for calculating the optimal sending
    // rate
    // or adjusting the congestion window.
    void SetBaseRtt(uint64_t baseRtt);
    // Enables or disables the variable window size feature, potentially for experimental congestion
    // control algorithms.
    void SetVarWin(bool v);
    // Sets a callback function that will be called when the application-level transfer is finished,
    // allowing for cleanup or further actions.
    void SetAppNotifyCallback(Callback<void> notifyAppFinish);

    // Returns the amount of data left to send, which can be used to determine if the transfer is
    // complete.
    uint64_t GetBytesLeft() const;
    // Generates a hash value based on the queue pair's source and destination IP addresses and
    // ports,
    // likely used for efficiently looking up queue pairs.
    uint32_t GetHash(void) const;
    // Updates the highest sequence number acknowledged by the receiver, which is crucial for
    // tracking which packets have been successfully received and adjusting the congestion window
    // accordingly.
    void Acknowledge(uint64_t ack);
    // Calculates the number of packets (or bytes) that have been sent but not yet acknowledged,
    // useful for congestion control and flow control decisions.
    uint64_t GetOnTheFly() const;
    // Determines if the number of packets on-the-fly has reached the congestion window limit,
    // indicating whether it's necessary to pause sending further packets.
    bool IsWinBound() const;
    // For Swift CC: update pacing delay (if exists)
    void UpdatePacing();
    // Calculates the current effective window size, potentially adjusting for variable window
    // algorithms or rate-based congestion control.
    uint64_t GetWin() const; // window size calculated from m_rate
    // Checks if the data transfer for this queue pair is complete, which could be based on whether
    // all data has been acknowledged or a stop time has been reached.
    bool IsFinished() const;
    // A similar function to GetWin but specifically for HPCC (High Precision Congestion Control) or
    // another specialized congestion control algorithm, adjusting the window based on different
    // criteria.
    uint64_t HpGetCurWin() const; // window size calculated from hp.m_curRate, used by HPCC
    uint32_t incastFlow;
};

class RdmaRxQueuePair : public Object
{ // Rx side queue pair
  public:


    struct ECNAccount
    {
        uint16_t qIndex;
        uint8_t ecnbits;
        uint16_t qfb;
        uint16_t total;

        ECNAccount()
        {
            memset(this, 0, sizeof(ECNAccount));
        }
    };

    struct
    {
        uint32_t last_nack;
        uint32_t del_nack;
        uint8_t recoveryState;
        uint32_t sack_high;

        std::vector<int> bitMap;

    } test;

    ECNAccount m_ecn_source;
    uint32_t sip, dip;
    uint16_t sport, dport;
    uint16_t m_ipid;
    uint32_t ReceiverNextExpectedSeq;
    Time m_nackTimer;
    int32_t m_milestone_rx;
    uint32_t m_lastNACK;
    EventId QcnTimerEvent; // if destroy this rxQp, remember to cancel this timer

    static TypeId GetTypeId(void);
    RdmaRxQueuePair();
    uint32_t GetHash(void) const;
};

class RdmaQueuePairGroup : public Object
{
  public:
    std::vector<Ptr<RdmaQueuePair>> m_qps;
    // std::vector<Ptr<RdmaRxQueuePair> > m_rxQps;

    static TypeId GetTypeId(void);
    RdmaQueuePairGroup(void);
    uint32_t GetN(void) const;
    Ptr<RdmaQueuePair> Get(uint32_t idx);
    Ptr<RdmaQueuePair> operator[](uint32_t idx);
    void AddQp(Ptr<RdmaQueuePair> qp);
    // void AddRxQp(Ptr<RdmaRxQueuePair> rxQp);
    void Clear(void);
};

} // namespace ns3

#endif /* RDMA_QUEUE_PAIR_H */
