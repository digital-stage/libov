#include "ovboxclient.h"
#include <condition_variable>
#include <string.h>
#include <strings.h>
#if defined(WIN32) || defined(UNDER_CE)
#include <ws2tcpip.h>
// for ifaddrs.h
#include <iphlpapi.h>
#define MSG_CONFIRM 0

#elif defined(LINUX) || defined(linux) || defined(__APPLE__)
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

#include "errmsg.h"
#include <cmath>
#include <errno.h>

ovboxclient_t::ovboxclient_t(const std::string& desthost, port_t destport,
                             port_t recport, port_t portoffset, int prio,
                             secret_t secret, stage_device_id_t callerid,
                             bool peer2peer_, bool donotsend_,
                             bool downmixonly_, bool sendlocal_)
    : prio(prio), secret(secret), remote_server(secret, callerid),
      toport(destport), recport(recport), portoffset(portoffset),
      callerid(callerid), runsession(true), mode(0), cb_ping(nullptr),
      cb_ping_data(nullptr), sendlocal(sendlocal_), last_tx(0), last_rx(0),
      t_bitrate(std::chrono::high_resolution_clock::now()), cb_seqerr(nullptr),
      cb_seqerr_data(nullptr), msgbuffers(new msgbuf_t[MAX_STAGE_ID])
{
  if(peer2peer_)
    mode |= B_PEER2PEER;
  if(downmixonly_)
    mode |= B_DOWNMIXONLY;
  if(donotsend_)
    mode |= B_DONOTSEND;
  local_server.set_timeout_usec(10000);
  local_server.set_destination("localhost");
  local_server.bind(recport, true);
  remote_server.set_destination(desthost.c_str());
  remote_server.set_timeout_usec(5000);
  remote_server.bind(0, false);
  localep = getipaddr();
  localep.sin_port = remote_server.getsockep().sin_port;
  sendthread = std::thread(&ovboxclient_t::sendsrv, this);
  recthread = std::thread(&ovboxclient_t::recsrv, this);
  pingthread = std::thread(&ovboxclient_t::pingservice, this);
}

ovboxclient_t::~ovboxclient_t()
{
  runsession = false;
  sendthread.join();
  recthread.join();
  pingthread.join();
  for(auto th = xrecthread.begin(); th != xrecthread.end(); ++th)
    th->join();
  delete[] msgbuffers;
}

void ovboxclient_t::getbitrate(double& txrate, double& rxrate)
{
  std::chrono::high_resolution_clock::time_point t2(
      std::chrono::high_resolution_clock::now());
  std::chrono::duration<double> time_span(
      std::chrono::duration_cast<std::chrono::duration<double>>(t2 -
                                                                t_bitrate));
  double sc(8.0 / std::max(1e-6, time_span.count()));
  txrate = sc * (remote_server.tx_bytes - last_tx);
  rxrate = sc * (remote_server.rx_bytes - last_rx);
  t_bitrate = t2;
  last_tx = remote_server.tx_bytes;
  last_rx = remote_server.rx_bytes;
}

void ovboxclient_t::set_ping_callback(
    std::function<void(stage_device_id_t, double, const endpoint_t&, void*)> f,
    void* d)
{
  cb_ping = f;
  cb_ping_data = d;
}

void ovboxclient_t::set_seqerr_callback(
    std::function<void(stage_device_id_t, sequence_t, sequence_t, port_t,
                       void*)>
        f,
    void* d)
{
  cb_seqerr = f;
  cb_seqerr_data = d;
}

void ovboxclient_t::add_receiverport(port_t srcxport, port_t destxport)
{
  xrecthread.emplace_back(
      std::thread(&ovboxclient_t::xrecsrv, this, srcxport, destxport));
}

void ovboxclient_t::add_extraport(port_t dest)
{
  xdest.push_back(dest);
}

