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
#include <set>
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
        uint32_t recoveryTag;
        uint32_t rightSeq;
        std::vector<int> bitMap;
        Time last_updateTime;
        double last_updateInterval;

    } test;

    /*
     * Selective Retransmission - sender-side state.
     * - hwBitMap: hardware bitmap of K bits (indexed by (PSN-eACK)/MTU)
     * - swBitMap: software bitmap (variable size, anchored at soft_eACK)
     * - SendOutOfOrder: true when the sender has detected loss and is in SR mode
     * - sackHigh: highest acked PSN seen (acts as receiveHigh on rx side)
     * - soft_eACK: software-side first hole; lives ahead of (eACK + K)
     * - eACK is encoded by snd_una/MTU; PSN is byte offset measured in MTU units.
     * - sendLeft / sendRight: tracker bounds used while waiting for sw update
     * - swPending: true if a software request is in-flight
     * - retransQueue: PSNs (byte units) flagged as lost and waiting for resend
     */
    struct
    {
        bool        SendOutOfOrder;
        std::vector<int> hwBitMap;   // size K once initialized
        std::vector<int> swBitMap;   // dynamic size
        uint32_t    sackHigh;
        uint32_t    soft_eACK;       // first hole as seen by software
        uint32_t    sendLeft;
        uint32_t    sendRight;
        bool        swPending;       // a soft update has been requested
        // Physical anchor PSN of hwBitMap[0]. Equals snd_una when no sw update
        // is in flight; frozen at the snd_una value at the moment swPending
        // was set, so that the merge in SrTxApplySwUpdate can align swBitMap
        // and hwBitMap regardless of intervening eACK advances.
        uint32_t    hwAnchor;
        std::vector<uint32_t> retransQueue; // PSNs to retransmit (in byte units)
        // Async-ingest queue: PSNs received from sack/ack while sw is busy.
        // Drained when SrTxApplySwUpdate runs.
        std::vector<uint32_t> swPendingQueue;
        uint64_t    swDeadlineNs;    // earliest time the sw update will land
        // FACK-style accounting: total SACKed bytes in [snd_una, snd_nxt) that
        // have been individually acknowledged (hwBitMap=1, RaLL compressed
        // range, or swBitMap=1). Subtracted from OnTheFly in IsWinBound so
        // cwnd doesn't double-count packets the receiver already has.
        uint64_t    sackedBytes;
        // RTO (retransmission timeout) for FACK. Without this, a retrans that
        // is itself dropped is never retried (no event re-enqueues it). The
        // timer fires 2*baseRTT after the last ACK/SACK; on a true timeout it
        // rescans [snd_una, snd_nxt) for unacked PSNs and pushes them to
        // retransQueue. Tail-loss of the final packet (no later send to
        // trigger a SACK) also relies on this.
        Time     lastAckTime;
        EventId  rtoTimer;
        // Fast-retransmit throttle: snd_una and time of the last fast-retrans
        // push so that bursts of SACKs for the same hole don't enqueue
        // duplicates.
        uint32_t lastFastRetransPsn;
        Time     lastFastRetransTime;
        // SRNIC-only: set of PSNs already popped from retransQueue and sent
        // on the wire, awaiting receiver confirmation. Without this tracker,
        // every fresh SACK triggers a full bitmap scan that re-pushes any
        // PSN whose retransmit hasn't yet been ACKed (bit still 0). Cleared
        // by SrnicTxApplyOoO when the matching SACK arrives, and by
        // SrnicTxHandleAck when snd_una advances past the entry.
        std::set<uint32_t> srnicInflightRetx;
    } sr;

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

    //
    bool ShouldSendPacketForSR() const;
    // True iff there are queued retransmissions waiting to be drained.
    bool HasRetransPending() const;
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

    /*
     * Selective Retransmission - receiver-side state.
     * - hwBitMap: hardware bitmap of K bits anchored at ePSN.
     *   bit i represents the packet with PSN = ePSN + i*MTU.
     * - swBitMap: software bitmap (variable size, anchored at soft_ePSN).
     * - ReceiveOutOfOrder: true once a hole appears.
     * - receiveHigh: highest received PSN.
     * - soft_ePSN: first hole as seen by software (ePSN <= soft_ePSN <= ...).
     * - recvLeft / recvRight: tracker bounds used while waiting for sw update.
     * - swPending: true while a sw update request is pending.
     */
    struct
    {
        bool ReceiveOutOfOrder;
        std::vector<int> hwBitMap;   // size K once initialized
        std::vector<int> swBitMap;   // dynamic size
        uint32_t receiveHigh;
        uint32_t soft_ePSN;          // first hole as seen by software
        uint32_t recvLeft;
        uint32_t recvRight;
        bool     swPending;
        // Physical anchor PSN of hwBitMap[0]. Equals ePSN when no sw update
        // is in flight; frozen at the ePSN value at the moment swPending was
        // set, so that the merge in SrRxApplySwUpdate can align swBitMap and
        // hwBitMap regardless of intervening ePSN advances.
        uint32_t hwAnchor;
        // Async-ingest queue (used while a sw update is in-flight)
        std::vector<uint32_t> swPendingQueue;
        uint64_t swDeadlineNs;       // earliest time the sw update will land
        // SRNIC fast-exit tracker (Fig 8). Highest PSN P such that we have
        // synchronously observed PSNs ePSN+1, ePSN+2, ..., P arrive (in any
        // order). Updated in the OOO branch of SrnicRxHandlePacket so it is
        // immune to swBitMap async lag. On the in-order transition the
        // receiver leaps ePSN past the entire run instead of waiting for
        // each ApplyOoO to fire incrementally.
        uint32_t srnicSeqHigh;
    } sr;

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
