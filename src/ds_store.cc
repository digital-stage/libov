#include "ds_store.h"
#include <iostream>

ds::ds_store_t::ds_store_t() {}

ds::ds_store_t::~ds_store_t() {}

void ds::ds_store_t::createStage(const nlohmann::json stage)
{
  auto lock = std::unique_lock<std::mutex>(this->stages_mutex_);
  const std::string _id = stage["_id"].get<std::string>();
  this->stages_[_id] = stage;
}

void ds::ds_store_t::updateStage(const std::string& stageId,
                                 const nlohmann::json update)
{
  auto lock = std::unique_lock<std::mutex>(this->stages_mutex_);
  this->stages_[stageId].merge_patch(update);
}

boost::optional<const ds::json::stage_t>
ds::ds_store_t::readStage(const std::string& stageId)
{
  auto lock = std::unique_lock<std::mutex>(this->stages_mutex_);
  if(!(this->stages_.count(stageId) > 0))
    return this->stages_[stageId].get<ds::json::stage_t>();
  return boost::none;
}

void ds::ds_store_t::removeStage(const std::string& stageId)
{
  auto lock = std::unique_lock<std::mutex>(this->stages_mutex_);
  this->stages_.erase(stageId);
}

void ds::ds_store_t::createGroup(const nlohmann::json group)
{
  auto lock = std::unique_lock<std::mutex>(this->groups_mutex_);
  const std::string _id = group["_id"].get<std::string>();
  this->groups_[_id] = group;
}

void ds::ds_store_t::updateGroup(const std::string& id,
                                 const nlohmann::json update)
{
  auto lock = std::unique_lock<std::mutex>(this->groups_mutex_);
  this->groups_[id].merge_patch(update);
}

boost::optional<const ds::json::group_t>
ds::ds_store_t::readGroup(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->groups_mutex_);
  if(!(this->groups_.count(id) > 0))
    return this->groups_[id].get<ds::json::group_t>();
  return boost::none;
}

void ds::ds_store_t::removeGroup(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->groups_mutex_);
  this->groups_.erase(id);
}

const std::string& ds::ds_store_t::getCurrentStageId()
{
  auto lock = std::unique_lock<std::mutex>(this->current_stage_id_mutex_);
  return this->currentStageId_;
}

void ds::ds_store_t::setCurrentStageId(const std::string& stageId)
{
  auto lock = std::unique_lock<std::mutex>(this->current_stage_id_mutex_);
  this->currentStageId_ = stageId;
  // Also update current stage member
}

void ds::ds_store_t::setLocalDevice(const nlohmann::json localDevice)
{
  auto lock = std::unique_lock<std::mutex>(this->local_device_mutex_);
  this->localDevice_ = localDevice;
}

boost::optional<const ds::json::device_t> ds::ds_store_t::getLocalDevice()
{
  auto lock = std::unique_lock<std::mutex>(this->local_device_mutex_);
  if(!(this->localDevice_.is_null())){
    return this->localDevice_.get<ds::json::device_t>();
  }
  return boost::none;
}

void ds::ds_store_t::updateLocalDevice(const nlohmann::json update)
{
  auto lock = std::unique_lock<std::mutex>(this->local_device_mutex_);
  this->localDevice_.merge_patch(update);
}

void ds::ds_store_t::setLocalUser(const nlohmann::json localUser)
{
  auto lock = std::unique_lock<std::mutex>(this->local_user_mutex_);
  this->localUser_ = localUser;
}

boost::optional<const ds::json::user_t> ds::ds_store_t::getLocalUser()
{
  auto lock = std::unique_lock<std::mutex>(this->local_user_mutex_);
  if(!(this->localUser_.is_null()))
    return this->localUser_.get<ds::json::user_t>();
  return boost::none;
}

void ds::ds_store_t::removeStages()
{
  auto lock = std::unique_lock<std::mutex>(this->stages_mutex_);
  this->stages_.clear();
}

void ds::ds_store_t::removeGroups()
{
  auto lock = std::unique_lock<std::mutex>(this->groups_mutex_);
  this->groups_.clear();
}