void ovboxclient_t::add_proxy_client(stage_device_id_t cid,
                                     const std::string& host)
{
  // resolve host:
  struct hostent* server;
  server = gethostbyname(host.c_str());
  if(server == NULL)
#if defined(WIN32) || defined(UNDER_CE)
    // windows:
    throw ErrMsg("No such host: " + std::to_string(WSAGetLastError()));
#else
    throw ErrMsg("No such host: " + std::string(hstrerror(h_errno)));
#endif
  endpoint_t serv_addr;
#if defined(WIN32) || defined(UNDER_CE)
  // windows:
  memset((char*)&serv_addr, 0, sizeof(serv_addr));
#else
  bzero((char*)&serv_addr, sizeof(serv_addr));
#endif
  serv_addr.sin_family = AF_INET;
  memcpy((char*)&serv_addr.sin_addr.s_addr, (char*)server->h_addr,
         server->h_length);
  proxyclients[cid] = serv_addr;
}

void ovboxclient_t::announce_new_connection(stage_device_id_t cid,
                                            const ep_desc_t& ep)
{
  if(cid == callerid)
    return;
  log(recport,
      "new connection for " + std::to_string(cid) + " from " + ep2str(ep.ep) +
          " in " + ((ep.mode & B_PEER2PEER) ? "peer-to-peer" : "server") +
          "-mode" + ((ep.mode & B_DOWNMIXONLY) ? " downmixonly" : "") +
          ((ep.mode & B_DONOTSEND) ? " donotsend" : "") + " v" + ep.version);
}

void ovboxclient_t::announce_connection_lost(stage_device_id_t cid)
{
  if(cid == callerid)
    return;
  log(recport, "connection for " + std::to_string(cid) + " lost.");
}

void ovboxclient_t::announce_latency(stage_device_id_t cid, double lmin,
                                     double lmean, double lmax,
                                     uint32_t received, uint32_t lost)
{
  if(cid == callerid)
    return;
  message_stat_t stat(sorter.get_stat(cid));
  message_stat_t ostat(stats[cid]);
  stats[cid] = stat;
  stat -= ostat;
  char ctmp[1024];
  sprintf(ctmp,
          "packages from %d received=%lu lost=%lu (%1.2f%%) seqerr=%lu "
          "recovered=%lu",
          cid, stat.received, stat.lost,
          100.0 * (double)stat.lost /
              (double)(std::max((size_t)1, stat.received + stat.lost)),
          stat.seqerr_in, stat.seqerr_in - stat.seqerr_out);
  log(recport, ctmp);
  std::vector<double> lat(pingstats_p2p[cid].get_min_med_99_mean_lost());
  sprintf(ctmp,
          "lat-p2p %d min=%1.2fms, median=%1.2fms, p99=%1.2fms mean=%1.2fms "
          "sent=%g received=%g",
          cid, lat[0], lat[1], lat[2], lat[3], lat[4], lat[5]);
  log(recport, ctmp);
  lat = pingstats_local[cid].get_min_med_99_mean_lost();
  sprintf(ctmp,
          "lat-loc %d min=%1.2fms, median=%1.2fms, p99=%1.2fms mean=%1.2fms "
          "sent=%g received=%g",
          cid, lat[0], lat[1], lat[2], lat[3], lat[4], lat[5]);
  log(recport, ctmp);
  lat = pingstats_srv[cid].get_min_med_99_mean_lost();
  sprintf(ctmp,
          "lat-srv %d min=%1.2fms, median=%1.2fms, p99=%1.2fms mean=%1.2fms "
          "sent=%g received=%g",
          cid, lat[0], lat[1], lat[2], lat[3], lat[4], lat[5]);
  log(recport, ctmp);
  double data[6];
  data[0] = cid;
  data[1] = lmin;
  data[2] = lmean;
  data[3] = lmax;
  data[4] = received;
  data[5] = lost;
  remote_server.pack_and_send(PORT_PEERLATREP, (const char*)data,
                              6 * sizeof(double));
}

void ovboxclient_t::handle_endpoint_list_update(stage_device_id_t cid,
                                                const endpoint_t& ep)
{
  DEBUG(cid);
  DEBUG(ep2str(ep));
}

