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
    IntHeader ih;
};

}; // namespace ns3

#endif /* QBB_HEADER */
