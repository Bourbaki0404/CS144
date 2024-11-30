#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
    if(message.RST){
        reassembler_.reader().set_error();
        state = LISTEN;
    }
    if(message.SYN && state == LISTEN){
        SenderISN = message.seqno;
        SenderSN = message.sequence_length();
        state = SYN_RECEIVED;
        unassembledIdx = 0;
        uint64_t currentIdx = message.seqno.unwrap(SenderISN, unassembledIdx);
        reassembler_.insert(currentIdx, message.payload, message.FIN);
    }else if(state == SYN_RECEIVED){
        if(!(message.SYN))
            state = ESTABLISHED;
    }
    if(state == ESTABLISHED || state == WAIT_CLOSE){
        uint64_t currentIdx = message.seqno.unwrap(SenderISN, unassembledIdx) - SenderSN + unassembledIdx;
        uint64_t bytes_pushed = reassembler_.writer().bytes_pushed();
        reassembler_.insert(currentIdx, message.payload, message.FIN);
        unassembledIdx += reassembler_.writer().bytes_pushed() - bytes_pushed;
        SenderSN += reassembler_.writer().bytes_pushed() - bytes_pushed;
        if(message.FIN){
            state = WAIT_CLOSE;
        }
        if(state == WAIT_CLOSE && reassembler_.writer().is_closed())
            state = CLOSED;
    }
}

TCPReceiverMessage TCPReceiver::send() const
{
      // Your code here.
      TCPReceiverMessage msg;
      msg.window_size = std::min(reassembler_.writer().available_capacity(), (uint64_t)UINT16_MAX);
      if(state == ESTABLISHED || state == SYN_RECEIVED)
          msg.ackno = Wrap32::wrap(SenderSN, SenderISN);
      if(state == WAIT_CLOSE || state == CLOSED){
          if(reassembler_.writer().is_closed())
            msg.ackno = Wrap32::wrap(SenderSN + 1, SenderISN);
          else
            msg.ackno = Wrap32::wrap(SenderSN, SenderISN);
      }
      msg.RST = reassembler_.writer().has_error() || reassembler_.reader().has_error();
      return msg;
}
