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
    while(!has_FIN){
        TCPSenderMessage msg = make_empty_message();
        if(not sync_){
            sync_ = true;
            retransmissionQueue = {};
            msg.SYN = true;
            has_FIN = false;
            timer = 0;
            lastSent = 0;
        }
        const auto &str_view = input_.reader().peek();
        uint16_t m_window_size = (window_size == 0) ? 1 : window_size;
        uint16_t wnd_remain = AckNo + m_window_size - lastSent - msg.SYN;
        uint16_t dataSize = std::min(std::min((uint16_t)str_view.size(), wnd_remain), (uint16_t)TCPConfig::MAX_PAYLOAD_SIZE);
        std::string payload(str_view.substr(0, dataSize));
        input_.reader().pop(dataSize);
        msg.payload = payload;
        //FIN and lastSent handling
        if(input_.reader().is_finished() and wnd_remain - dataSize > 0){
            msg.FIN = true;
            has_FIN = true;
        }
        lastSent += msg.sequence_length();
        if(msg.sequence_length() > 0){
            timer_running = true;
            if(retransmissionQueue.empty())
                timer = 0;
            retransmissionQueue.emplace(msg.seqno.unwrap(isn_, input_.reader().bytes_popped()), msg);
            transmit(msg);
        }else{
            if(msg.RST)
                transmit(msg);
            return;
        }
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
    if(msg.ackno.has_value() and msg.ackno->unwrap(isn_, input_.reader().bytes_popped()) > lastSent)
        return; //reject impossible ackno
    window_size = msg.window_size;
    if(msg.ackno.has_value()){
        uint64_t msg_ackNo = msg.ackno->unwrap(isn_, AckNo);
        if(msg_ackNo > AckNo){
            AckNo = msg_ackNo;
            timer = 0;
            RTO_ms = initial_RTO_ms_;
        }
    }
    if(msg.RST){
         input_.set_error();
    }
    if(!retransmissionQueue.empty()) {
        uint64_t oldseqno = retransmissionQueue.front().first;
        TCPSenderMessage oldmsg = retransmissionQueue.front().second;
        while (oldseqno + oldmsg.sequence_length() <= AckNo) {
            consecutiveRetransmissionNum = 0;
            retransmissionQueue.pop();
            if(retransmissionQueue.empty())
                break;
            oldseqno = retransmissionQueue.front().first;
            oldmsg = retransmissionQueue.front().second;
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
        if(window_size > 0){
            consecutiveRetransmissionNum++;
        }
        if(window_size != 0)
            RTO_ms = RTO_ms << 1;
        timer = 0;
    }
}