void ds::ds_store_t::createCustomGroup(const nlohmann::json customGroup)
{
  auto lock = std::unique_lock<std::mutex>(this->custom_groups_mutex_);
  const std::string _id = customGroup["_id"].get<std::string>();
  this->customGroups_[_id] = customGroup;
  this->customGroupIdByGroupId_[customGroup["groupId"]] = _id;
}

void ds::ds_store_t::updateCustomGroup(const std::string& id,
                                       const nlohmann::json update)
{
  auto lock = std::unique_lock<std::mutex>(this->custom_groups_mutex_);
  this->customGroups_[id].merge_patch(update);
}

boost::optional<const ds::json::custom_group_t>
ds::ds_store_t::readCustomGroup(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->custom_groups_mutex_);
  if(!(this->customGroups_.count(id) > 0))
    return this->customGroups_[id].get<ds::json::custom_group_t>();
  return boost::none;
}

void ds::ds_store_t::removeCustomGroup(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->custom_groups_mutex_);
  const std::string groupId = this->customGroups_[id]["groupId"];
  this->customGroups_.erase(id);
  this->customGroupIdByGroupId_.erase(groupId);
}

void ds::ds_store_t::removeCustomGroups()
{
  auto lock = std::unique_lock<std::mutex>(this->custom_groups_mutex_);
  this->customGroups_.clear();
  this->customGroupIdByGroupId_.clear();
}

void ds::ds_store_t::createStageMember(const nlohmann::json stageMember)
{
  auto lock = std::unique_lock<std::mutex>(this->stage_members_mutex_);
  const std::string _id = stageMember["_id"].get<std::string>();
  this->stageMembers_[_id] = stageMember;
  this->stageMemberIdsByStageId_[stageMember["stageId"]].push_back(_id);
  const auto localUser = this->getLocalUser();
  if(localUser && localUser->_id == stageMember["userId"]) {
    std::cout << "FOUND CURRENT STAGE MEMBER!" << std::endl;
    this->currentStageMemberId_ = _id;
  }
}

void ds::ds_store_t::updateStageMember(const std::string& id,
                                       const nlohmann::json update)
{
  auto lock = std::unique_lock<std::mutex>(this->stage_members_mutex_);
  this->stageMembers_[id].merge_patch(update);
}

boost::optional<const ds::json::stage_member_t>
ds::ds_store_t::readStageMember(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->stage_members_mutex_);
  if(this->stageMembers_.count(id) > 0) {
    return this->stageMembers_[id].get<ds::json::stage_member_t>();
  }
  return boost::none;
}

const std::vector<const ds::json::stage_member_t>
ds::ds_store_t::readStageMembersByStage(const std::string& stageId)
{
  auto lock = std::unique_lock<std::mutex>(this->stage_members_mutex_);
  std::vector<const ds::json::stage_member_t> stageMembers;
  if(this->stageMemberIdsByStageId_.count(stageId) > 0) {
    for(const auto& stageMemberId : this->stageMemberIdsByStageId_[stageId]) {
      const auto stageMember = this->readStageMember(stageMemberId);
      if(stageMember) {
        stageMembers.push_back(stageMember.get());
      }
    }
  }
  return stageMembers;
}

void ds::ds_store_t::removeStageMember(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->stage_members_mutex_);
  const std::string stageId = this->stageMembers_[id]["stageId"];
  const std::string userId = this->stageMembers_[id]["userId"];
  this->stageMembers_.erase(id);
  std::remove(this->stageMemberIdsByStageId_[stageId].begin(),
              this->stageMemberIdsByStageId_[stageId].end(), id);
  const auto localUser = this->getLocalUser();
  if(localUser && localUser->_id == userId) {
    this->currentStageMemberId_.clear();
  }
}

void ds::ds_store_t::removeStageMembers()
{
  auto lock = std::unique_lock<std::mutex>(this->stage_members_mutex_);
  this->stageMembers_.clear();
  this->stageMemberIdsByStageId_.clear();
}

void ds::ds_store_t::createCustomStageMember(
    const nlohmann::json customStageMember)
{
  auto lock = std::unique_lock<std::mutex>(this->custom_stage_members_mutex_);
  std::string _id = customStageMember["_id"].get<std::string>();
  std::string stageMemberId =
      customStageMember["stageMemberId"].get<std::string>();
  this->customStageMembers_[_id] = customStageMember;
  this->customStageMemberIdByStageMemberId_[stageMemberId] = _id;
}

