#include "wrapping_integers.hh"
#include <iostream>
#include <cstdlib>
using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  // Your code here.
  return Wrap32 { zero_point + n };
}

// covert seqno -> absolute sequence number
uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  // Your code here.
  uint64_t k = checkpoint >> 32;
  uint64_t offset = static_cast<uint64_t>(this->raw_value_ - zero_point.raw_value_);
  uint64_t asn1 = k * (1UL<<32) + offset;
  uint64_t asn2 = k==0?asn1:(k-1) * (1UL<<32) + offset;
  uint64_t asn3 = (k+1) * (1UL<<32) + offset;
  uint64_t abs1 = asn1 >= checkpoint? asn1-checkpoint:checkpoint-asn1;
  uint64_t abs2 = asn2 >= checkpoint? asn2-checkpoint:checkpoint-asn2;
  uint64_t abs3 = asn3 >= checkpoint? asn3-checkpoint:checkpoint-asn3;
  uint64_t minAbs = min( min(abs1, abs2), abs3);
  if (minAbs == abs1) return asn1;
  if (minAbs == abs2) return asn2;
  if (minAbs == abs3) return asn3;
  return {};
}
