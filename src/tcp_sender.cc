#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return lastSent - AckNo;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  // Your code here.
  return consecutiveRetransmissionNum;
}

void TCPSender::push( const TransmitFunction& transmit )
{
    TCPSenderMessage msg = make_empty_message();
    if(state == CLOSED){
        retransmissionQueue = {};
        msg.SYN = true;
        state = SYN_SENT;
        lastSent = msg.sequence_length();
        retransmissionQueue.emplace(0, msg);
        if(input_.reader().is_finished()){
            msg.FIN = true;
            lastSent++;
        }
        transmit(msg);
    }
    while(state == ESTABLISHED){
        const auto &str_view = input_.reader().peek();
        uint16_t m_window_size = (window_size == 0) ? 1 : window_size;
        uint16_t wnd_remain = AckNo + m_window_size - lastSent;
        uint16_t dataSize = std::min(std::min((uint16_t)str_view.size(), wnd_remain), (uint16_t)TCPConfig::MAX_PAYLOAD_SIZE);
        if(msg.RST){
            state = RST;
        }
        if(dataSize > 0 || input_.reader().is_finished() || msg.RST){
            std::string payload(str_view.substr(0, dataSize));
            uint64_t data_seqno = input_.reader().bytes_popped() + 1;
            input_.reader().pop(dataSize);
            lastSent = input_.reader().bytes_popped() + 1;
            msg.seqno = Wrap32::wrap(data_seqno, isn_);
            msg.payload = payload;
            if(dataSize > 0 || ((input_.reader().is_finished()) && wnd_remain > 0) || msg.RST){

                if(input_.reader().is_finished() && lastSent < AckNo + m_window_size){
                    state = FIN_WAIT1;
                    msg.FIN = true;
                    lastSent++;
                }
                retransmissionQueue.emplace(data_seqno, msg);
                transmit(msg);
                if(msg.RST)
                    return;
            }else{
                break;
            }
        }else{
            break;
        }
    }
    if(msg.sequence_length() > 0){
        timer_running = true;
    }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
    TCPSenderMessage msg = { .seqno = Wrap32::wrap( lastSent, isn_ ),
            .SYN = false,
            .payload = string(),
            .FIN = false,
            .RST = input_.has_error() };
    return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size = msg.window_size;
  if(msg.RST){
      state = RST;
      input_.set_error();
  }

  if(state == SYN_SENT && msg.ackno.has_value() &&
    msg.ackno.value().unwrap(isn_, AckNo) == 1){
      state = ESTABLISHED;
      AckNo = 1;
      if(retransmissionQueue.front().second.seqno == isn_){
          retransmissionQueue.pop();
      }
  }else if(state == ESTABLISHED || state == FIN_WAIT1){
      uint64_t msg_ackno = msg.ackno->unwrap(isn_, AckNo);
      if(AckNo < msg_ackno){
          AckNo = msg_ackno;
          RTO_ms = initial_RTO_ms_;
          timer = 0;
      }
      if(!retransmissionQueue.empty()){
          uint64_t oldseqno = retransmissionQueue.front().first;
          TCPSenderMessage oldmsg = retransmissionQueue.front().second;
          while(oldseqno + oldmsg.sequence_length() <= AckNo){
              consecutiveRetransmissionNum = 0;
              retransmissionQueue.pop();
              if(retransmissionQueue.empty())
                  break;
              oldseqno = retransmissionQueue.front().first;
              oldmsg = retransmissionQueue.front().second;
          }
      }
  }
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
    if(timer_running)
        timer += ms_since_last_tick;
    if(timer >= RTO_ms && !retransmissionQueue.empty()){
        TCPSenderMessage oldmsg = retransmissionQueue.front().second;
        transmit(oldmsg);
        if(((state == ESTABLISHED || state == FIN_WAIT1) && window_size > 0) || state == SYN_SENT){
            consecutiveRetransmissionNum++;
        }
        if(window_size != 0)
            RTO_ms = RTO_ms << 1;
        timer = 0;
    }
}
