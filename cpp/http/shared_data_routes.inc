// Included from server.cpp after the room base route is defined.
protect(server,*repo,"GET","/v1/me",[&](auto&,auto& r,const Id& actor){
  output(r,repo->get_profile(actor),[](const User& u)->json{return {{"user",profile(u)}};});
});
protect(server,*repo,"PUT","/v1/me/profile",[&](auto& req,auto& r,const Id& actor){
  auto j=body(req,{"animal_icon"});
  output(r,repo->update_profile(actor,bounded(j,"animal_icon",16)),
    [](const User& u)->json{return {{"user",profile(u)}};});
});
protect(server,*repo,"DELETE","/v1/me",[&](auto&,auto& r,const Id& actor){
  output(r,repo->delete_user(actor,now_ms()),[](Unit)->json{
    return {{"deleted",true},{"storage_cleanup","queued"}};},202);
});
protect(server,*repo,"DELETE",base+"/me/data",[&](auto& req,auto& r,const Id& actor){
  output(r,repo->delete_member_data(actor,normalize_uuid(req.matches[1]),false,now_ms()),
    [](Unit)->json{return {{"deleted",true},{"storage_cleanup","queued"}};},202);
});
protect(server,*repo,"DELETE",base+"/me/route",[&](auto& req,auto& r,const Id& actor){
  output(r,repo->delete_member_data(actor,normalize_uuid(req.matches[1]),true,now_ms()),
    [](Unit)->json{return {{"deleted",true}};});
});
protect(server,*repo,"PUT",base+"/device-status",[&](auto& req,auto& r,const Id& actor){
  auto j=body(req,{"battery_percent","is_charging","sharing_epoch"});
  DeviceStatusDraft draft;
  if(!j.at("battery_percent").is_null()) {
    auto battery=integer<std::int64_t>(j,"battery_percent");
    if(battery<0 || battery>100) throw std::invalid_argument("Invalid battery percentage");
    draft.battery_percent=static_cast<int>(battery);
  }
  if(!j.at("is_charging").is_null()) draft.is_charging=json_boolean(j,"is_charging");
  draft.sharing_epoch=integer<Epoch>(j,"sharing_epoch");
  if(draft.sharing_epoch<1) throw std::invalid_argument("Invalid sharing epoch");
  output(r,repo->update_device_status(actor,normalize_uuid(req.matches[1]),draft,now_ms()),
    [](const DeviceStatus& d)->json{return {{"device_status",device_status(d)}};});
});
protect(server,*repo,"GET",base+"/marks",[&](auto& req,auto& r,const Id& actor){
  output(r,repo->list_marks(actor,normalize_uuid(req.matches[1])),[](const std::vector<MapMark>& marks)->json{
    json values=json::array();for(const auto& mark:marks)values.push_back(map_mark(mark));
    return {{"marks",values}};});
});
protect(server,*repo,"POST",base+"/marks",[&](auto& req,auto& r,const Id& actor){
  auto j=body(req,{"name","latitude","longitude"});
  MapMarkDraft draft{bounded(j,"name",60),{number(j,"latitude"),number(j,"longitude")}};
  if(std::abs(draft.coordinate.latitude)>90 || std::abs(draft.coordinate.longitude)>180)
    throw std::invalid_argument("Invalid coordinates");
  output(r,repo->create_mark(actor,normalize_uuid(req.matches[1]),draft,now_ms()),
    [](const MapMark& mark)->json{return {{"mark",map_mark(mark)}};},201);
});
const auto mark_path=base+R"(/marks/([0-9a-fA-F-]{36}))";
protect(server,*repo,"PUT",mark_path,[&](auto& req,auto& r,const Id& actor){
  auto j=body(req,{"name"});
  output(r,repo->rename_mark(actor,normalize_uuid(req.matches[1]),normalize_uuid(req.matches[2]),
    bounded(j,"name",60),now_ms()),[](const MapMark& mark)->json{return {{"mark",map_mark(mark)}};});
});
protect(server,*repo,"DELETE",mark_path,[&](auto& req,auto& r,const Id& actor){
  output(r,repo->delete_mark(actor,normalize_uuid(req.matches[1]),normalize_uuid(req.matches[2])),
    [](Unit)->json{return json::object();});
});
auto route_handler=[&](const httplib::Request& req,httplib::Response& r,const Id& actor){
  for(const auto& [key,value]:req.params)
    if(key!="after_id" && key!="limit") throw std::invalid_argument("Unknown query parameter");
  const auto after=query_integer(req,"after_id",0),limit=query_integer(req,"limit",200);
  if(limit<1 || limit>500) throw std::invalid_argument("Invalid route limit");
  const auto member_id=req.matches.size()>2?normalize_uuid(req.matches[2]):Id{};
  output(r,repo->get_route(actor,normalize_uuid(req.matches[1]),member_id,after,static_cast<int>(limit),now_ms()),
    [](const RoutePage& page)->json{
      json points=json::array();
      for(const auto& point:page.points) {
        auto value=location(point.location);value["point_id"]=point.id;value["member_id"]=point.member_id;
        points.push_back(std::move(value));
      }
      return {{"points",points},{"next_after_id",page.next_after_id}};
    });
};
protect(server,*repo,"GET",base+"/me/route",route_handler);
protect(server,*repo,"GET",base+R"(/members/([0-9a-fA-F-]{36})/route)",route_handler);