void ds::ds_store_t::updateCustomStageMember(const std::string& id,
                                             const nlohmann::json update)
{
  auto lock = std::unique_lock<std::mutex>(this->custom_stage_members_mutex_);
  this->customStageMembers_[id].merge_patch(update);
  // We may implement the change of the stageMemberId, but this should never
  // happen if backend works as expected
}

boost::optional<const ds::json::custom_stage_member_t>
ds::ds_store_t::readCustomStageMember(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->custom_stage_members_mutex_);
  if(!(this->customStageMembers_.count(id) > 0))
    return this->customStageMembers_[id].get<ds::json::custom_stage_member_t>();
  return boost::none;
}

boost::optional<const ds::json::custom_stage_member_t>
ds::ds_store_t::readCustomStageMemberByStageMember(
    const std::string& stageMemberId)
{
  auto lock = std::unique_lock<std::mutex>(this->custom_stage_members_mutex_);
  if(this->customStageMemberIdByStageMemberId_.count(stageMemberId)) {
    return this->readCustomStageMember(
        this->customStageMemberIdByStageMemberId_[stageMemberId]);
  }
  return boost::none;
}

void ds::ds_store_t::removeCustomStageMember(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->custom_stage_members_mutex_);
  const std::string stageMemberId =
      this->customStageMembers_[id]["stageMemberId"];
  this->customStageMembers_.erase(id);
  this->customStageMemberIdByStageMemberId_.erase(stageMemberId);
}

void ds::ds_store_t::removeCustomStageMembers()
{
  auto lock = std::unique_lock<std::mutex>(this->custom_stage_members_mutex_);
  this->customStageMembers_.clear();
  this->customStageMemberIdByStageMemberId_.clear();
}

void ds::ds_store_t::createSoundCard(const nlohmann::json soundCard)
{
  auto lock = std::unique_lock<std::mutex>(this->sound_cards_mutex_);
  const std::string _id = soundCard["_id"].get<std::string>();
  this->soundCards_[_id] = soundCard;
  this->soundCardIdByName_[soundCard["name"]] = _id;
}

void ds::ds_store_t::updateSoundCard(const std::string& id,
                                     const nlohmann::json update)
{
  auto lock = std::unique_lock<std::mutex>(this->sound_cards_mutex_);
  this->soundCards_[id].merge_patch(update);
}

boost::optional<const ds::json::soundcard_t>
ds::ds_store_t::readSoundCard(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->sound_cards_mutex_);
  if(!(this->soundCards_.count(id) > 0))
    return this->soundCards_[id].get<ds::json::soundcard_t>();
  return boost::none;
}

boost::optional<const ds::json::soundcard_t>
ds::ds_store_t::readSoundCardByName(const std::string& name)
{
  auto lock = std::unique_lock<std::mutex>(this->sound_cards_mutex_);
  if(!(this->soundCardIdByName_.count(name) > 0))
    return this->readSoundCard(this->soundCardIdByName_[name]);
  return boost::none;
}

void ds::ds_store_t::removeSoundCard(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->sound_cards_mutex_);
  const std::string soundCardName = this->soundCards_[id]["name"];
  this->soundCards_.erase(id);
  this->soundCardIdByName_.erase(soundCardName);
}

void ds::ds_store_t::removeSoundCards()
{
  auto lock = std::unique_lock<std::mutex>(this->sound_cards_mutex_);
  this->soundCards_.clear();
  this->soundCardIdByName_.clear();
}

void ds::ds_store_t::createOvTrack(const nlohmann::json ovTrack)
{
  auto lock = std::unique_lock<std::mutex>(this->ov_tracks_mutex_);
  const std::string _id = ovTrack["_id"].get<std::string>();
  this->ovTracks_[_id] = ovTrack;
}

void ds::ds_store_t::updateOvTrack(const std::string& id,
                                   const nlohmann::json update)
{
  auto lock = std::unique_lock<std::mutex>(this->ov_tracks_mutex_);
  this->ovTracks_[id].merge_patch(update);
}

boost::optional<const ds::json::ov_track_t>
ds::ds_store_t::readOvTrack(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->ov_tracks_mutex_);
  if(!(this->ovTracks_.count(id) > 0))
    return this->ovTracks_[id].get<ds::json::ov_track_t>();
  return boost::none;
}