// ping service
void ovboxclient_t::pingservice()
{
  while(runsession) {
    std::this_thread::sleep_for(std::chrono::milliseconds(PINGPERIODMS));
    // send registration to server:
    remote_server.send_registration(mode, toport, localep);
    // send ping to other peers:
    size_t ocid(0);
    for(auto ep : endpoints) {
      if(ep.timeout && (ocid != callerid)) {
        remote_server.send_ping(ep.ep, ocid);
        ++pingstats_p2p[ocid].sent;
        remote_server.send_ping(remote_server.get_destination(), ocid,
                                PORT_PING_SRV);
        ++pingstats_srv[ocid].sent;
        // test if peer is in same network:
        if((endpoints[callerid].ep.sin_addr.s_addr == ep.ep.sin_addr.s_addr) &&
           (ep.localep.sin_addr.s_addr != 0)) {
          remote_server.send_ping(ep.localep, ocid, PORT_PING_LOCAL);
          ++pingstats_local[ocid].sent;
        }
      }
      ++ocid;
    }
  }
}

// this thread receives messages from the server:
void ovboxclient_t::sendsrv()
{
  try {
    set_thread_prio(prio);
    msgbuf_t msg;
    while(runsession) {
      remote_server.recv_sec_msg(msg);
      msgbuf_t* pmsg(&msg);
      while(sorter.process(&pmsg))
        process_msg(*pmsg);
    }
  }
  catch(const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    runsession = false;
  }
}

void ovboxclient_t::process_ping_msg(msgbuf_t& msg)
{
  stage_device_id_t cid(msg.cid);
  msg_callerid(msg.rawbuffer) = callerid;
  switch(msg.destport) {
  case PORT_PING:
    // we received a ping message, so we just send it back as a pong
    // message and with our own stage device id:
    msg_port(msg.rawbuffer) = PORT_PONG;
    break;
  case PORT_PING_SRV:
    // we received a ping message via server so we just send it back as a pong
    // message and with our own stage device id:
    msg_port(msg.rawbuffer) = PORT_PONG_SRV;
    *((stage_device_id_t*)(msg.msg)) = cid;
    break;
  case PORT_PING_LOCAL:
    // we received a ping message, so we just send it back as a pong
    // message and with our own stage device id:
    msg_port(msg.rawbuffer) = PORT_PONG_LOCAL;
    break;
  }
  remote_server.send(msg.rawbuffer, msg.size + HEADERLEN, msg.sender);
}

void ovboxclient_t::process_pong_msg(msgbuf_t& msg)
{
  char* tbuf(msg.msg);
  size_t tsize(msg.size);
  ping_stat_t& stat(pingstats_p2p[msg.cid]);
  switch(msg.destport) {
  case PORT_PONG_SRV:
    tbuf += sizeof(stage_device_id_t);
    tsize -= sizeof(stage_device_id_t);
    stat = pingstats_srv[msg.cid];
    break;
  case PORT_PONG_LOCAL:
    stat = pingstats_local[msg.cid];
    break;
  }
  double tms(get_pingtime(tbuf, tsize));
  if(tms > 0) {
    if(cb_ping)
      cb_ping(msg.cid, tms, msg.sender, cb_ping_data);
    stat.add_value(tms);
  }
}

void ovboxclient_t::process_msg(msgbuf_t& msg)
{
  msg.valid = false;
  // avoid handling of loopback messages:
  if((msg.cid == callerid) && (msg.destport != PORT_LISTCID))
    return;
  // not a special port, thus we forward data to localhost and proxy
  // clients:
  if(msg.destport > MAXSPECIALPORT) {
    local_server.send(msg.msg, msg.size, msg.destport + portoffset);
    for(auto xd : xdest)
      local_server.send(msg.msg, msg.size, msg.destport + xd);
    // now send to proxy clients:
    for(auto client : proxyclients) {
      if(msg.cid != client.first) {
        client.second.sin_port = htons((unsigned short)msg.destport);
        remote_server.send(msg.msg, msg.size, client.second);
      }
    }
    return;
  }
  switch(msg.destport) {
  case PORT_PING:
  case PORT_PING_SRV:
  case PORT_PING_LOCAL:
    process_ping_msg(msg);
    break;
  case PORT_PONG:
  case PORT_PONG_SRV:
  case PORT_PONG_LOCAL:
    process_pong_msg(msg);
    break;
  case PORT_SETLOCALIP:
    // we received the local IP address of a peer:
    if(msg.size == sizeof(endpoint_t)) {
      cid_setlocalip(msg.cid, *((endpoint_t*)(msg.msg)));
    }
    break;
  case PORT_LISTCID:
    if(msg.size == sizeof(endpoint_t)) {
      // seq is peer2peer flag:
      cid_register(msg.cid, *((endpoint_t*)(msg.msg)), msg.seq, "");
    }
    break;
  }
}

