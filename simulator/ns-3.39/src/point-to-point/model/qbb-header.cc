#include "qbb-header.h"

#include "ns3/address-utils.h"
#include "ns3/buffer.h"
#include "ns3/log.h"

#include <cstdint>
#include <iostream>
#include <stdint.h>

NS_LOG_COMPONENT_DEFINE("qbbHeader");

namespace ns3
{

NS_OBJECT_ENSURE_REGISTERED(qbbHeader);

qbbHeader::qbbHeader(uint16_t pg)
    : sport(0),
      dport(0),
      flags(0),
      m_pg(pg),
      m_seq(0),
      m_epsn(0)
{
}

qbbHeader::qbbHeader()
    : sport(0),
      dport(0),
      flags(0),
      m_pg(0),
      m_seq(0),
      m_epsn(0)
{
}

qbbHeader::~qbbHeader()
{
}

void
qbbHeader::SetPG(uint16_t pg)
{
    m_pg = pg;
}

void
qbbHeader::SetSeq(uint32_t seq)
{
    m_seq = seq;
}

void
qbbHeader::SetEpsn(uint32_t epsn)
{
    m_epsn = epsn;
}

void
qbbHeader::SetSport(uint32_t _sport)
{
    sport = _sport;
}

void
qbbHeader::SetDport(uint32_t _dport)
{
    dport = _dport;
}

void
qbbHeader::SetTs(uint64_t ts)
{
    NS_ASSERT_MSG(IntHeader::mode == 1, "qbbHeader cannot SetTs when IntHeader::mode != 1");
    ih.ts = ts;
}

void
qbbHeader::SetCnp()
{
    flags |= 1 << FLAG_CNP;
}

void
qbbHeader::SetIntHeader(const IntHeader& _ih)
{
    ih = _ih;
}

void
qbbHeader::SetSwiftEndDelay(const uint64_t t4)
{
    auto swift = &(this->ih.swift);
    auto t2 = swift->remote_delay;
    swift->remote_delay = t4 - t2;
}

void
qbbHeader::SetSwiftSentTime(const uint64_t t1)
{
    auto swift = &(this->ih.swift);
    swift->ts = t1;
}

uint16_t
qbbHeader::GetPG() const
{
    return m_pg;
}

uint32_t
qbbHeader::GetSeq() const
{
    return m_seq;
}

uint32_t
qbbHeader::GetEpsn() const
{
    return m_epsn;
}

uint16_t
qbbHeader::GetSport() const
{
    return sport;
}

uint16_t
qbbHeader::GetDport() const
{
    return dport;
}

uint64_t
qbbHeader::GetTs() const
{
    NS_ASSERT_MSG(IntHeader::mode == 1, "qbbHeader cannot GetTs when IntHeader::mode != 1");
    return ih.ts;
}

uint8_t
qbbHeader::GetCnp() const
{
    return (flags >> FLAG_CNP) & 1;
}

TypeId
qbbHeader::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::qbbHeader").SetParent<Header>().AddConstructor<qbbHeader>();
    return tid;
}

TypeId
qbbHeader::GetInstanceTypeId(void) const
{
    return GetTypeId();
}

void
qbbHeader::Print(std::ostream& os) const
{
    os << "qbb:"
       << "pg=" << m_pg << ",seq=" << m_seq;
}

uint32_t
qbbHeader::GetSerializedSize(void) const
{
    return GetBaseSize() + IntHeader::GetStaticSize();
}

uint32_t
qbbHeader::GetBaseSize()
{
    qbbHeader tmp;
    return sizeof(tmp.sport) + sizeof(tmp.dport) + sizeof(tmp.flags) + sizeof(tmp.m_pg) +
           sizeof(tmp.m_seq) + sizeof(tmp.m_epsn);
}

void
qbbHeader::Serialize(Buffer::Iterator start) const
{
    Buffer::Iterator i = start;
    i.WriteU16(sport);
    i.WriteU16(dport);
    i.WriteU16(flags);
    i.WriteU16(m_pg);
    i.WriteU32(m_seq);
    i.WriteU32(m_epsn);

    // write IntHeader
    ih.Serialize(i);
}

uint32_t
qbbHeader::Deserialize(Buffer::Iterator start)
{
    Buffer::Iterator i = start;
    sport = i.ReadU16();
    dport = i.ReadU16();
    flags = i.ReadU16();
    m_pg = i.ReadU16();
    m_seq = i.ReadU32();
    m_epsn = i.ReadU32();

    // read IntHeader
    ih.Deserialize(i);
    return GetSerializedSize();
}
}; // namespace ns3