void ds::ds_store_t::removeOvTrack(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->ov_tracks_mutex_);
  this->ovTracks_.erase(id);
}

void ds::ds_store_t::removeOvTracks()
{
  auto lock = std::unique_lock<std::mutex>(this->ov_tracks_mutex_);
  this->ovTracks_.clear();
}

void ds::ds_store_t::createRemoteOvTrack(const nlohmann::json remoteOvTrack)
{
  auto lock = std::unique_lock<std::mutex>(this->remote_ov_tracks_mutex_);
  const std::string _id = remoteOvTrack["_id"].get<std::string>();
  this->remoteOvTracks_[_id] = remoteOvTrack;
  this->remoteOvTrackIdsByStageMemberId_[remoteOvTrack["stageMemberId"]]
      .push_back(_id);
}

void ds::ds_store_t::updateRemoteOvTrack(const std::string& id,
                                         const nlohmann::json update)
{
  auto lock = std::unique_lock<std::mutex>(this->remote_ov_tracks_mutex_);
  this->remoteOvTracks_[id].merge_patch(update);
}

boost::optional<const ds::json::remote_ov_track_t>
ds::ds_store_t::readRemoteOvTrack(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->remote_ov_tracks_mutex_);
  if(!(this->remoteOvTracks_.count(id) > 0))
    return this->remoteOvTracks_[id].get<ds::json::remote_ov_track_t>();
  return boost::none;
}

void ds::ds_store_t::removeRemoteOvTrack(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->remote_ov_tracks_mutex_);
  const std::string stageMemberId = this->remoteOvTracks_[id]["stageMemberId"];
  std::remove(this->remoteOvTrackIdsByStageMemberId_[stageMemberId].begin(),
              this->remoteOvTrackIdsByStageMemberId_[stageMemberId].end(), id);
  this->remoteOvTracks_.erase(id);
}

void ds::ds_store_t::removeRemoteOvTracks()
{
  auto lock = std::unique_lock<std::mutex>(this->remote_ov_tracks_mutex_);
  this->remoteOvTracks_.clear();
  this->remoteOvTrackIdsByStageMemberId_.clear();
}

void ds::ds_store_t::createCustomRemoteOvTrack(
    const nlohmann::json customRemoteOvTrack)
{
  auto lock =
      std::unique_lock<std::mutex>(this->custom_remote_ov_tracks_mutex_);
  const std::string _id = customRemoteOvTrack["_id"].get<std::string>();
  this->customRemoteOvTracks_[_id] = customRemoteOvTrack;
  this->customOvTrackIdByOvTrackId_[customRemoteOvTrack["remoteOvTrackId"]] =
      _id;
}

void ds::ds_store_t::updateCustomRemoteOvTrack(const std::string& id,
                                               const nlohmann::json update)
{
  auto lock =
      std::unique_lock<std::mutex>(this->custom_remote_ov_tracks_mutex_);
  this->customRemoteOvTracks_[id].merge_patch(update);
}

boost::optional<const ds::json::custom_remote_ov_track_t>
ds::ds_store_t::readCustomRemoteOvTrack(const std::string& id)
{
  auto lock =
      std::unique_lock<std::mutex>(this->custom_remote_ov_tracks_mutex_);
  if(!(this->customRemoteOvTracks_.count(id) > 0))
    return this->customRemoteOvTracks_[id]
        .get<ds::json::custom_remote_ov_track_t>();
  return boost::none;
}

void ds::ds_store_t::removeCustomRemoteOvTrack(const std::string& id)
{
  auto lock =
      std::unique_lock<std::mutex>(this->custom_remote_ov_tracks_mutex_);
  const std::string remoteOvTrackId =
      this->customRemoteOvTracks_[id]["remoteOvTrackId"];
  this->customRemoteOvTracks_.erase(id);
  this->customOvTrackIdByOvTrackId_[remoteOvTrackId];
}

void ds::ds_store_t::removeCustomRemoteOvTracks()
{
  auto lock =
      std::unique_lock<std::mutex>(this->custom_remote_ov_tracks_mutex_);
  this->customRemoteOvTracks_.clear();
  this->customOvTrackIdByOvTrackId_.clear();
}