// this thread receives local UDP messages and handles them:
void ovboxclient_t::recsrv()
{
  try {
    set_thread_prio(prio);
    char buffer[BUFSIZE];
    char msg[BUFSIZE];
    endpoint_t sender_endpoint;
    log(recport, "listening");
    while(runsession) {
      ssize_t n = local_server.recvfrom(buffer, BUFSIZE, sender_endpoint);
      if(n > 0) {
        size_t un = remote_server.packmsg(msg, BUFSIZE, recport, buffer, n);
        bool sendtoserver(!(mode & B_PEER2PEER));
        if(mode & B_PEER2PEER) {
          // we are in peer-to-peer mode.
          size_t ocid(0);
          for(auto ep : endpoints) {
            if(ep.timeout) {
              // endpoint is active.
              if(ocid != callerid) {
                // not sending to ourself.
                if(ep.mode & B_PEER2PEER) {
                  // other end is in peer-to-peer mode.
                  if(!(ep.mode & B_DONOTSEND)) {
                    // sending is not deactivated.
                    if(sendlocal &&
                       (endpoints[callerid].ep.sin_addr.s_addr ==
                        ep.ep.sin_addr.s_addr) &&
                       (ep.localep.sin_addr.s_addr != 0))
                      // same network.
                      remote_server.send(msg, un, ep.localep);
                    else
                      remote_server.send(msg, un, ep.ep);
                  }
                } else {
                  sendtoserver = true;
                }
              }
            }
            ++ocid;
          }
          // serve proxy clients:
          for(auto client : proxyclients) {
            // send unencoded message:
            client.second.sin_port = htons((unsigned short)recport);
            remote_server.send(buffer, n, client.second);
          }
        }
        if(sendtoserver) {
          remote_server.send(msg, un, toport);
        }
      }
    }
  }
  catch(const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    runsession = false;
  }
}

// this thread receives local UDP messages and handles them:
void ovboxclient_t::xrecsrv(port_t srcport, port_t destport)
{
  try {
    udpsocket_t xlocal_server;
    xlocal_server.set_timeout_usec(100000);
    xlocal_server.set_destination("localhost");
    xlocal_server.bind(srcport, true);
    set_thread_prio(prio);
    char buffer[BUFSIZE];
    char msg[BUFSIZE];
    endpoint_t sender_endpoint;
    log(recport, "listening");
    while(runsession) {
      ssize_t n = xlocal_server.recvfrom(buffer, BUFSIZE, sender_endpoint);
      if(n > 0) {
        size_t un = remote_server.packmsg(msg, BUFSIZE, destport, buffer, n);
        bool sendtoserver(!(mode & B_PEER2PEER));
        if(mode & B_PEER2PEER) {
          size_t ocid(0);
          for(auto ep : endpoints) {
            if(ep.timeout) {
              if((ocid != callerid) && (ep.mode & B_PEER2PEER) &&
                 (!(ep.mode & B_DONOTSEND))) {
                remote_server.send(msg, un, ep.ep);
              } else {
                sendtoserver = true;
              }
            }
            ++ocid;
          }
        }
        if(sendtoserver) {
          remote_server.send(msg, un, toport);
        }
      }
    }
  }
  catch(const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    runsession = false;
  }
}

