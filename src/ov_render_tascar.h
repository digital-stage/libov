#ifndef OV_RENDER_TASCAR
#define OV_RENDER_TASCAR

#include "../tascar/libtascar/include/session.h"
#include "ov_tools.h"
#include "ovboxclient.h"
#include "spawn_process.h"
#include <lo/lo.h>

class ov_render_tascar_t : public ov_render_base_t {
public:
  ov_render_tascar_t(const std::string& deviceid, port_t pinglogport_);
  ~ov_render_tascar_t();
  void start_session();
  void end_session();
  void clear_stage();
  void start_audiobackend();
  void stop_audiobackend();
  void add_stage_device(const stage_device_t& stagedevice);
  void rm_stage_device(stage_device_id_t stagedeviceid);
  void set_stage(const std::map<stage_device_id_t, stage_device_t>&);
  void set_stage_device_gain(stage_device_id_t stagedeviceid, double gain);
  void set_stage_device_channel_gain(stage_device_id_t stagedeviceid,
                                     device_channel_id_t channeldeviceid,
                                     double gain);
  void set_stage_device_channel_position(stage_device_id_t stagedeviceid,
                                         device_channel_id_t channeldeviceid,
                                         pos_t position,
                                         zyx_euler_t orientation);
  void set_render_settings(const render_settings_t& rendersettings,
                           stage_device_id_t thisstagedeviceid);
  std::string get_stagedev_name(stage_device_id_t stagedeviceid) const;
  void getbitrate(double& txrate, double& rxrate);
  std::vector<std::string> get_input_channel_ids() const;
  double get_load() const;
  void set_extra_config(const std::string&);
  class metronome_t {
  public:
    metronome_t();
    metronome_t(const nlohmann::json& js);
    bool operator!=(const metronome_t& a);
    void set_xmlattr(xmlpp::Element* em, xmlpp::Element* ed) const;
    void update_osc(TASCAR::osc_server_t* srv, const std::string& dev) const;

    uint32_t bpb;
    double bpm;
    bool bypass;
    double delay;
    double level;
  };

private:
  void create_virtual_acoustics(xmlpp::Element* session, xmlpp::Element* e_rec,
                                xmlpp::Element* e_scene);
  void create_raw_dev(xmlpp::Element* session);
  // for the time being we (optionally if jack is chosen as an audio
  // backend) start the jack backend. This will be replaced by a more
  // generic audio backend interface:
  spawn_process_t* h_jack;
  // for the time being we start the webmixer as a local nodejs server
  // on port 8080. This will be replaced (or extended) by a web mixer
  // on the remote configuration interface:
  spawn_process_t* h_webmixer;
  TASCAR::session_t* tascar;
  ovboxclient_t* ovboxclient;
  port_t pinglogport;
  lo_address pinglogaddr;
  std::vector<std::string> inputports;
  double headtrack_tauref;
  // self-monitor delay in milliseconds:
  double selfmonitor_delay;
  // metronome settings:
  metronome_t metronome;
  const std::string zita_path;
  /**
     \brief serve as proxy

     A device which serves as proxy will send forwarded packages
     not to localhost, but to a multicast group instead.
   */
  bool is_proxy;
  /**
     \brief use a proxy

     A device which uses a proxy will not receive packages from a
     server or peers, but will listen to a multicast group

  */
  bool use_proxy;
};

#endif

/*
 * Local Variables:
 * mode: c++
 * compile-command: "make -C .."
 * End:
 */
