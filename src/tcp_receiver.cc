#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
    if(message.RST){
        reassembler_.reader().set_error();
    }
    if(message.SYN){
        SenderISN = message.seqno;
        sync_ = true;
        AckNo = 1;
    }

    if(sync_){
        uint64_t streamIdx = message.seqno.unwrap(SenderISN, reassembler_.writer().bytes_pushed());
        if(!message.SYN)
            streamIdx--;
        uint64_t bytes_pushed = reassembler_.writer().bytes_pushed();
        reassembler_.insert(streamIdx, message.payload, message.FIN);
        uint64_t offset = reassembler_.writer().bytes_pushed() - bytes_pushed;
        AckNo += offset;
        if(reassembler_.writer().writer().is_closed())
            AckNo++;
    }
}

TCPReceiverMessage TCPReceiver::send() const
{
      TCPReceiverMessage msg;
      msg.window_size = std::min(reassembler_.writer().available_capacity(), (uint64_t)UINT16_MAX);
      if(sync_)
          msg.ackno = Wrap32::wrap(AckNo, SenderISN);
      msg.RST = reassembler_.writer().has_error() || reassembler_.reader().has_error();
      return msg;
}
