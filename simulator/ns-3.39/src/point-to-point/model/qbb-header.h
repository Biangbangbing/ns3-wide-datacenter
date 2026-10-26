// yibo

#ifndef QBB_HEADER_H
#define QBB_HEADER_H

#include "ns3/buffer.h"
#include "ns3/header.h"
#include "ns3/int-header.h"

#include <cstdint>
#include <stdint.h>

namespace ns3
{

/**
 * \ingroup Pause
 * \brief Header for the Congestion Notification Message
 *
 * This class has two fields: The five-tuple flow id and the quantized
 * congestion level. This can be serialized to or deserialzed from a byte
 * buffer.
 */

class qbbHeader : public Header
{
  public:
    enum
    {
        FLAG_CNP = 0
    };

    qbbHeader(uint16_t pg);
    qbbHeader();
    virtual ~qbbHeader();

    // Setters
    /**
     * \param pg The PG
     */
    void SetPG(uint16_t pg);
    void SetSeq(uint32_t seq);
    void SetSport(uint32_t _sport);
    void SetDport(uint32_t _dport);
    void SetTs(uint64_t ts);
    void SetCnp();
    void SetIntHeader(const IntHeader& _ih);
    // Set swift endpoint delay duration, pass sending timestamp
    void SetSwiftEndDelay(uint64_t t4);
    // Set swift sending time t_sent (t1)
    void SetSwiftSentTime(uint64_t t1);
    // Selective Retransmit: receiver's current ReceiverNextExpectedSeq.
    // Carried on every ACK/SACK so the sender can fast-forward its bitmap
    // and snd_una whenever the receiver's ePSN has advanced past what the
    // bare seq field implies (e.g. when a hole-fill walked through the
    // hwBitMap and pushed ePSN through compressed regions).
    void SetEpsn(uint32_t epsn);

    // Getters
    /**
     * \return The pg
     */
    uint16_t GetPG() const;
    uint32_t GetSeq() const;
    uint16_t GetPort() const;
    uint16_t GetSport() const;
    uint16_t GetDport() const;
    uint64_t GetTs() const;
    uint8_t GetCnp() const;
    uint32_t GetEpsn() const;

    static TypeId GetTypeId(void);
    TypeId GetInstanceTypeId(void) const override;
    void Print(std::ostream& os) const override;
    uint32_t GetSerializedSize(void) const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;
    static uint32_t GetBaseSize(); // size without INT

  private:
    uint16_t sport, dport;
    uint16_t flags;
    uint16_t m_pg;
    uint32_t m_seq; // the qbb sequence number.
    uint32_t m_epsn; // SR: receiver's current ePSN at ack/sack time.
    IntHeader ih;
};

}; // namespace ns3

#endif /* QBB_HEADER */