bool message_sorter_t::process(msgbuf_t** ppmsg)
{
  if((*ppmsg)->valid) {
    msgbuf_t* pmsg(*ppmsg);
    // handle special ports separately:
    if(pmsg->destport <= MAXSPECIALPORT) {
      pmsg->valid = false;
      return true;
    }
    // we received a message, check for sequence order
    ++stat[pmsg->cid].received;
    bool notfirst(seq_in[pmsg->cid].find(pmsg->destport) !=
                  seq_in[pmsg->cid].end());
    // get input sequence difference:
    sequence_t dseq_in(deltaseq(seq_in, *pmsg));
    sequence_t dseq_io(deltaseq_const(seq_out, *pmsg));
    if((dseq_in != 0) && notfirst)
      stat[pmsg->cid].lost += dseq_in - 1;
    // dropout:
    if((dseq_in > 1) && (dseq_io > 1)) {
      buf1.copy(*pmsg);
      (*ppmsg)->valid = false;
      return false;
    }
    stat[pmsg->cid].seqerr_in += (dseq_in < 0);
    if((dseq_in < -1) || ((dseq_io > 1) && (dseq_in > 0))) {
      if(buf1.valid && (buf1.cid == pmsg->cid) &&
         (buf1.destport == pmsg->destport) && (buf1.seq < pmsg->seq)) {
        buf2.copy(*pmsg);
        *ppmsg = &buf1;
        buf1.valid = false;
        sequence_t dseq_out(deltaseq(seq_out, buf1));
        stat[pmsg->cid].seqerr_out += (dseq_out < 0);
        return true;
      }
    }
    sequence_t dseq_out(deltaseq(seq_out, *pmsg));
    pmsg->valid = false;
    stat[pmsg->cid].seqerr_out += (dseq_out < 0);
    return true;
  }
  if(buf1.valid) {
    *ppmsg = &buf1;
    sequence_t dseq_out(deltaseq(seq_out, buf1));
    buf1.valid = false;
    stat[buf1.cid].seqerr_out += (dseq_out < 0);
    return true;
  }
  if(buf2.valid) {
    sequence_t dseq_out(deltaseq(seq_out, buf2));
    *ppmsg = &buf2;
    buf2.valid = false;
    stat[buf2.cid].seqerr_out += (dseq_out < 0);
    return true;
  }
  return false;
}

message_stat_t::message_stat_t()
    : received(0u), lost(0u), seqerr_in(0u), seqerr_out(0u)
{
}

void message_stat_t::reset()
{
  received = 0u;
  lost = 0u;
  seqerr_in = 0u;
  seqerr_out = 0u;
}

void message_stat_t::operator+=(const message_stat_t& src)
{
  received += src.received;
  lost += src.lost;
  seqerr_in += src.seqerr_in;
  seqerr_out += src.seqerr_out;
}

void message_stat_t::operator-=(const message_stat_t& src)
{
  received -= src.received;
  lost -= src.lost;
  seqerr_in -= src.seqerr_in;
  seqerr_out -= src.seqerr_out;
}

message_stat_t message_sorter_t::get_stat(stage_device_id_t id)
{
  return stat[id];
}

ping_stat_t::ping_stat_t(size_t N)
    : sent(0), received(0), data(N, 0.0), idx(0), filled(0), sum(0.0)
{
}

void ping_stat_t::add_value(double pt)
{
  ++received;
  sum -= data[idx];
  data[idx] = pt;
  sum += pt;
  ++idx;
  if(idx >= data.size())
    idx = 0;
  if(filled < data.size())
    ++filled;
}

std::vector<double> ping_stat_t::get_min_med_99_mean_lost() const
{
  if(!filled)
    return {0.0, 0.0, 0.0, 0.0, (double)sent, (double)received};
  std::vector<double> sb(data);
  sb.resize(filled);
  std::sort(sb.begin(), sb.end());
  size_t idx_med(std::round(0.5 * (filled - 1)));
  size_t idx_99(std::round(0.99 * (filled - 1)));
  double med(sb[idx_med]);
  if((filled & 1) == 0) {
    // even number of samples, median is mean of two neighbours
    if(idx_med)
      med += sb[idx_med - 1];
    else
      med += sb[idx_med + 1];
    med *= 0.5;
  }
  return {sb[0], med, sb[idx_99], sum / filled, (double)sent, (double)received};
}

/*
 * Local Variables:
 * compile-command: "make -C .."
 * End:
 */