const std::vector<ds::json::ov_track_t> ds::ds_store_t::readOvTracks()
{
  auto lock = std::unique_lock<std::mutex>(this->ov_tracks_mutex_);
  std::vector<ds::json::ov_track_t> tracks;
  for(const auto& pair : this->ovTracks_) {
    tracks.push_back(pair.second);
  }
  return tracks;
}

boost::optional<const ds::json::stage_t> ds::ds_store_t::getCurrentStage()
{
  auto lock = std::unique_lock<std::mutex>(this->stages_mutex_);
  const std::string currentStageId = this->getCurrentStageId();
  if(!currentStageId.empty()) {
    return this->readStage(currentStageId);
  }
  return boost::none;
}

boost::optional<const ds::json::stage_member_t>
ds::ds_store_t::getCurrentStageMember()
{
  if(!this->currentStageMemberId_.empty()) {
    return this->readStageMember(this->currentStageMemberId_);
  }
  return boost::none;
}

boost::optional<const ds::json::custom_stage_member_t>
ds::ds_store_t::getCustomStageMemberByStageMemberId(
    const std::string& stageMemberId)
{
  auto lock = std::unique_lock<std::mutex>(this->custom_stage_members_mutex_);
  if(this->customStageMemberIdByStageMemberId_.count(stageMemberId) > 0) {
    return this->readCustomStageMember(
        this->customStageMemberIdByStageMemberId_[stageMemberId]);
  }
  return boost::none;
}

boost::optional<const ds::json::custom_group_t>
ds::ds_store_t::getCustomGroupByGroupId(const std::string& customStageMemberId)
{
  auto lock = std::unique_lock<std::mutex>(this->custom_groups_mutex_);
  if(this->customGroupIdByGroupId_.count(customStageMemberId) > 0) {
    return this->readCustomGroup(
        this->customGroupIdByGroupId_[customStageMemberId]);
  }
  return boost::none;
}

boost::optional<const ds::json::custom_remote_ov_track_t>
ds::ds_store_t::getCustomOvTrackByOvTrackId(const std::string& ovTrackId)
{
  auto lock =
      std::unique_lock<std::mutex>(this->custom_remote_ov_tracks_mutex_);
  if(this->customOvTrackIdByOvTrackId_.count(ovTrackId) > 0) {
    return this->readCustomRemoteOvTrack(
        this->customOvTrackIdByOvTrackId_[ovTrackId]);
  }
  return boost::none;
}

const std::vector<const ds::json::remote_ov_track_t>
ds::ds_store_t::getRemoteOvTracksByStageMemberId(
    const std::string& stageMemberId)
{
  auto lock = std::unique_lock<std::mutex>(this->remote_ov_tracks_mutex_);
  std::vector<const ds::json::remote_ov_track_t> remoteTracks;
  if(this->remoteOvTrackIdsByStageMemberId_.count(stageMemberId) > 0) {
    for(const std::string& remoteOvTrackId :
        this->remoteOvTrackIdsByStageMemberId_[stageMemberId]) {
      const auto remoteOvTrack = this->readRemoteOvTrack(remoteOvTrackId);
      if(remoteOvTrack) {
        remoteTracks.push_back(remoteOvTrack.get());
      }
    }
  }
  return remoteTracks;
}

void ds::ds_store_t::createUser(const nlohmann::json user)
{
  auto lock = std::unique_lock<std::mutex>(this->users_mutex_);
  const std::string _id = user["_id"].get<std::string>();
  this->users_[_id] = user;
}

void ds::ds_store_t::updateUser(const std::string& id,
                                const nlohmann::json update)
{
  auto lock = std::unique_lock<std::mutex>(this->users_mutex_);
  this->users_[id].merge_patch(update);
}

boost::optional<const ds::json::user_t>
ds::ds_store_t::readUser(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->users_mutex_);
  if(!(this->users_.count(id) > 0))
    return this->users_[id].get<ds::json::user_t>();
  return boost::none;
}

void ds::ds_store_t::removeUser(const std::string& id)
{
  auto lock = std::unique_lock<std::mutex>(this->users_mutex_);
  this->users_.erase(id);
}

void ds::ds_store_t::removeUsers()
{
  auto lock = std::unique_lock<std::mutex>(this->users_mutex_);
  this->users_.clear();
}