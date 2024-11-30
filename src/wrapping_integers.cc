#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
    return Wrap32(
            zero_point.raw_value_ + n);
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
    if(raw_value_ < zero_point.raw_value_){
        if(checkpoint < raw_value_ + ((uint64_t)1 << 32)){
            return raw_value_ + ((uint64_t)1 << 32) - zero_point.raw_value_;
        }else{
            return ((checkpoint + zero_point.raw_value_+ ((uint64_t)1 << 31) - raw_value_) & ((uint64_t)0xFFFFFFFF << 32)) + raw_value_ - zero_point.raw_value_;
        }
    }else{
        if(checkpoint < raw_value_){
            return raw_value_ - zero_point.raw_value_;
        }else{
            return ((checkpoint + zero_point.raw_value_ + ((uint64_t)1 << 31) - raw_value_) & ((uint64_t)0xFFFFFFFF << 32)) + raw_value_ - zero_point.raw_value_ ;
        }
    }
}
